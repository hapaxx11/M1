**Main menu: battery indicator now matches the splash/home screen** — The main
  menu's battery glyph (added in v0.9.3.4, cherry-picked from NipTek-M1) used a
  different icon and layout than the rest of the firmware. It now reuses the
  same `m1_draw_battery_indicator()` widget as the home screen (identical
  frame, proportional fill, and charging-bolt style), positioned in the free
  top-left area above the M1 logo. The old triangle-based charging bolt and
  below-icon percentage layout were removed as unused.
