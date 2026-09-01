**Peer link (ESP-NOW): app-layer protocol modules** — Foundation for M1↔M1
  peer link over the ESP-NOW compatibility layer: application-layer
  fragmentation (`espnow_chunk`) that carries up to 240-byte payloads across the
  42-byte SPI ceiling, saved-capture sharing helpers (`espnow_shareable`), short
  peer text messaging (`espnow_message`), a danger-gated remote-trigger state
  machine (`espnow_trigger`), and an Encrypt-then-MAC authenticated-encryption
  envelope (`espnow_crypto`, AES-256-CBC + HMAC-SHA256).  All modules are pure
  logic with host unit tests; live use is gated behind `M1_ESP32_CAP_ESPNOW`,
  which the STM32 now infers for confirmed native CD3 until brain firmware
  self-reports bit 24 directly.
