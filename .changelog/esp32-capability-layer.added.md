**ESP32 runtime capability layer** (`m1_esp32_caps.h/c`) — replaces compile-time
  `#ifdef` gating with a runtime command-capability bitmap queried via
  `CMD_GET_STATUS` at first use.  Each `M1_ESP32_CMD_*` bit corresponds to a
  specific command family the ESP32 supports.  Falls back to a minimal
  `M1_ESP32_CMD_PROFILE_AT_FALLBACK` bitmap for older AT firmware that does not
  implement the status command, preserving Bad-BT access.  Call sites in WiFi /
  Bluetooth scenes now call `m1_esp32_require_cap()` which draws a standard
  "not supported" screen naming the active ESP32 firmware instead of hardcoded
  strings.  The Settings / qMonstatek device-info `esp32_version` field is now
  populated from the runtime handshake result.
