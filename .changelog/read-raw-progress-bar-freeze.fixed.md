**Sub-GHz Read Raw: progress bar freezes immediately when signal drops below RSSI threshold** — cursor
  advancement was previously driven by post-signal activity even after the real transmission had
  ended. Cursor advancement is now gated by RSSI on each tick, so the bar stops moving
  immediately once the signal falls below the configured threshold.
