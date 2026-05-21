**Firmware update watchdog timeout** — added missing `m1_wdt_reset()` calls
  to the ESP32 firmware update path (image file MD5 validation, flash erase/write
  loop, and backup read loop) and to the STM32 bank CRC verification loop,
  preventing IWDG-triggered reboots and freezes during firmware operations.
