**f_write return value checks** — added FRESULT checks to all unchecked
  `f_write()` calls in WiFi (7 sites) and Bluetooth (4 sites) file I/O paths.
  Loop writes now break on failure; header writes return false; best-effort
  single writes explicitly acknowledge the return value.
