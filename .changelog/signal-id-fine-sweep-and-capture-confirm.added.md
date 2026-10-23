**Sub-GHz: Smart ID fine-sweep refinement** — After the pre-scan, Smart ID now
  re-centers the 8 strongest detected frequencies with the Remote Analyzer's
  31-point fine sweep (±300 kHz, 20 kHz steps) and uses the sweep's AM/FM
  verdict (preferred over the burst-based classifier) when identifying the
  signal.  The Remote Analyzer scene's fine stage now shares the same radio
  sweep helper (behavior unchanged).

**Sub-GHz: Signal ID decoder confirmation** — When a fingerprint match links
  to a real protocol decoder (automotive/registry-linked signatures), Signal
  ID and Smart ID now make one bounded (400 ms) real edge-capture decode
  attempt on the hit frequency; a successful decode upgrades the entry to a
  100%-confidence decoder-named hit.  Attempts are limited to one per
  signature+band and a per-pass time budget; failed attempts are silent.
