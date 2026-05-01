**RTC clock source switched from LSI to LSE** — the 32.768 kHz crystal (Y2) that is
  physically present on the board was not being used; the RTC was running from the internal
  RC oscillator (LSI, ±1–5% accuracy = up to 72 min/day drift).  Enabling LSE reduces
  drift to ±20–50 ppm (1–4 s/day).  No hardware change required.
