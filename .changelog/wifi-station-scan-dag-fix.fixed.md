**WiFi: Station Scan / Station Wardrive "Start failed" on dag T-800 firmware** — Station Scan
  and Station Wardrive called the binary-SPI-only `CMD_STA_SCAN_START` command with no ESP32
  capability check. dag T-800 AT firmware never sets `M1_ESP32_CAP_STA_SCAN` (it implements
  neither the SiN360 binary command nor neddy299's `AT+STASCAN`), so the scan silently failed to
  start. Both delegates now use the capability-gated `DELEGATE_FEATURE` wrapper and show the
  standard "not supported" screen instead of failing silently.
