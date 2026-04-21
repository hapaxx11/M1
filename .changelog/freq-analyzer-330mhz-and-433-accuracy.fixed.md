**Sub-GHz Frequency Analyzer: two-stage Momentum-style scan** — Adopted the same
  two-stage algorithm used by Momentum/Flipper firmware. Stage 1 (coarse) scans the
  preset frequency table (~25 entries per band, ~50 ms) instead of a linear MHz sweep,
  guaranteeing exact known frequencies like 433.920 MHz and 330.000 MHz are always
  checked. Stage 2 (fine) sweeps ±300 kHz at 20 kHz step around the coarse peak
  (~30 steps, ~60 ms) when a signal exceeds the threshold, providing sub-20-kHz
  accuracy for any signal — including non-preset frequencies. Total scan cycle
  ≈ 110 ms vs the previous 3.9 s linear sweep. Fixes: 330 MHz not detected in the
  300 MHz band, and 433.92 MHz incorrectly shown as 434.00 MHz.
