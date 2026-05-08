**ESP32 capability layer: per-command `M1_ESP32_CMD_*` bitmap** — replaced the
  high-level `M1_ESP32_CAP_*` feature-group constants with fine-grained
  `M1_ESP32_CMD_*` bits, one per command family from `m1_esp32_cmd.h`.  Call
  sites now specify exactly which commands they need (`M1_ESP32_CMD_WIFI_JOIN`,
  `M1_ESP32_CMD_AT_BLE_HID`, `M1_ESP32_CMD_AT_802154`, etc.) rather than
  aggregate labels like `WIFI_ATTACK` or `BLE_SPAM`.  The single-bitmap
  `CMD_GET_STATUS` protocol (41-byte payload, `uint64_t cap_bitmap`) and the
  AT secondary probe (`AT+GETSTATUSHEX`) remain unchanged.  Firmware that
  implements `CMD_GET_STATUS` sets the same `M1_ESP32_CMD_*` bits regardless of
  whether it uses binary SPI commands or AT text commands internally.
