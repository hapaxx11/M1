**Sub-GHz: fix reboot when saving .sgh/.sub file after decoding a signal** — the old save
  path allocated a 16 KB `flipper_subghz_signal_t` on the task stack, overflowing the 16 KB
  `subfunc_handler_task` stack.  The scene now calls the lightweight
  `flipper_subghz_save_m1native_key()` / `flipper_subghz_save_key()` helpers (~1 KB on stack)
  and ensures the `/SUBGHZ` directory is created before opening the file, preventing a silent
  failure on a fresh SD card.  RAW recordings now also write the correct Flipper-compatible
  filetype header (`Flipper SubGhz RAW File`) instead of the non-standard `SubGhz RAW File`.
