**Bluetooth: BLE Spam AT command support** — BLE Spam now works with dag
  T-800 ESP32 firmware via `AT+M1BLESPAM=<mode>` commands. Added unified
  "BLE Spam" mode picker (All/Apple/Google/Microsoft) as the primary entry
  in the BLE Spam menu. Individual spam types (Sour Apple, SwiftPair, etc.)
  auto-detect AT vs binary SPI firmware and dispatch accordingly. SiN360
  binary SPI compatibility fully retained.
