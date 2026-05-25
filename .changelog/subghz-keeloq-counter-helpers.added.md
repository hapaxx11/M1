**Sub-GHz: KeeLoq counter decode / encode pure-logic helpers** — new
  `subghz_signal_fields_keeloq_counter_decode()` /
  `subghz_signal_fields_keeloq_counter_encode()` in
  `Sub_Ghz/subghz_signal_fields.{c,h}` provide a named, host-testable API
  for reading and substituting the 16-bit rolling counter inside a KeeLoq
  encrypted HOP word.  Foundation for the upcoming SignalSettings
  per-file counter editor (Phase 9c-2 / 9c-3).  6 new host tests cover
  round-trip, low-16-plaintext-bit preservation under substitution, and
  equivalence with `keeloq_increment_hop()` for the counter+1 case
  (82 host tests total, all passing under ASan+UBSan).
