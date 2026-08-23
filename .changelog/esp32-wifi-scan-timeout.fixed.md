**ESP32: WiFi Scan RPC timeout too short for a real scan (issue #719 Phase 5)** —
  Field read-back on Settings > Dashboard > page 5/5 confirmed WiFi Scan was
  failing with `"op0103 no-reply st253 r0 p0"`: the brain's WIFI_SCAN
  request/response replies only once the *entire* channel sweep completes
  (unlike STA_SCAN/BLE_SCAN's quick start-then-poll pattern), so the shared
  2 s timeout used for prompt control commands was too short and the M1 Link
  transport gave up before a real scan finished. `m1_esp32_rpc_wifi_scan()`
  now uses a dedicated, longer `M1_ESP32_RPC_WIFI_SCAN_TIMEOUT_S` (10 s).
