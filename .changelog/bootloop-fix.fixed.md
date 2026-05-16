**Boot loop fix — IWDG deferred + escape hatch**: The IWDG (watchdog timer) is
  now armed inside `m1_wdt_init()` instead of `main()`, so its countdown does not
  start until LCD, SD card, and startup config have all completed. The prescaler is
  widened from 32 to 128 (4 s → 16 s window) as belt-and-suspenders. A backup-register
  boot-loop escape hatch suppresses the IWDG after 3 consecutive watchdog resets,
  allowing the device to always reach the main menu for recovery without a debugger.
