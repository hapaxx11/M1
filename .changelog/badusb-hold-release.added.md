- **BadUSB: `HOLD` and `RELEASE` DuckyScript commands** — the BadUSB engine now
  supports pressing and holding keys/modifiers across subsequent commands.
  `HOLD <combo>` presses and holds one or more keys (e.g. `HOLD SHIFT`,
  `HOLD CTRL c`), and they stay down — even across `DELAY`s — until a matching
  `RELEASE <combo>` (or a bare `RELEASE`, which releases everything). The held
  keys are merged into every HID report (up to the 6-key boot-keyboard limit)
  and are always released when the script ends. Held-key bookkeeping lives in a
  new pure-logic module (`badusb_hold`) covered by host-side unit tests.
