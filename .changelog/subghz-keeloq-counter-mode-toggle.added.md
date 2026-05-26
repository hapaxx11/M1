**Sub-GHz: KeeLoq CounterMode toggle in SignalSettings + replay-path wiring (Phase 9d-3)** —
  The `CounterMode:` field added in Phase 9d-2 is now editable and honoured at
  transmit time.  The SignalSettings scene gains a third selectable row,
  `CntMode: Increment / Static`, which is always reachable on supported
  KeeLoq-family files (toggling does not require the manufacturer key to be
  resolvable, unlike the Counter editor).  OK on this row flips the value in
  place and persists the change via `flipper_subghz_save_key_full()`,
  preserving `Manufacture:` and the other fields.  The Phase 9b Button writer
  and Phase 9c-3 Counter writer were switched to the same `save_key_full()`
  helper so that editing Button or Counter on a Static file no longer silently
  resets the mode back to Increment.  The KeeLoq replay path in `m1_sub_ghz.c`
  now parses the `CounterMode:` field directly from the `.sub` stream and
  forwards it to `KeeLoqEncParams::static_counter`, so Static-mode `.sub`
  files re-emit the captured encrypted hop word verbatim instead of running
  the decrypt → counter+1 → re-encrypt path.  Two new host tests cover the
  load → save round-trip preservation for both modes (69 total in
  `test_flipper_subghz`).
