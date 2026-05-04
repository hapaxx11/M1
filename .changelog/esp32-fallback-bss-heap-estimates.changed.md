**ESP32 capability layer: memory footprint estimates** — `bss_bytes` and `free_heap_bytes`
  are now maintained as internal developer constants (`M1_ESP32_FALLBACK_*` in
  `m1_csrc/m1_esp32_caps.h`) rather than wire-protocol fields.  They are always populated
  from compile-time estimates based on source analysis of the known Hapax-fork ESP32
  firmware releases (SiN360 ≈ 200 KB BSS / 160 KB heap; AT/C3 ≈ 284 KB BSS / 112 KB
  heap) and are accessible via `m1_esp32_caps_bss_bytes()` / `m1_esp32_caps_free_heap()`
  for OOM diagnostics.  The `CMD_GET_STATUS` wire payload is now 37 bytes (down from 45).
