**Firmware Update: default ESP32 source pointed at retired repo** — the
  device's built-in "esp32" download source list defaulted to
  `bedge117/esp32-at-monstatek-m1`, which bedge117 has retired in favor of the
  native SPI "Brain" firmware (`bedge117/m1-esp32-brain`). Hapax already
  speaks that firmware's M1_RPC binary SPI protocol (see `m1_esp32_caps.h`),
  so the retired-repo default meant selecting it from the device menu would
  fetch an ESP32 image that is no longer compatible with current C3/Brain or
  genuine stock builds. The default and the auto-appended `esp32` category
  block (`m1_csrc/m1_fw_source.c`) now point to `bedge117/m1-esp32-brain`'s
  `factory_m1-esp32-brain.bin` asset (matches the existing 0x0 factory-flash
  default), keeping Hapax devices aligned with bedge117's actively maintained
  ESP32 co-processor firmware.
