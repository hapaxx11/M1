**ESP32 runtime capability layer** (`m1_esp32_caps.h/c`) — replaces compile-time
  `#ifdef` gating with a runtime capability bitmap queried via `CMD_GET_STATUS` at
  first use. Falls back to a compile-flag-derived bitmap for older ESP32 firmware
  that does not implement the status command, preserving current behaviour exactly.
  Call sites in WiFi / Bluetooth scenes now call `m1_esp32_require_cap()` which draws
  a standard "not supported" screen naming the active ESP32 firmware instead of
  hardcoded "Not available with SiN360 FW" strings. The Settings / qMonstatek device-info
  `esp32_version` field is now populated from the runtime handshake result.
