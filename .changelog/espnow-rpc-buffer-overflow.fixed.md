**ESP-NOW brain: peer list and message reception undersized response buffer fixed** —
  auditing all other brain-based ESP32 RPC calls after the WiFi/BLE scan
  buffer overflow fix turned up the same defect in the ESP-NOW HAL
  (`m1_espnow_hal.c`): every `M1_RPC_NOW_*` call hardcoded the response
  reception capacity to `SPI_BUF_SIZE` (64 bytes) inside `espnow_rpc_cmd()`,
  ignoring the caller's actual buffer size, and truncated the returned
  length to a `uint8_t`. `M1_RPC_NOW_PEERS_GET` can report up to 16 peers
  (up to 497 bytes) and `M1_RPC_NOW_RECV_GET` up to a 240-byte message (249
  bytes total) — both silently truncated well before parsing, dropping
  peers from the discovery list and corrupting/losing received messages.
  `espnow_rpc_cmd()` now takes an explicit response-capacity/length pair,
  `m1_espnow_poll_peers()` / `m1_espnow_recv_msg()` size their buffers to
  the protocol's documented worst case, and the peer/message decoders were
  extracted into a pure, host-testable `espnow_rpc_parse.c` module.
