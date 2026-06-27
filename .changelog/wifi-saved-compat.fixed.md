**WiFi: Saved Networks compatibility layer** — Saved Networks, Status, and
  Disconnect now work on all ESP32 firmware variants (SiN360 binary SPI and
  AT-based: dag T-800, bedge117).  Previously these screens were gated on
  `ESP32_FEATURE_WIFI_JOIN` (AT-firmware only), so SiN360 users saw "Not
  Supported" even though their credentials were already stored on the SD card.
  `wifi_connect_from_saved()` now dispatches to `AT+CWJAP` on AT firmware and
  `CMD_WIFI_JOIN` on SiN360, mirroring the Scan & Connect path.
  `wifi_disconnect()` likewise dispatches to `AT+CWQAP` or `CMD_WIFI_DISCONNECT`.
  Credentials remain stored in `0:/System/wifi_creds.bin` (AES-256-CBC
  encrypted) and are portable across all firmware variants.
