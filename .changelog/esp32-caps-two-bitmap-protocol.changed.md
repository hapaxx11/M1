**ESP32 capability layer: split two-bitmap `CMD_GET_STATUS` protocol** — the
  `CMD_GET_STATUS` payload is now 45 bytes carrying two separate bitmaps:
  `at_cmd_bitmap` (uint64_t) for individual standard AT commands and `ext_bitmap`
  (uint32_t) for binary-SPI-only extensions.  The host-side `m1_esp32_caps_derive()`
  maps these wire-format bits to the existing `M1_ESP32_CAP_*` constants, keeping
  all call sites unchanged.  The `bss_bytes` / `free_heap_bytes` fields and their
  accessor functions (`m1_esp32_caps_bss_bytes`, `m1_esp32_caps_free_heap`) are
  removed — they were not present in any released firmware.  Wire-format bit
  definitions (`M1_AT_CMD_*`, `M1_EXT_CMD_*`) and per-firmware profile macros are
  now documented in `m1_esp32_caps.h`, mirroring the ESP32 firmware's `m1_protocol.h`.
