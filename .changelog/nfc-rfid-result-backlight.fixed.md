**NFC/RFID: keep the backlight on for the read result** — a successful read that
  followed a long scan with no button press could show its result/info screen
  with the backlight already dimmed by the inactivity timer. The read-complete
  screen now wakes the backlight and restarts the inactivity timer so the result
  is actually visible (ported from upstream Monstatek). Scanning still times out
  normally. The backlight screen-saver decision is now host-tested
  (`tests/test_lcd_saver.c`).
