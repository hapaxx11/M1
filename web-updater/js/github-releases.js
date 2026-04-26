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
 * CORS proxies for GitHub release asset downloads — last-resort fallback only.
 *
 * The primary download path uses the GitHub REST API asset endpoint:
 *   GET https://api.github.com/repos/{owner}/{repo}/releases/assets/{id}
 *   Accept: application/octet-stream
 *
 * api.github.com always sends proper Access-Control-Allow-Origin headers, and
 * the CDN redirect target (objects.githubusercontent.com) does too, so no
 * proxy is needed when the download URL is an api.github.com URL.
 *
 * These proxies are kept as a fallback only for the case where a raw
 * browser_download_url (github.com / objects.githubusercontent.com) is passed
 * directly.  They are tried in priority order; the first proxy that returns a
 * successful response wins.
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
    'github.com',
]);

/**
 * Return true when the URL targets a host that requires a CORS proxy.
 * API calls (api.github.com) are not proxied — they already have CORS headers.
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

            // Use the GitHub API asset endpoint for downloads — it has proper
            // CORS headers (Access-Control-Allow-Origin: *) so no proxy is needed.
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
 * fetched directly with Accept: application/octet-stream.  api.github.com
 * responds with proper CORS headers on the 302 redirect, and the CDN target
 * (objects.githubusercontent.com) also sends Access-Control-Allow-Origin: *,
 * so the browser can read the binary without any proxy.
 *
 * When the URL targets github.com or objects.githubusercontent.com directly
 * (e.g. a browser_download_url passed externally), each proxy in CORS_PROXIES
 * is tried in order.  The first proxy that returns a successful response wins.
 * If every proxy returns an error, the last error is thrown.
 *
 * @param {string}   url          - Download URL (API asset URL or direct CDN URL)
 * @param {function} [onProgress] - Callback(loaded, total) for progress
 * @returns {Promise<Uint8Array>} Firmware binary data
 */
export async function downloadFirmware(url, onProgress = null) {
    let resp;

    if (url.startsWith(`${GITHUB_API}/`)) {
        // Primary path: GitHub REST API asset endpoint — no proxy needed.
        // api.github.com sends Access-Control-Allow-Origin on both the 302
        // redirect response and the CDN destination.
        resp = await fetch(url, {
            headers: {
                'Accept': 'application/octet-stream',
                'X-GitHub-Api-Version': '2022-11-28',
            },
        });
        if (!resp.ok) {
            throw new Error(`Download failed: ${resp.status} ${resp.statusText}`);
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
