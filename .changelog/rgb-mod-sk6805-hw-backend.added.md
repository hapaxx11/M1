**RGB Mod: SK6805-1515 hardware backend finalised** — replaced the placeholder `rgb_backlight_hw_write()` with a real GPIO bit-bang driver targeting PD3 (GPIOD).
  Hardware is auto-detected at boot via a pull-down probe on PD3 (HIGH = chain present).
  Timing constants calibrated for 250 MHz Cortex-M33: T1H 720 ns / T0H 240 ns / reset 60 µs via DWT busy-wait.
  Interrupts are fully disabled during frame transmission to preserve strict NeoPixel waveform timing.
