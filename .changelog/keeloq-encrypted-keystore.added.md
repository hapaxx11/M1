**KeeLoq keystore: AES-256-CBC encrypted SD fallback** — When manufacturer
  keys are not embedded at build time, the firmware can load them from an
  AES-256-CBC encrypted binary (`0:/SUBGHZ/keeloq_mfcodes.enc`) on the SD
  card so that keys are not exposed as plain text.  The firmware decrypts the
  file on load using a fixed product key.  Legacy plaintext
  `0:/SUBGHZ/keeloq_mfcodes` files are automatically migrated to the
  encrypted format on the next boot (encrypted copy created, plaintext deleted
  — no manual step needed).  Both compact `HEX:TYPE:NAME` lines and RocketGod
  SubGHz Toolkit multi-line format are accepted inside the encrypted payload.
