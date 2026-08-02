**Sub-GHz: Protocol Filter (ignore protocols on capture)** — a Momentum-style
  per-protocol ignore list.  A new **Protocols** entry in the Sub-GHz **Config**
  screen opens a scrollable list of every registry protocol with an On / Ignored
  toggle (OK or LEFT/RIGHT flips the selection).  Ignored protocols are skipped
  by every Sub-GHz reading feature — Read (live capture), Read Raw, Decode Raw,
  Playlist decode, and the Hapax RF Rosetta **Signal ID** / **Smart ID** decode
  stage — via the shared `subghz_ignore_is_ignored()` gate.  The ignore list is
  persisted to the settings file as a compact hex bitmask (`subghz_ignore=`) and
  is host-tested (`tests/test_subghz_protocol_ignore.c`, plus new skip cases in
  `tests/test_subghz_decode_try_fn.c`).
