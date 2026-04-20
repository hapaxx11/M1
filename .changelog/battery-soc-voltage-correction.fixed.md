**Battery: voltage-based SoC correction for miscalibrated fuel gauges** — When
  the BQ27421 fuel gauge reports 0% (or ≤5%) state of charge but the measured
  battery voltage indicates meaningful charge remains (above the configured
  terminate threshold), a piecewise-linear voltage-to-SoC estimate is applied
  as a floor.  This prevents the "Asian M1" hardware variant — where the device
  stays powered and fully functional at a reported 0% — from incorrectly blocking
  firmware updates that require ≥25% battery.
