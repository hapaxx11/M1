**ESP32: AT firmware fallback profiles** — Added `M1_ESP32_CAP_PROFILE_AT_BEDGE117`
  and `M1_ESP32_CAP_PROFILE_AT_NEDDY299` capability profiles for AT firmware variants.
  CMD_GET_STATUS failure now falls back to the AT baseline profile (`BLE_HID` + `802154`)
  instead of the SiN360 profile, enabling Bad-BT on bedge117/neddy299/dag firmware without
  `feat/cmd_get_status` support. `DELEGATE_CAPPED` in WiFi and Bluetooth scenes now
  pre-initialises the ESP32 transport before the capability check so the query can succeed
  regardless of which delegate ran previously. Documented dag
  (`dagnazty/esp32-at-monstatek-m1`) as a supported AT firmware variant.
