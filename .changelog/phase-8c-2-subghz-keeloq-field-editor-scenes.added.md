**Sub-GHz: Create-from-scratch KeeLoq field editors** — Adds four new
  scenes (`SubGhzSceneSetSerial` / `SubGhzSceneSetButton` /
  `SubGhzSceneSetCounter` / `SubGhzSceneSetMfKey`) backing the upcoming
  KeeLoq-family entry flow.  The three numeric editors reuse the
  host-tested hex-digit editor and are sized via the Phase 8c-1 catalog's
  `serial_bits` / `button_bits` / `counter_bits`.  The manufacturer picker
  lists every entry in the currently-loaded `keeloq_mfcodes` table (or
  the built-in flash vault) so users can select the master key whose
  cipher will be used to encrypt the assembled hop word.  A new
  `keeloq_mfkeys_get_at()` accessor allows ordered iteration of the
  in-memory table.  No user-visible behaviour change yet — the scenes
  are registered but not yet linked into a flow; the next phase will
  chain SetType → SetSerial → SetButton → SetCounter → SetMfKey →
  Transmitter for the three KeeLoq-family protocols.
