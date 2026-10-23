**ESP32: multi-second freeze and watchdog boot loops with Bluetooth Direct
  enabled** — the ESP32 probe chain (reachable from the high-priority RPC task
  during Bluetooth Direct bring-up) busy-spun in HAL_Delay, up to ~10 s at a
  stretch in the AT-fallback retry loop, starving the UI and the watchdog feeder.
  Those waits now yield to the scheduler once it is running.
