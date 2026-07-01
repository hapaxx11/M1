**Infrared: Learn New Remote now saves again** — fixed a decoder bug where
`irmp_get_data()` never reported a completed frame for stop-bit protocols
(NEC and most TV remotes), so "Learn New Remote" stayed on *Reading...* and
pressing OK exited without saving. The end-of-frame completion was gated on a
stop-bit flag that the edge-based sampler set but never cleared (the original
interrupt-driven IRMP re-entered to clear it; the M1 port does not). Captured
signals now decode and save.
