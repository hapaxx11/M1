**Settings Dashboard: ESP32 firmware identification** — The system page of the
  Settings Dashboard now shows the detected ESP32 firmware name (e.g.
  `ESP32 SiN360-0.9.6`) when capability data has been cached from prior ESP32
  feature use. If no cached data exists, the line prompts the user to scan WiFi
  to detect the firmware. The name is read from the caps cache without
  triggering a probe, so the dashboard repaints instantly in all cases.
