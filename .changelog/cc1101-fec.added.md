**Sub-GHz: CC1101 FEC encode/decode utility** — imported and adapted the CC1101
  Forward Error Correction (FEC) algorithm from
  [SpaceTeddy/urh-cc1101_FEC_encode_decode_plugin](https://github.com/SpaceTeddy/urh-cc1101_FEC_encode_decode_plugin).
  Adds `Sub_Ghz/cc1101_fec.c` / `cc1101_fec.h`: a hardware-independent C module
  implementing the rate-1/2 Viterbi convolutional codec, 4-byte block interleaver,
  and CRC-16/IBM used by TI CC1101-based IoT sensors when their FEC feature is enabled.
  Enables the M1 to decode raw GFSK packets from CC1101-FEC devices in software.
  Covered by 26 host-side unit tests (encode/decode roundtrip, known vector, edge cases).
