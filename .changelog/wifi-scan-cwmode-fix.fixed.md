**WiFi: fix "Scan AP Failed" with dag T-800 firmware** — `AT+CWMODE=1` (Station mode)
  is now sent before `AT+CWLAP` in both the AP scan and PMKID Grab paths.  dag T-800
  may boot with WiFi disabled or in AP mode, causing `AT+CWLAP` to return `ERROR`.
  Also calls `esp32_queue_reset_locked()` to clear stale AT exchange data before scanning
  under the SPI-AT request semaphore.
  Mirrors the pattern established by `wifi_ap_scan_list()` in `esp_app_main.c`.
