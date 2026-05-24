**DuckyScript parser: extract pure-logic module with 42 unit tests** —
  Extracted key-name lookup, modifier parsing, ASCII-to-HID mapping,
  and line-type classification from `m1_badusb.c` into a standalone
  `badusb_parser.c/h` module. Added comprehensive host-side test suite
  (`test_badusb_parser.c`) covering all named keys, modifier aliases,
  separator styles, ASCII mapping, and every DuckyScript line type.
