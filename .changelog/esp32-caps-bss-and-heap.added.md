**ESP32 capability layer: `CMD_GET_STATUS` payload extended with `bss_bytes` and
  `free_heap_bytes`** — the 45-byte `m1_esp32_status_payload_t` (was 37 bytes) now
  carries the ESP32 firmware's static BSS segment size and runtime free heap.  Custom
  ESP32 firmware should populate `bss_bytes` from linker symbols
  (`(&__bss_end - &__bss_start) * sizeof(int)`) and `free_heap_bytes` from
  `esp_get_free_heap_size()`.  Both fields accept `0` when unavailable.  No
  existing firmware implements `CMD_GET_STATUS` yet so there is no backwards-
  compatibility break.  New API: `m1_esp32_caps_bss_bytes()` and
  `m1_esp32_caps_free_heap()`.
