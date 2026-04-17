**Sub-GHz Magellan decoder: fix inverted bit polarity and header skip** — The Magellan decoder
  incorrectly delegated to the generic OOK PWM decoder which reads standard polarity
  (bit 1 = LONG HIGH + SHORT LOW). Magellan uses inverted polarity (bit 1 = SHORT HIGH + LONG LOW),
  so every decoded bit was wrong. The decoder also failed to skip Magellan's unique frame header
  (800µs burst + 12 toggles + 1200µs start bit) before reading data bits. Replaced the
  broken generic-PWM delegation with a dedicated Magellan decoder that correctly identifies the
  start bit and decodes bits with the proper inverted polarity. Verified with 16 roundtrip unit
  tests covering bare-data buffers, full-frame buffers, and representative real-world keys.
  Protocol analysis also confirmed that CAME 12-bit, GateTX 24-bit, and Nice FLO 12-bit
  encoder/decoder pairs use consistent standard OOK PWM polarity (no bugs found in those).
