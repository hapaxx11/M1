**Sub-GHz Saved: fixed "Send failed / Memory error" when emulating any .sub file** —
  After the SiN360 binary-SPI integration enlarged firmware BSS, the newlib heap
  became too small to reliably satisfy the Emulate path's back-buffer (up to
  128 KB) and SD scratch-buffer (4 KB) allocations, even immediately after a
  reboot. `sub_ghz_replay_start_async()` returned 5 → Transmitter rendered
  "Memory error", TIM1+DMA was never armed, the M1 LED never blinked and
  Flipper detected nothing on air. Mirrors the v0.9.1.14 fix applied to
  `sub_ghz_ring_buffers_init()` for Read Raw — `sub_ghz_raw_samples_init()` /
  `sub_ghz_raw_samples_deinit()` now allocate from the FreeRTOS heap
  (`pvPortMalloc` / `vPortFree`, ≥180 KB free) instead of the newlib heap.
  Affects both the legacy blocking replay (`sub_ghz_replay_datafile`) and the
  async Read Raw send path that share the same back buffer.
