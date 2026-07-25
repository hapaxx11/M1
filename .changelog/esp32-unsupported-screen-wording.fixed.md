**ESP32 "feature not supported" screen wording** — the capability-gate screen
  shown when a feature is unavailable read the broken-English "Flash compatible
  ESP32 firmware".  It now reads "Flash a compatible ESP32 firmware".  The body
  lines are exposed as macros (`M1_ESP32_UNSUPPORTED_LINE_*`) so the wording is
  covered by host-side unit tests.
