- **Sub-GHz: static-code "Send" no longer hangs on the Sending overlay** — Sending
  a captured static-code signal (e.g. a Princeton remote) from the Receiver-Info
  detail view sat on the "Sending…" screen for a fixed ~2 s and only ever emitted
  a single frame.  The completion wait polled `subghz_decenc_ctl.ntx_raw_repeat`,
  a counter that is only decremented by the file-based replay engine and never by
  this direct-buffer path, so it always ran its full safety timeout.  The send now
  emits the intended repeats (1 + `SUBGHZ_TX_RAW_REPLAY_REPEAT_DEFAULT`) and waits
  on the real per-burst DMA transfer-complete (bounded by a per-burst safety cap),
  finishing as soon as the frames are on the air.  The burst-count and per-burst
  wait policy is extracted into host-tested pure logic (`Sub_Ghz/subghz_static_tx.c`).
