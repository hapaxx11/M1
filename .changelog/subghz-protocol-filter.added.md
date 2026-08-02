**Sub-GHz: Protocol Filter (ignore protocol groups on capture)** — a
  Momentum-style category ignore filter.  A new **Ignore** entry in the Sub-GHz
  **Config** screen opens a short list of protocol *groups* — **Weather**,
  **TPMS**, **Vehicles**, **Gates**, **Sensors**, **Pagers** — each with an
  On / Ignored toggle (OK or LEFT/RIGHT flips the selection).  Every protocol
  belonging to an ignored group is skipped by all Sub-GHz reading features —
  Read (live capture), Read Raw, Decode Raw, Playlist decode, and the Hapax RF
  Rosetta **Signal ID** / **Smart ID** decode stage — via the shared
  `subghz_ignore_is_ignored()` gate.  Group membership is data-driven from the
  registry (Weather/TPMS from protocol `type`, the rest from a name-keyed
  table).  The ignored-group set is persisted to the settings file as a compact
  hex bitmask (`subghz_ignore=`) and is host-tested
  (`tests/test_subghz_protocol_ignore.c`, plus new skip cases in
  `tests/test_subghz_decode_try_fn.c`).
