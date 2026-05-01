**Sub-GHz Read Raw: progress bar now freezes after signal drops below RSSI threshold** — the cursor
  advancement was previously driven by every ISR edge event including post-signal noise, bypassing
  the debounce logic. Cursor advance and debounce reset are now gated on RSSI being above the
  configured threshold so the bar stops moving once the signal ends (plus the debounce tail).
