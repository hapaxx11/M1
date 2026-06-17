**WiFi: PMKID Grab via dag T-800 AT path** — new "PMKID Grab" entry in the WiFi Attacks menu
  (WifiSceneAttackPmkidAt). Uses AT+CWLAP to scan visible APs, presents a scrollable one-at-a-time
  selection list, then sends AT+M1PMKID to solicit the PMKID from the chosen AP (dag T-800 firmware
  only). On success, saves to `pmkid/captures.22000` in Hashcat WPA*01 format via the existing
  `pmkid_save_to_sd()` helper. Gated on M1_ESP32_CAP_BEACON (T-800 fingerprint bit).
  Pure-logic AT parsing extracted into `wifi_at_scan.c/h` with 23 Unity tests.
