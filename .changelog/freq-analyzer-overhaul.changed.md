**Sub-GHz: Frequency Analyzer overhaul** — Rewrote `sub_ghz_frequency_reader()` to match
  Momentum's frequency-analyzer UX. Fixes the incorrect "Frequency Reader" title. Now
  continuously sweeps a user-selected ISM band (300-316 / 387-464 / 779-928 MHz) each pass
  and reports the peak-signal frequency with a live RSSI bar graph. New controls: L/R cycles
  the scan band, UP/DOWN adjusts the detection threshold (−50…−100 dBm in 5 dBm steps), OK
  toggles hold/resume (freezes the displayed frequency), and BACK exits. Threshold marker is
  drawn over the RSSI bar so the user can see at a glance whether the current signal crosses
  it. A brief buzzer pulse fires when a signal above threshold is detected. Cleanup now uses
  `radio_set_antenna_mode(ISOLATED)` + SI4463 sleep + `menu_sub_ghz_exit()`, matching the
  patterns used by the Spectrum Analyzer and Freq Scanner.
