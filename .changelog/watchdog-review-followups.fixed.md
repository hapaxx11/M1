**Watchdog boot-loop counter hardening**: Boot-loop counter (RTC backup DR2)
  is now clamped to `[0, M1_WDT_BOOT_LOOP_MAX]` before incrementing, preventing
  stale or garbage data retained while VBAT is present (e.g., across a firmware
  flash) from tripping the 3-reset threshold on a single subsequent IWDG reset.
  IWDG reload ticks (`IWDG_RELOAD`) are now cleanly separated from the FreeRTOS
  WDT task period by introducing `M1_WDT_TASK_PERIOD_MS` (2000 ms), eliminating
  the unit confusion between hardware counter values and milliseconds.
  CubeMX regeneration warning strengthened in `MX_IWDG_Init()` to explicitly
  document that the deferred `HAL_IWDG_Init()` call must be manually removed
  from any freshly regenerated `main.c`.
