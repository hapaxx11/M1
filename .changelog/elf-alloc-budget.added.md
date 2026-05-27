ELF loader now enforces a 96 KB cumulative allocation budget per app load,
  preventing oversized or malformed `.m1app` files from exhausting the shared
  FreeRTOS heap. The budget leaves sufficient headroom for ESP32/STM32 firmware
  flashing and other system operations. Loads that exceed the budget return
  `ELF_ERR_BUDGET` and display "App too large" to the user.
