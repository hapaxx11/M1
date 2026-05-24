**Watchdog: feed IWDG before long blocking delays** — Added `m1_wdt_reset()`
  calls before every `vTaskDelay(≥2500 ms)` in error-message display paths
  across `m1_http_client.c`, `m1_esp32_fw_download.c`, and `m1_fw_download.c`.
  Prevents potential IWDG-triggered reboots when error screens are shown
  during firmware download or SNTP sync operations.
