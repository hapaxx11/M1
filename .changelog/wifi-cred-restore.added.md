**WiFi: Restore encrypted credential save/load** — `wifi_saved_networks()` now shows a
  scrollable list of saved networks (up to 8), allows one-tap reconnect using the stored
  password (no prompt), and supports LEFT-button deletion with a confirmation dialog.
  Credentials are AES-256-CBC encrypted and stored in `0:/System/wifi_creds.bin` via the
  existing `m1_wifi_cred` module. Successful connects from both Scan & Connect and
  General → Join WiFi now automatically save credentials. `wifi_show_status()` now
  displays the actual connected SSID instead of a placeholder message.
  `wifi_disconnect()` also clears the in-memory SSID on disconnect.
