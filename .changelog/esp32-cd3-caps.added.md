**ESP32: CD3 firmware support (bedge117/m1-esp32-brain)** — Map capability
  bits 18–20 for the new CD3 native binary-RPC firmware: `M1_ESP32_CAP_PMKID`
  (dedicated PMKID capture), `M1_ESP32_CAP_HANDSHAKE` (WPA handshake/EAPOL
  capture with pcap), and `M1_ESP32_CAP_OTA` (ESP32 OTA self-update).  Add
  `M1_ESP32_CAP_PROFILE_CD3` conservative fallback profile.  Detect CD3
  automatically via M1_RPC PING (magic `0x4D31`) as probe 3 in
  `m1_esp32_caps_init()`, before the AT+CMD? fallback.  Add `esp32_firmware_is_cd3()`
  classifier and `ESP32_FEATURE_PMKID/HANDSHAKE/OTA` feature-map entries.
  Pure-logic M1_RPC frame helpers (CRC16, build/parse) are in
  `m1_esp32_caps.h`; host-tested (94 tests pass).
