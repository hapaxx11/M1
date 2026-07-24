**Documentation: disambiguate CD3 vs CD3-AT ESP32 firmware, correct CD3 OTA/PMKID status** —
  bedge117 has authored two separate, non-interoperable ESP32-C6 coprocessor
  firmwares that were previously conflated in our docs under "C3"/"bedge117".
  These are now clearly named: **CD3-AT** (`bedge117/esp32-at-monstatek-m1`,
  AT-command based; also covers the neddy299/dagnazty/hapaxx11 forks of it)
  and **CD3** (`bedge117/m1-esp32-brain`, native ESP-IDF binary `M1_RPC`
  protocol). Also corrected the documented status of CD3's PMKID capture and
  ESP32 OTA self-update capabilities: verified against public source that both
  are reserved-but-unimplemented `M1_RPC` message IDs (no dispatch case,
  NAK `ERR_UNSUPPORTED`) in shipped CD3 releases — not working features as
  previously implied. WPA handshake capture is dispatched and functional but
  not yet included in CD3's self-reported capability bitmap. Updated
  `README.md`, `documentation/esp32_firmware.md`,
  `m1_csrc/m1_esp32_caps.h`/`.c` comments, and the `esp32-coprocessor` /
  `forks-tracker` skills accordingly. No functional/code changes.
