**KeeLoq: removed SD card key delivery/bundling** — manufacturer keys are now
  embedded directly into the firmware binary at build time via the GitHub
  Actions secret `KEELOQ_KEY_VAULT`.  The bundled/distributed
  `SubGHz/keeloq_mfcodes` SD card file, the web updater's "Install KeeLoq
  keys" option, `scripts/convert_keeloq_keys.py`,
  `scripts/encrypt_keeloq_keys.py`, and the bundled `keeloq_mfcodes.example`
  template file have all been removed.  Manual SD card installation
  (`0:/SUBGHZ/keeloq_mfcodes` or `0:/SUBGHZ/keeloq_mfcodes.enc`) is still
  supported as a fallback for users without the vault secret.
  `scripts/gen_keeloq_mfkeys_builtin.py` (the build-time key embedder) is
  retained.
