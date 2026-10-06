**ESP32: add the host-side codec and client for MtkCore's canonical "Native M1
  SPI v1" transport.** This is the full-feature-parity transport MtkCore exposes
  alongside its Legacy-SPI compatibility adapter (magic `"M1S1"`, 1024-byte
  cells, a 40-byte little-endian header, CRC32C framing, 16-bit service + 16-bit
  opcode addressing, request-id fragment reassembly, and a paginated per-opcode
  `GET_CAPABILITIES` negotiation in place of a capability bitmap). The new
  `m1_esp32_native.h` codec and `m1_esp32_native.c` client implement the
  HELLO handshake, single- and multi-cell request/response exchange, and
  decoders for PING, GET_API_IDENTITY and GET_CAPABILITIES, driven through an
  injectable 1024-byte exchange primitive. Every host-verifiable part is covered
  by `tests/test_esp32_native.c` (31 tests: CRC32C check-value anchor, exact
  header byte offsets, cell build/verify, fragment reassembly, payload decoders,
  and the client over a fake transport). The physical 512→1024 SPI handshake and
  live transport activation are deferred to an on-hardware follow-up: MtkCore's
  own source documents the HELLO negotiation payload and the cell-size handshake
  as undefined, so they cannot be validated without the device.
