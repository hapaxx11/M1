**ESP32 capability layer: single `cap_bitmap` `CMD_GET_STATUS` protocol** — the
  `CMD_GET_STATUS` payload is now 41 bytes carrying a single `cap_bitmap` (uint64_t)
  that contains `M1_ESP32_CAP_*` bits directly.  Any firmware variant — binary-SPI
  or AT text commands — sets the same bit for the same capability; there is no
  derivation step on the STM32 side and no possibility of two different bit positions
  meaning the same feature.  The `bss_bytes` / `free_heap_bytes` fields and their
  accessor functions (`m1_esp32_caps_bss_bytes`, `m1_esp32_caps_free_heap`) are
  removed — they were not present in any released firmware.  The protocol is
  documented in `documentation/esp32_firmware.md` and `m1_esp32_caps.h`.
