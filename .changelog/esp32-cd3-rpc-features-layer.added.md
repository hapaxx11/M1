**ESP32 brain-CD3 per-feature M1_RPC layer** — WiFi / BLE / 802.15.4 features can
  now drive the native "brain" CD3 firmware (bedge117/m1-esp32-brain), not just
  ESP-NOW. Adds `m1_esp32_rpc_features.c/.h` on top of the M1_RPC client: the
  authoritative feature → opcode map (`esp32_feature_rpc_opcode()`) plus a typed
  action wrapper per feature action (`m1_esp32_rpc_wifi_scan()`,
  `m1_esp32_rpc_deauth_start()`, `m1_esp32_rpc_ble_hid_key()`,
  `m1_esp32_rpc_zb_sniff_get()`, and the `*_start` / `*_stop` triggers) that
  builds the canonical payload, dispatches over SPI-HD, and decodes the reply.
  The 802.15.4 Zigbee/Thread sniffer and flood (`m1_802154.c`) now branch on
  `m1_esp32_active_transport()` and use these calls on brain CD3 while keeping
  the AT `+ZIGSNIFF` / `+ZIGFLOOD` text path for every other build (incl. legacy
  CD3-AT). The layer is transport-injectable and fully host-tested
  (`tests/test_esp32_rpc_features.c`).
