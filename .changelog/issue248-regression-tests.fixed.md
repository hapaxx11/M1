**Sub-GHz: Issue #248 regression tests** — added three host-side unit tests to
  `tests/test_flipper_subghz.c` covering the fw 0.9.0.122 payload-format bug: the
  corrupted `Payload: 0x000000000000000lX` (a `%lX` format artifact) is now confirmed
  to be parsed gracefully (key=0, no crash) for both the 330 MHz Asia/APAC gate-remote
  file and the 433.92 MHz file from the issue. A third test validates the full load
  path for a correctly-saved 330 MHz Princeton file.
