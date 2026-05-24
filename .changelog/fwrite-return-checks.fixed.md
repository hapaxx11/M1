**f_write return value checks** — added FRESULT checks to all unchecked
  `f_write()` calls in WiFi (7 sites), Bluetooth (4 sites), Settings (25 sites
  via SETTINGS_WRITE macro), IR Universal Remote (4 sites), and IR Quick Remote
  (2 sites). Loop writes now break on failure; header writes return false;
  settings save tracks write errors with a flag and logs on failure.
