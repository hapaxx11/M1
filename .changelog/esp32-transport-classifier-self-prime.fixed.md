**ESP32: fix "Last feature RPC: no call yet" on the first WiFi Scan (issue #719)**
  — `m1_esp32_active_transport()` only read the *cached* capability bitmap and
  never re-probed, so before `m1_esp32_caps_init()` had run at least once in a
  session it read back an all-zero bitmap and misclassified a brain-CD3 device
  as `ESP32_TRANSPORT_NONE`. The un-gated WiFi Scan / "Scan & Connect" entry is
  typically the very first ESP32 feature a user tries, so its scan silently
  fell through to the legacy binary-SPI scan path instead of
  `m1_esp32_rpc_wifi_scan()` — `m1_esp32_rpc_call()` was never invoked, and
  Settings > Dashboard page 5/5 kept reading "Last feature RPC: no call yet"
  even after a scan was attempted. `m1_esp32_active_transport()` now
  self-primes exactly like `m1_esp32_has_cap()`: it runs `m1_esp32_caps_init()`
  first (only once the ESP32 HAL transport is up) whenever the bitmap has not
  yet been queried.
