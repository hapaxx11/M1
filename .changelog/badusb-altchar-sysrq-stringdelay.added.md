- **BadUSB: `STRINGDELAY`, `SYSRQ`, `ALTCHAR` and `ALTSTRING`/`ALTCODE`
  DuckyScript commands** — the BadUSB engine now covers the remaining common
  Flipper/Momentum DuckyScript commands. `STRINGDELAY <ms>` (alias
  `STRING_DELAY`) inserts a delay between each typed character for slow targets;
  `SYSRQ <key>` issues the Linux Magic SysRq combo (Alt+PrintScreen+key);
  `ALTCHAR <code>` types a single character via the Windows Alt+Numpad method;
  and `ALTSTRING`/`ALTCODE <text>` types an entire string using Alt+Numpad
  codes. The digit→keypad mapping and command classification are pure-logic and
  covered by host-side unit tests. With these, the M1 supports 13 of the 14
  Flipper/Momentum DuckyScript command families (only the hardware-coupled
  `WAIT_FOR_BUTTON_PRESS` remains).
