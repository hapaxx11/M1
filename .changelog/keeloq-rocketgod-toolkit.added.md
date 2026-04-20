**Sub-GHz: RocketGod SubGHz Toolkit integration** — The `keeloq_mfcodes` manufacturer
  key store parser now accepts both the compact Flipper-compatible format
  (`HEX:TYPE:Name`) and the multi-line format exported by
  [RocketGod's SubGHz Toolkit](https://github.com/RocketGod-git/RocketGods-SubGHz-Toolkit)
  for Flipper Zero.  Users can copy the toolkit's `keeloq_keys.txt` directly to
  `SUBGHZ/keeloq_mfcodes` on the M1's SD card without a conversion step.  A new
  `scripts/convert_keeloq_keys.py` utility is also included for users who prefer the
  compact format.  The new `keeloq_mfkeys_load_text()` API enables host-side unit
  testing of the parser; 22 new C tests and 28 Python tests cover both format variants.
