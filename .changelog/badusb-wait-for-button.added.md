- **BadUSB: `WAIT_FOR_BUTTON_PRESS` DuckyScript command (experimental)** — the
  BadUSB engine now recognizes and acts on `WAIT_FOR_BUTTON_PRESS`, completing
  parity (14/14) with the Flipper/Momentum DuckyScript command families. Because
  the M1 has no dedicated "continue" key during a run, the command is mapped to
  "pause until any M1 keypad button is pressed" (OK/UP/DOWN/LEFT/RIGHT resume,
  BACK aborts). As this behavior is **experimental**, a paused script is signaled
  non-intrusively three ways: an on-screen "BadUSB Paused / WAIT_FOR_BUTTON
  (experimental)" prompt, a slow (~1 Hz) red LED flash for the duration of the
  wait, and a debug-log warning. The command classification is pure-logic and
  covered by host-side unit tests.
