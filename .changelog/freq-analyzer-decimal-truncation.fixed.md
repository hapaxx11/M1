**Sub-GHz Frequency Analyzer: decimal digits no longer always display as `.000`** — The
  fine-scan stage (Stage 2) was gated behind the signal threshold, so it only ran when RSSI
  exceeded the user-settable threshold (-75 dBm default). The coarse scan only checks preset
  frequencies and a 2 MHz grid — both produce exact-MHz values with a zero kHz component.
  With no real signal above threshold the fine scan never fired, so the displayed frequency
  was always a coarse-grid multiple with ".000" decimal. The threshold gate has been removed
  from Stage 2: the fine scan now always runs when a valid coarse peak exists, giving sub-MHz
  precision regardless of signal strength. The threshold continues to control the buzzer
  notification and "Signal!" indicator.
