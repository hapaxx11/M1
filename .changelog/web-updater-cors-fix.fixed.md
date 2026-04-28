**Web Updater: fix firmware download blocked by CORS policy** — GitHub's CDN changed
  from `objects.githubusercontent.com` to `release-assets.githubusercontent.com`; the new
  CDN does not include `Access-Control-Allow-Origin` headers. The web updater now catches
  the resulting CORS block and automatically retries the download through its CORS proxy
  list (`corsproxy.io` → `proxy.corsfix.com`).
