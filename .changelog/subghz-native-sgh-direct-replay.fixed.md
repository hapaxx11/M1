**Sub-GHz: M1 native .sgh NOISE files now replay directly without conversion** — C3.12
  and SiN360 `.sgh` recordings previously failed with "Memory error" (error 5) when
  Emulate was chosen in Saved, because they were routed through the Flipper `.sub`
  converter which wrote a temp file and parsed it via `sub_ghz_raw_samples_init()`.
  M1 native NOISE files are now detected via `is_m1_native` (set during file load)
  and replayed directly with `sub_ghz_replay_datafile()`, bypassing the conversion
  entirely. The same direct-replay dispatch is applied in the Playlist scene via the
  new `flipper_subghz_probe()` header probe. The `sub_ghz_raw_samples_init()` CRLF
  assumption (`strlen(token) + 2`) is also fixed to scan past actual line terminators,
  so LF-only files from Linux-origin firmware builds stream correctly.
