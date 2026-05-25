**Sub-GHz: KeeLoq Static counter mode (Phase 9d-1)** — `keeloq_encode_replay()`
  now accepts a `static_counter` parameter that, when set, re-emits the captured
  encrypted hop word verbatim instead of incrementing the 16-bit rolling counter.
  Pure-logic foundation only; the existing replay path continues to default to
  Increment mode.  A user-facing toggle to select Increment vs Static per `.sub`
  file lands in Phase 9d-3.
