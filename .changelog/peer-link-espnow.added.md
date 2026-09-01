**Peer link (ESP-NOW): app-layer protocol modules** — Foundation for M1↔M1
  peer link over the ESP-NOW compatibility layer: application-layer
  fragmentation (`espnow_chunk`) that carries up to 240-byte payloads across the
  42-byte SPI ceiling, saved-capture sharing helpers (`espnow_shareable`), short
  peer text messaging (`espnow_message`), a danger-gated remote-trigger state
  machine (`espnow_trigger`), and an Encrypt-then-MAC authenticated-encryption
  envelope (`espnow_crypto`, AES-256-CBC + HMAC-SHA256).  Peer Link also now
  exposes Messages, Send Capture, and Remote Trigger scenes: paired devices can
  compose short direct-frame text messages, send saved Sub-GHz/NFC/RFID/IR
  captures through the existing storage browser, use Send to Peer shortcuts from
  saved capture action menus over the CRC-checked transfer path, and request a
  paired peer to replay a named Sub-GHz/IR capture only after explicit receiver
  consent.  The pure modules
  have host unit tests; live use is gated behind `M1_ESP32_CAP_ESPNOW`, which the
  STM32 now infers for confirmed native CD3 until brain firmware self-reports bit
  24 directly.
