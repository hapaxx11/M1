**Sub-GHz: SignalSettings live counter display** — The Saved → Settings
  scene now decrypts and displays the live 16-bit rolling counter for
  KeeLoq-family `.sub` files (KeeLoq / Star Line / Jarolift), replacing
  the prior `(9c)` placeholder.  Counter resolution uses the file's
  `Manufacture:` line + the loaded manufacturer-key table; files using
  Secure learning, missing a `Manufacture:` line, or whose manufacturer
  is absent from the keystore display `Counter: key?` so the gating
  cause is visible at a glance.  Cross-scene accessors
  `subghz_signal_settings_has_counter()` / `_get_counter()` are exposed
  for the upcoming editable-counter editor.
