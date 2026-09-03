- **BadUSB: `STRINGLN` and `REM_BLOCK`/`END_REM` DuckyScript commands** — the
  BadUSB engine now supports two standard Flipper/Momentum DuckyScript commands
  that many payloads rely on. `STRINGLN <text>` types the text and then presses
  ENTER (a bare `STRINGLN` just presses ENTER), and `REM_BLOCK` … `END_REM`
  delimits a multi-line comment block so long payloads can be documented without
  a `REM` on every line. Both are parsed by the pure-logic DuckyScript parser and
  covered by host-side unit tests.
