**OTA downloads: support native CD3 brain firmware** — Release listings and
  firmware downloads now use streaming M1_RPC HTTP requests on CD3 instead of
  sending unsupported ESP-AT commands that timed out and were misreported as
  DNS failures. HTTPS redirects and chunked responses are handled by the
  ESP32, downloads stream directly to SD in bounded chunks, and older brain
  firmware reports that an update is needed rather than failing as DNS. The
  CD3 source also accepts both factory-image names used by GitHub releases,
  including on devices with an existing `fw_sources.txt`.
