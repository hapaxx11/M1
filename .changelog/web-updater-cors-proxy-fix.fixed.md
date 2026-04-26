**Web Updater: firmware download no longer relies on third-party CORS proxies** — asset
  downloads now use the GitHub REST API asset endpoint
  (`api.github.com/repos/.../releases/assets/{id}` with `Accept: application/octet-stream`)
  which carries its own `Access-Control-Allow-Origin` header, eliminating the dependency on
  allorigins.win / corsproxy.io / corsfix.com proxies that were blocking or failing.
  The broken `allorigins.win` entry has been removed from the proxy fallback list and the
  `corsproxy.io` URL format has been corrected.
