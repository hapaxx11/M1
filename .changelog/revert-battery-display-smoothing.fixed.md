**Battery display: revert rate-limiter; show raw fuel-gauge SoC directly** — Removed the
  1%/s rate-limiter (`battery_soc_smooth`) that caused the displayed percentage to lag the
  logical value used by firmware-update guards and low-battery checks. All display sites
  now use `battery_level` directly, eliminating the confusing split where the screen could
  show a different number than the decision-making code was using. Matches the approach used
  by Flipper Zero, Momentum, and Unleashed firmware.
