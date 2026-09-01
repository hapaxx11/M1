**ESP32: wall-clock timing on the "no-reply" feature RPC diagnostic (issue #719 Phase 6)** —
  A repeat field report showed WiFi Scan still failing with the exact same
  `Settings > Dashboard > page 5/5` line (`"op0103 no-reply st253 r0 p0"`) even
  after the Phase 5 timeout widening, which is ambiguous: it cannot tell apart
  "the scan genuinely needs longer than the 10 s budget" from "the transport
  gave up early despite the fix." The `"no-reply"` line now appends how long
  the M1 Link transport actually waited, e.g.
  `"op0103 no-reply st253 r0 p0 t10s"` (waited the full budget — needs more
  time) vs `"op0103 no-reply st253 r0 p0 t1s"` (gave up early — a poll-budget
  bug, not a too-small number), pinpointing which root cause to chase next.
