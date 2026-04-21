**Sub-GHz RSSI Meter: anchored "dBm" labels** — The "dBm" unit label now stays at a
  fixed pixel position for both the current and peak readings. Previously the label
  shifted horizontally as the numeric value changed digit count (e.g. -9 → -100).
  All five display elements (current value, "dBm", "Pk:", peak value, "dBm") are now
  drawn at independent, fixed X coordinates.
