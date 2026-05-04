**Sub-GHz Read Raw: fixed "Record failed / Check SD card or free memory" after using ESP32 features** —
  After switching to the SiN360 binary-SPI ESP32 firmware, Sub-GHz Read Raw failed whenever Bad-BT,
  Zigbee scan, or ESP32 firmware update had been used first in the same session.  The AT-over-SPI task
  created by `esp32_main_init()` held ~17 KB of heap (8 KB task stack, 4 KB capture buffer, 4 KB stream
  buffer, semaphores) that was never released when `m1_esp32_deinit()` ran, leaving insufficient free
  heap for the 128 KB capture buffer and 28 KB SD-writer reserve required by Read Raw.
  `esp32_main_deinit()` is now called inside `m1_esp32_deinit()` on every code path, reclaiming the
  full allocation before Sub-GHz starts.
