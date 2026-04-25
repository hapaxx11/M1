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
 * The GitHub API (api.github.com) responds with proper CORS headers and is
 * fetched directly.  However, `browser_download_url` points to either
 * github.com or objects.githubusercontent.com — both of which redirect to a
 * CDN that does NOT send Access-Control-Allow-Origin headers.  Any direct
 * browser fetch of these URLs is blocked by CORS policy.
 *
 * Routing asset downloads through a CORS-capable proxy adds the required
 * headers so the browser can receive the binary data.
 *
 * Multiple proxies are listed in priority order.  If a proxy returns any
 * non-2xx response or the fetch itself rejects (network error), the download
 * retries with the next proxy in the list.
 *
 * Each entry:
 *   prefix — URL prefix to prepend (the target URL follows immediately)
 *   encode — true: target URL is percent-encoded before appending
 *            false: target URL is appended raw
 */
const CORS_PROXIES = [
    { prefix: 'https://api.allorigins.win/raw?url=', encode: true  },
    { prefix: 'https://corsproxy.io/?url=',          encode: true  },
    { prefix: 'https://proxy.corsfix.com/?',         encode: false },
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
                    downloadUrl: fwAsset.browser_download_url,
                } : null,
                rawAsset: rawAsset ? {
                    name: rawAsset.name,
                    size: rawAsset.size,
                    downloadUrl: rawAsset.browser_download_url,
                } : null,
                sdAsset: sdAsset ? {
                    name: sdAsset.name,
                    size: sdAsset.size,
                    downloadUrl: sdAsset.browser_download_url,
                } : null,
            };
        })
        .filter(r => r.fwAsset !== null);  // Only show releases with firmware
}

/**
 * Download a firmware binary from a release asset URL.
 *
 * When the URL requires a CORS proxy, each proxy in CORS_PROXIES is tried in
 * order.  The first proxy that returns a successful response wins.  If every
 * proxy returns an error, the last error is thrown.
 *
 * @param {string}   url          - Direct download URL
 * @param {function} [onProgress] - Callback(loaded, total) for progress
 * @returns {Promise<Uint8Array>} Firmware binary data
 */
export async function downloadFirmware(url, onProgress = null) {
    let resp;
    if (needsProxy(url)) {
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
