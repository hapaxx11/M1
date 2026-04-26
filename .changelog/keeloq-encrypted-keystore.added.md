**KeeLoq keystore: AES-256-CBC encrypted storage** — The manufacturer key
  file (`keeloq_mfcodes`) is now stored as an AES-256-CBC encrypted binary
  (`keeloq_mfcodes.enc`) on the SD card so that keys are not exposed as plain
  text.  The firmware decrypts the file on load using a fixed product key.
  Legacy plaintext files are automatically migrated to the encrypted format on
  the next boot (encrypted copy created, plaintext deleted — no manual step
  needed).  A new `scripts/encrypt_keeloq_keys.py` companion script converts
  and encrypts RocketGod SubGHz Toolkit output or compact plaintext in one
  step; `convert_keeloq_keys.py` is retained for backward compatibility.
