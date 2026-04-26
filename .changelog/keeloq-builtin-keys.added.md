**Sub-GHz KeeLoq: build-time manufacturer key embedding** — KeeLoq manufacturer
  keys can now be baked into the firmware binary at build time from a private key
  vault (Flipper-compatible approach). When a `KEELOQ_KEY_VAULT` GitHub Actions
  secret is configured, `scripts/gen_keeloq_mfkeys_builtin.py` embeds the keys
  directly into flash before compilation; `keeloq_mfkeys_load()` uses them without
  ever consulting the SD card, so the keys are never visible as a file on removable
  media. Builds without the secret fall back to the existing SD card keystore
  (`keeloq_mfcodes.enc`).
