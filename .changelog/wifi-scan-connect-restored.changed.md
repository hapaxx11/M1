**WiFi: Restore Scan & Connect screen** — "Scan & Connect" (top-level WiFi menu) now
  scans for APs and, when OK is pressed, prompts for a password and attempts to connect,
  restoring the original scan+connect workflow. With SiN360 ESP32 firmware the connect
  command is not yet supported; the screen shows "Not available / Use AT firmware" rather
  than silently launching a deauth attack. Deauth remains available under WiFi → Attacks → Deauth.
