**ESP32 brain: WiFi/BLE/STA scan and Zigbee sniff "AP scan failed" fixed** — the
  M1_RPC response buffer was hardcoded to 256 bytes, but the brain firmware
  returns bulk scan/sniff lists (up to ~1.8 KB) as a single response. Any real
  scan with more than a handful of results overflowed the buffer and failed
  with "AP scan failed. Please try again." Response buffers for
  `m1_esp32_rpc_wifi_scan()`, `m1_esp32_rpc_sta_scan_results()`,
  `m1_esp32_rpc_ble_scan_results()`, `m1_esp32_rpc_zb_sniff_get()`, and the
  BLE scan path in `m1_bt.c` are now heap-allocated large enough to hold the
  firmware's largest documented bulk response.
