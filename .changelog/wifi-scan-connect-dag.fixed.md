**WiFi: Scan & Connect now works with dag T-800 AT firmware** — `wifi_scan_ap()` now starts
  the SPI-AT task before probing firmware capabilities, allowing dag firmware to be correctly
  identified. Scan uses `AT+CWLAP` and connect uses `AT+CWJAP` on AT firmware; SiN360 binary-SPI
  path (`CMD_WIFI_SCAN_START/NEXT`, `CMD_WIFI_JOIN`) is unchanged.
