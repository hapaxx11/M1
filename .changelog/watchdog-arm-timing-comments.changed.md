Documentation: Clarified IWDG arming/timing comments to match the real init order (`m1_wdt_init()` runs before `startup_config_handler()`), including the early-boot 16s watchdog window description.
