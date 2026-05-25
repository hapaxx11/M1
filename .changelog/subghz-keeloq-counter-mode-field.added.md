**Sub-GHz: `CounterMode:` `.sub` field parser + writer (Phase 9d-2)** — Flipper
  Key files may now carry an optional `CounterMode:` line (`Increment` or
  `Static`) that selects the KeeLoq-family replay policy.  Files that omit the
  field, carry `Increment`, or carry an unknown value all load as INCREMENT, so
  every existing `.sub` file replays unchanged.  A new
  `flipper_subghz_save_key_full()` helper writes both `Manufacture:` and
  `CounterMode:` and elides the line when the value is the default Increment to
  keep Phase 9b/9c saved files byte-identical.  The Phase 9b
  `flipper_subghz_save_key_with_manufacture()` API is preserved and now
  delegates to the new helper.  Wiring the toggle to the SignalSettings UI and
  the replay path lands in Phase 9d-3.
