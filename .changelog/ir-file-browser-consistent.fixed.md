**Infrared: fix "Learned" and Universal Remote showing file-not-found with no browse option**
  — `browse_directory()` dead-ended with no navigation when the target folder was empty or
  the user had not yet copied IR files to the SD card.  Replaced with `ir_browse_with_fb()`,
  a wrapper around the stock Monstatek `m1_file_browser`, which always shows a ".." entry
  so the user can navigate up to `0:/IR/` or the SD card root to find files elsewhere.
  — `Infrared > Universal Remote > Browse IRDB` and `> Learned` both now use the consistent
  `m1_file_browser` approach (matching the Sub-GHz, BadBT, and other module patterns).
  — `Infrared > Replay` continues to open the Learned directory directly but now uses the
  same `m1_file_browser` approach, eliminating the dead-end when no learned files exist yet.
