**DuckyScript parser: extract pure-logic module with 42 unit tests** —
  Extracted key-name lookup, modifier parsing, ASCII-to-HID mapping,
  and line-type classification from `m1_badusb.c` into a standalone
  `badusb_parser.c/h` module. Added host-side test suite
  (`test_badusb_parser.c`) covering named keys, modifier aliases,
  separator styles, ASCII mapping, and core DuckyScript line types
  (REM, DELAY, DEFAULT_DELAY, STRING, REPEAT, modifier+key combos,
  standalone keys). Advanced commands (ALTCHAR, HOLD/RELEASE, MOUSE,
  MEDIA, WAIT_FOR_BUTTON_PRESS, DEFINE) are not yet implemented.
