**Sub-GHz: Signal Identifier scene (RF Rosetta)** — Added a new "Signal ID"
  entry to the Sub-GHz menu that sweeps a curated list of common ISM
  frequencies with the SI4463 radio and, whenever it catches activity above an
  adjustable RSSI threshold, samples a short RSSI burst to classify the
  modulation family (OOK vs FSK), builds a sensor-agnostic fingerprint, and —
  only when that fingerprint is discriminating enough to name a protocol —
  scores it against the protocol database. Matches accumulate into a live,
  confidence-ranked report the user reads on-screen (name + confidence, with
  scroll, clear, and threshold controls); frequencies with activity that
  cannot be identified are still surfaced as "active, unidentified" rather than
  a misleading guess. All decision logic lives in pure, host-tested modules
  (`rf_scan_plan`, `rf_ook_fsk`, plus the existing fingerprint/match/sweep
  core); the SI4463 delegate is a thin wrapper reusing the proven Freq Scanner
  receive path, so the scoring engine and protocol database are untouched. The
  OOK-vs-FSK-from-RSSI step is a heuristic and reports modest confidence.
  Inspired by RF Rosetta.
