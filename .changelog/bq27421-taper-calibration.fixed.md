**Battery: align BQ27421 taper current with BQ25896 ITERM; seed DEFAULT_DESIGN_CAP** — The
  BQ27421 fuel gauge taper current is now 256 mA to match the BQ25896 charger's ITERM setting
  (was 240 mA). `DEFAULT_DESIGN_CAP` is now explicitly written to the State block alongside
  `DESIGN_CAPACITY`, replacing the TI factory default of 1000 mAh so the gauge seeds Qmax
  from the correct design capacity on first boot.
