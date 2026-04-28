/**
 * github-releases.js
 *
 * Fetches firmware releases from the GitHub Releases API.
 * No API key required for public repositories (rate-limited to 60 req/hr).
 *
 * @license See COPYING.txt for license details.
 */

const GITHUB_API = 'https://api.github.com';

/*
 * CORS proxies for GitHub release asset downloads.
 *
 * The primary download path uses the GitHub REST API asset endpoint:
 *   GET https://api.github.com/repos/{owner}/{repo}/releases/assets/{id}
 *   Accept: application/octet-stream
 *
 * api.github.com responds with a 302 redirect to a GitHub CDN host.
 * The CDN historically was objects.githubusercontent.com (CORS-safe), but
 * GitHub now redirects to release-assets.githubusercontent.com which does NOT
 * include Access-Control-Allow-Origin headers, causing browsers to block the
 * response.  downloadFirmware() therefore wraps the direct api.github.com
 * fetch in a try/catch and falls back to these proxies on any failure.
 *
 * They are also used for any raw browser_download_url
 * (github.com / objects.githubusercontent.com / release-assets.githubusercontent.com)
 * passed directly.  They are tried in priority order; the first proxy that
 * returns a successful response wins.
 *
 * Each entry:
 *   prefix — URL prefix to prepend (the target URL follows immediately)
 *   encode — true: target URL is percent-encoded before appending
 *            false: target URL is appended raw
 */
const CORS_PROXIES = [
    { prefix: 'https://corsproxy.io/?',      encode: true  },
    { prefix: 'https://proxy.corsfix.com/?', encode: false },
];

/** Hostnames whose fetch responses are blocked by CORS policy. */
const CORS_BLOCKED_HOSTS = new Set([
    'objects.githubusercontent.com',
    'release-assets.githubusercontent.com',
    'github.com',
]);

/**
 * Return true when the URL targets a host that requires a CORS proxy.
 * api.github.com URLs are handled by downloadFirmware() separately — they
 * attempt a direct fetch first and fall back to a proxy if the CDN redirect
 * is blocked; they do not go through this function.
 *
 * @param {string} url
 * @returns {boolean}
 */
function needsProxy(url) {
    try {
        const { hostname } = new URL(url);
        return CORS_BLOCKED_HOSTS.has(hostname);
    } catch (_) {
        return false;
    }
}

/**
 * Fetch available firmware releases from a GitHub repository.
 *
 * @param {string} owner - Repository owner (e.g. 'hapaxx11')
 * @param {string} repo  - Repository name  (e.g. 'M1')
 * @param {object} [options]
 * @param {boolean} [options.includePrerelease=true] - Include pre-releases
 * @param {number}  [options.perPage=20] - Max releases to fetch
 * @returns {Promise<Array>} Array of release objects with firmware assets
 */
export async function fetchReleases(owner, repo, options = {}) {
    const includePrerelease = options.includePrerelease !== false;
    const perPage = options.perPage || 20;

    const url = `${GITHUB_API}/repos/${owner}/${repo}/releases?per_page=${perPage}`;
    const resp = await fetch(url, {
        headers: { 'Accept': 'application/vnd.github.v3+json' },
    });

    if (!resp.ok) {
        throw new Error(`GitHub API error: ${resp.status} ${resp.statusText}`);
    }

    const releases = await resp.json();

    return releases
        .filter(r => includePrerelease || !r.prerelease)
        .map(r => {
            // Find the _wCRC.bin asset (the update image)
            const fwAsset = r.assets.find(a => a.name.endsWith('_wCRC.bin'));
            // Also find the plain .bin (no _wCRC suffix)
            const rawAsset = r.assets.find(a =>
                a.name.endsWith('.bin') && !a.name.endsWith('_wCRC.bin')
            );
            // Find SD card assets archive
            const sdAsset = r.assets.find(a => a.name === 'SD_Assets.zip');

            // Use the GitHub API asset endpoint for downloads.
            // downloadFirmware() will try direct first and fall back to a
            // CORS proxy if GitHub's CDN redirect target lacks CORS headers.
            const apiAssetUrl = (id) =>
                `${GITHUB_API}/repos/${owner}/${repo}/releases/assets/${id}`;

            return {
                id: r.id,
                tagName: r.tag_name,
                name: r.name || r.tag_name,
                prerelease: r.prerelease,
                publishedAt: r.published_at,
                body: r.body,
                fwAsset: fwAsset ? {
                    name: fwAsset.name,
                    size: fwAsset.size,
                    downloadUrl: apiAssetUrl(fwAsset.id),
                } : null,
                rawAsset: rawAsset ? {
                    name: rawAsset.name,
                    size: rawAsset.size,
                    downloadUrl: apiAssetUrl(rawAsset.id),
                } : null,
                sdAsset: sdAsset ? {
                    name: sdAsset.name,
                    size: sdAsset.size,
                    downloadUrl: apiAssetUrl(sdAsset.id),
                } : null,
            };
        })
        .filter(r => r.fwAsset !== null);  // Only show releases with firmware
}

/**
 * Download a firmware binary from a release asset URL.
 *
 * When the URL is a GitHub REST API asset endpoint (api.github.com), it is
 * fetched directly first with Accept: application/octet-stream.  GitHub's CDN
 * historically redirected to objects.githubusercontent.com (CORS-safe), but
 * now redirects to release-assets.githubusercontent.com which does NOT include
 * Access-Control-Allow-Origin headers.  If the direct fetch fails for any
 * reason (including a CORS block, which the browser surfaces as a TypeError),
 * each proxy in CORS_PROXIES is tried in order.
 *
 * When the URL targets github.com, objects.githubusercontent.com, or
 * release-assets.githubusercontent.com directly (e.g. a browser_download_url
 * passed externally), the proxy list is tried immediately without a direct
 * attempt.
 *
 * The first proxy that returns a successful response wins.  If every proxy
 * also fails, the last error is thrown.
 *
 * @param {string}   url          - Download URL (API asset URL or direct CDN URL)
 * @param {function} [onProgress] - Callback(loaded, total) for progress
 * @returns {Promise<Uint8Array>} Firmware binary data
 */
export async function downloadFirmware(url, onProgress = null) {
    let resp;

    if (url.startsWith(`${GITHUB_API}/`)) {
        // Primary path: GitHub REST API asset endpoint.
        // api.github.com responds with a 302 redirect to a GitHub CDN host.
        // The CDN historically was objects.githubusercontent.com (CORS-safe),
        // but GitHub now redirects to release-assets.githubusercontent.com
        // which does NOT include Access-Control-Allow-Origin headers.
        // A browser CORS block surfaces as a TypeError ("Failed to fetch"),
        // NOT as an HTTP error code.  We therefore catch TypeError specifically
        // and fall back to the CORS proxy list only in that case.
        // Real HTTP errors (4xx/5xx) are thrown immediately — a proxy cannot
        // fix an authentication or authorization failure from GitHub.
        const apiHeaders = {
            'Accept': 'application/octet-stream',
            'X-GitHub-Api-Version': '2022-11-28',
        };
        let directErr;
        let isCorsFailure = false;
        try {
            resp = await fetch(url, { headers: apiHeaders });
            if (!resp.ok) {
                // Real HTTP error — surface it immediately; no proxy retry.
                throw new Error(`${resp.status} ${resp.statusText}`);
            }
        } catch (err) {
            // TypeError = network/CORS failure (browser blocked CDN redirect).
            // Any other Error type = the HTTP error we threw above.
            isCorsFailure = err instanceof TypeError;
            directErr = err;
            resp = null;
        }

        if (!resp) {
            if (!isCorsFailure) {
                throw new Error(`Download failed: ${directErr?.message ?? 'unknown error'}`);
            }
            // CORS/network failure — try each proxy in turn.
            // Include the same headers for consistency with the direct request.
            let lastError = directErr;
            for (const { prefix, encode } of CORS_PROXIES) {
                const proxyUrl = prefix + (encode ? encodeURIComponent(url) : url);
                try {
                    resp = await fetch(proxyUrl, { headers: apiHeaders });
                    if (resp.ok) { lastError = null; break; }
                    lastError = new Error(`${resp.status} ${resp.statusText}`);
                    resp.body?.cancel();
                    resp = null;
                } catch (err) {
                    lastError = err;
                    resp = null;
                }
            }
            if (!resp) {
                throw new Error(`Download failed: ${lastError?.message ?? 'all proxies exhausted'}`);
            }
        }
    } else if (needsProxy(url)) {
        let lastError;
        for (const { prefix, encode } of CORS_PROXIES) {
            const proxyUrl = prefix + (encode ? encodeURIComponent(url) : url);
            try {
                resp = await fetch(proxyUrl);
                if (resp.ok) break;
                lastError = new Error(`Download failed: ${resp.status} ${resp.statusText}`);
                // Cancel the body to free the connection before trying the next proxy.
                resp.body?.cancel();
            } catch (err) {
                lastError = err;
            }
        }
        if (!resp || !resp.ok) {
            throw lastError || new Error('Download failed: all proxies exhausted');
        }
    } else {
        resp = await fetch(url);
        if (!resp.ok) {
            throw new Error(`Download failed: ${resp.status} ${resp.statusText}`);
        }
    }

    const contentLength = parseInt(resp.headers.get('Content-Length') || '0', 10);
    const reader = resp.body.getReader();
    const chunks = [];
    let loaded = 0;

    while (true) {
        const { done, value } = await reader.read();
        if (done) break;
        chunks.push(value);
        loaded += value.length;
        if (onProgress) {
            onProgress(loaded, contentLength);
        }
    }

    // Concatenate chunks into a single Uint8Array
    const result = new Uint8Array(loaded);
    let offset = 0;
    for (const chunk of chunks) {
        result.set(chunk, offset);
        offset += chunk.length;
    }

    return result;
}

/**
 * Download the SD card assets archive (SD_Assets.zip) from a release asset URL.
 *
 * @param {string}   url          - Direct download URL
 * @param {function} [onProgress] - Callback(loaded, total) for progress
 * @returns {Promise<Uint8Array>} ZIP archive data
 */
export async function downloadSdAssets(url, onProgress = null) {
    return downloadFirmware(url, onProgress);
}
