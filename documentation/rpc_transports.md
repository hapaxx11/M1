# RPC Transports

RPC frames use the same framing and CRC regardless of transport. USB CDC remains
the default ingress and egress. A non-USB transport must register a transmit
callback and feed complete byte chunks through `m1_rpc_feed_from_transport()`;
responses are sent through the transport that supplied each request.

The parser keeps ownership of an incomplete frame. If bytes arrive from another
transport before that frame completes, the feed call returns `false` without
consuming them. Transport adapters must retain and retry rejected bytes rather
than discard them.

Screen capture responses follow the request transport. Periodic screen frames
continue over the transport that started the stream. The device-info response
appends `mobile_session_active` after the existing battery detail fields; it is
true while the BLE/WiFi RPC client reports an active session through
`m1_rpc_set_mobile_session_active()`.
