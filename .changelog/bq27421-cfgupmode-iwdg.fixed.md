**BQ27421: fixed IWDG reset loop after SD-card firmware update** — `bq27421_init()`
  ran two infinite CFGUPMODE polling loops before the watchdog task existed and at
  the highest FreeRTOS task priority, so nothing refreshed the 4-second IWDG.
  Added per-iteration `m1_wdt_reset()` calls and a 3-second hard timeout to both
  loops; the device no longer reboots continuously after flashing a new release.
