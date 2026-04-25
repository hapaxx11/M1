**Web Updater: CORS proxy fallback** — firmware downloads now try `corsproxy.io` first and
  fall back to `proxy.corsfix.com` if the primary proxy returns an error, fixing the 403
  failure seen when downloading release assets.
