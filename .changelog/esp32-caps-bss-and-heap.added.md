**ESP32 capability layer: developer-only memory footprint accessors** —
  `m1_esp32_caps_bss_bytes()` and `m1_esp32_caps_free_heap()` return compile-time
  BSS and free-heap estimates (`M1_ESP32_FALLBACK_*` constants) for OOM diagnostics
  and buffer-sizing decisions.  Profile is selected at runtime from the resolved
  capability bitmap (`M1_ESP32_CAP_WIFI_JOIN` present → AT/C3; absent → SiN360).
  These fields are **not** carried over the wire — `CMD_GET_STATUS` payload remains
  41 bytes (`proto_ver` + `cap_bitmap` uint64_t + `fw_name[32]`).
