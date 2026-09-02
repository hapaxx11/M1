**RAM: ESP32 flasher stubs moved from RAM to flash** — the ~91 KB
  `esp_stub[]` table (`Esp32_serial_flasher`) was being placed in RAM (`.data`)
  because its payloads were non-`const` compound literals. Making them `const`
  moves them to flash (`.rodata`), cutting RAM usage from 99.98% (144 bytes
  free) to 85.80% (~93 KB free) with no change to flash usage. Resolves the
  critical RAM-budget condition (#747).
