**ESP32 brain CD3: fix WiFi AP scan decode desync** — `m1_esp32_rpc_wifi_scan()`
  read the M1_RPC `WIFI_SCAN` response's AP count as a single byte and started
  parsing entries at offset 1, but the brain firmware (`m1-esp32-brain`)
  encodes the count as 2 little-endian bytes (matching `STA_SCAN_RESULTS`).
  This shifted every scan entry by one byte, corrupting BSSID/RSSI/channel/
  SSID for every result and causing "AP Scan failed please try again", "No
  APs found" (2.4G Survey), and (by extension, since they build on the AP
  list) "No Stations Found" / "No Targets Found" against the native brain CD3
  firmware. Fixed to read the 2-byte count and parse entries from offset 2.
