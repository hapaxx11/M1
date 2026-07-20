**Sub-GHz: sweep-report aggregator for the Signal Identifier (RF Rosetta)** —
  Added the pure-logic core that turns a stream of per-detection
  identification results into a live, deduplicated, confidence-ranked report:
  identical protocols seen on the same band merge (raising confidence, keeping
  the strongest RSSI and its frequency), the list stays sorted best-first, and
  a bounded set of slots evicts only the weakest entry when a more confident
  signal arrives. This is the host-tested foundation the forthcoming Signal
  Identifier sweep scene will feed and render; it plugs into the existing
  fingerprint/match engine without touching the scoring or protocol database.
  Inspired by RF Rosetta.
