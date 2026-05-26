Sub-GHz: pure-logic `subghz_signal_fields` module that extracts and
  reassembles {serial, button, encrypted-hop} fields from the 64-bit
  Flipper SubGhz Key File representation for KeeLoq, Star Line, and
  Jarolift remotes.  Foundation for the upcoming SignalSettings scene
  (per-file counter / button editing).  20 new host tests cover known
  layouts for both KeeLoq/Jarolift and Star Line, round-trip
  invariants, a cross-check against the Phase 8c-3
  `subghz_keeloq_create_key()` flow, bad-argument paths, and
  over-range field masking; all pass under ASan+UBSan.
