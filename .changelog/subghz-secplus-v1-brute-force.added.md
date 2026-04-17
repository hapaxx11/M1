**Sub-GHz Brute Force: Security+ 1.0 (Chamberlain) rolling-code mode** — Brute Force
  now supports Security+ v1 / Chamberlain as a counter mode: the ternary OOK two-sub-packet
  encoding is computed on-device from the counter value and transmitted via the existing raw
  TX path.  A new pure-C encoder module (`subghz_secplus_v1_encoder`) implements the
  argilo/secplus algorithm (MIT) adapted from Flipper Zero firmware (GPLv3) with 11 host-side
  unit tests.
