**ESP32: support Monstatek MtkCore firmware over its Legacy-SPI compatibility
  adapter.** MtkCore speaks our m1_link binary-RPC wire protocol but reports a
  zero capability bitmap by design, so the host previously misdetected it as an
  AT device and disabled every feature. The host now fingerprints MtkCore
  (`cap_bitmap == 0` plus a dotted-semver `fw_name` such as `0.8.1.0`),
  synthesises the new `M1_ESP32_CAP_PROFILE_MTKCORE` capability profile, and
  routes it to `ESP32_TRANSPORT_RPC`. This lights up WiFi
  scan/join/deauth/beacon/handshake/SoftAP/packet-monitor/captive-portal and BLE
  scan/adv/GATT. ESP-NOW, 802.15.4, PMKID, karma, probe-flood and BLE HID/spam
  remain unsupported over the compat adapter (they require MtkCore's Native M1
  SPI v1 transport, not yet implemented host-side). Detection and transport
  routing are host-tested (`tests/test_esp32_caps.c`,
  `tests/test_esp32_feature_map.c`).
