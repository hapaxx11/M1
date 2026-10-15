# RPC Transports

RPC frames use the same framing and CRC regardless of transport. USB CDC remains
the default ingress and egress. A non-USB transport must register a transmit
callback and feed complete byte chunks through `m1_rpc_feed_from_transport()`;
responses are sent through the transport that supplied each request.

The native ESP32 brain is a virtual RPC transport for the M1: its task polls
`RPC_STATUS` and `QMON_POLL`, then feeds queued qMonstatek frames through the
same request dispatcher. Replies return through `QMON_RESP`. Large M1 Link
requests are split into 502-byte `FRAG` payloads followed by the final request
frame, matching the brain's 512-byte SPI transaction protocol.

The parser keeps ownership of an incomplete frame. If bytes arrive from another
transport before that frame completes, the feed call returns `false` without
consuming them. Transport adapters must retain and retry rejected bytes rather
than discard them. The ESP32 relay retains one rejected qMonstatek frame in a
bounded heap allocation and retries it after the partial frame completes.

For an ESP32-relayed `SCREEN_START`, the M1 pushes its normalized 1024-byte
framebuffer using `SCREEN_PUSH`. The brain sends frames over NUS for a BLE
session or UDP for a WiFi session; its BLE path is capped at approximately
5 fps. Bluetooth Direct NUS advertising is opt-in from System Settings and the
preference is persisted in `0:/System/settings.cfg`.

The device-info response appends `mobile_session_active` after the existing
battery detail fields; it is byte offset 66 (when the full 67-byte response is
present). It reflects the brain's connected TCP/BLE client status through
`m1_rpc_set_mobile_session_active()`. The web updater exposes this as
`mobileSessionActive` when parsing the extended response.
