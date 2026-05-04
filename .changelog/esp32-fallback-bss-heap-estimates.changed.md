**ESP32 capability layer: fallback BSS/heap estimates** — the compile-flag fallback path
  (used when firmware does not yet implement `CMD_GET_STATUS`) now populates `bss_bytes`
  and `free_heap_bytes` with best-guess estimates derived from source analysis of known
  Hapax-fork ESP32 firmware releases: SiN360 ≈ 200 KB BSS / 160 KB heap;
  AT/C3 ≈ 284 KB BSS / 112 KB heap. Live measurements via `CMD_GET_STATUS` still
  take priority whenever compatible firmware is connected.
