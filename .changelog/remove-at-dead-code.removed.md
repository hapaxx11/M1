**Remove dead AT-layer modules** — `m1_deauth.c/h` (AT+DEAUTH/AT+STASCAN deauther,
  replaced by `wifi_attack_deauth()` via binary SPI) and `m1_ble_spam.c/h` (AT+BLEADVDATA
  BLE spam, replaced by `ble_spam_*()` functions in `m1_bt.c` via binary SPI) were still
  compiled but never called after the SiN360 adoption. Both files and their CMakeLists
  entries removed.
