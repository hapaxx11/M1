**Web Updater: CORS proxy fix (invalid_origin)** — replaced the `corsproxy.io` + `proxy.corsfix.com`
  proxy pair (both blocked or rejecting requests) with `api.allorigins.win` (primary, percent-encoded
  URL format) → `corsproxy.io` (secondary, percent-encoded) → `proxy.corsfix.com` (last-resort, raw
  URL); also corrected the `corsproxy.io` URL format to use the required `?url=ENCODED` query
  parameter that was missing from the previous fix.
