**Sub-GHz Read Raw: improve "Record failed" diagnostics** — the error message now
  distinguishes between low-memory and SD-card failures and shows the current free
  heap size (e.g. "Heap free: 12345 B") so the root cause can be identified.
  Matching `M1_LOG_I` lines are emitted on the serial console for both failure paths.
