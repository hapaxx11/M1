**Battery: align BQ27421 taper current with BQ25896 ITERM; seed DEFAULT_DESIGN_CAP** — The
  BQ27421 fuel gauge was configured with a 240 mA taper current while the BQ25896 charger
  terminates at 256 mA. This mismatch caused every Qmax learning update to fire at the wrong
  operating point, biasing the full-charge capacity estimate and producing erratic mid-discharge
  SoC step corrections. Both are now 256 mA. Additionally, `DEFAULT_DESIGN_CAP` (the Qmax seed
  used on the very first power-on) is now written alongside `DESIGN_CAPACITY`, replacing the
  TI factory default of 1000 mAh with the correct design value so the gauge converges from the
  first cycle.
