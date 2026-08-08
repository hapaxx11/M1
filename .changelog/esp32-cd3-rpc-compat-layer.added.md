**ESP32 CD3 M1_RPC compatibility layer** — added a reusable host-side M1_RPC
  client (`m1_esp32_rpc.c/.h`) so WiFi / BLE / 802.15.4 features can drive the
  native "brain" CD3 firmware (bedge117/m1-esp32-brain), which speaks the
  binary M1_RPC protocol instead of AT text commands. Provides the canonical
  opcode map, payload structs, a NAK/status-aware `m1_esp32_rpc_call()`, and an
  `esp32_firmware_transport()` selector that routes each detected firmware to
  the right encoder. ESP-NOW is refactored onto the shared client as its first
  consumer. The legacy CD3-AT firmware continues to use the AT path unchanged.
