ESP32 capability cache now persists across `m1_esp32_deinit()` / `m1_esp32_init()`
  cycles for the lifetime of the STM32 firmware.  Capabilities are a property of
  the ESP32 firmware variant and cannot change without a system restart, so the
  cache no longer needs to be cleared on every WiFi/BT/IEEE scene exit — this
  removes a ~5 s `AT+CMD?` re-probe penalty on every scene entry when the
  connected ESP32 runs AT firmware.  `m1_esp32_caps_reset()` remains available
  for explicit invalidation after an in-field OTA reflash of the coprocessor.
