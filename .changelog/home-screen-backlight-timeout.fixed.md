**Home screen: backlight no longer wakes or stays on due to background refresh** —
  The home screen battery indicator and clock were refreshed by calling
  `startup_info_screen_display()`, which also turned the backlight on and reset the
  inactivity timer. This caused the screen to wake without user interaction (when
  charging state changed) and prevented the backlight from ever timing out while on
  the home screen. Background refreshes now use `startup_home_screen_refresh()`, which
  redraws display RAM without touching the backlight or the timeout counter.
  Additionally, the clock (`HH:MM`) in the home screen status bar now updates every
  minute even when battery data is unchanged.
