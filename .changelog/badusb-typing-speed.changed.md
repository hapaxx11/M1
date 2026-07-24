**Bad-USB: faster typing** — Removed redundant fixed per-key/per-character
  delays in DuckyScript typing; timing is now gated solely by the existing
  HID report poll-complete wait, delivering ~4x faster typing while still
  guaranteeing every key-down/key-up edge is sent in order. Cherry-picked
  from bedge117/M1 (C3).
