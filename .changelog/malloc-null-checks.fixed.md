**Memory safety: NULL-check all malloc/calloc/realloc calls** — Audited every
  heap allocation in `m1_csrc/`. Added NULL checks and safe-realloc patterns
  (temp-pointer + fallback) to 10 previously unchecked call sites across
  `m1_file_browser.c`, `m1_badbt.c`, `music_player.c`, and `m1_fw_update.c`.
  Prevents HardFault on out-of-memory conditions.
