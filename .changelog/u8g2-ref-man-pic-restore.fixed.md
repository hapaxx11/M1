**u8g2: Restore `U8G2_REF_MAN_PIC` define** — lost during u8g2 v2.36.19 wholesale
  file replacement. Without this M1-specific define, `m1_message_box()` (via
  `u8g2_UserInterfaceMessage()`) enters u8g2's internal GPIO polling loop instead
  of returning to M1's FreeRTOS event handler, causing message boxes to block.
  Added vendored dependency update audit rules to CLAUDE.md to prevent recurrence.
