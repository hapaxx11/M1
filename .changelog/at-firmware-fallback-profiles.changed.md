**ESP32: AT firmware fallback** — CMD_GET_STATUS failure now falls back to
  `M1_ESP32_CMD_PROFILE_AT_FALLBACK` (BLE HID + 802.15.4) instead of the SiN360
  profile, enabling Bad-BT on bedge117/neddy299/dag firmware without
  `feat/cmd_get_status` support. `DELEGATE_CAPPED` in WiFi and Bluetooth scenes
  now pre-initialises the ESP32 transport before the capability check so the query
  can succeed regardless of which delegate ran previously. Documented dag
  (`dagnazty/esp32-at-monstatek-m1`) as a supported AT firmware variant.
