# WiFi cleanup — Phase 5: WiFi Hotspot (deferred / optional)

- Added a capability-gated **WiFi Hotspot** entry to the **General** menu
  (`WifiSceneGeneralHotspot`), backed by a new `M1_ESP32_CAP_WIFI_HOTSPOT` bit
  and `ESP32_FEATURE_WIFI_HOTSPOT` classifier entry. No shipped AT, SiN360 or
  CD3 firmware self-reports this capability yet, so the entry shows the
  standard "Feature not supported" screen until a firmware advertises it
  (plan §3.9).
- Implemented `wifi_general_hotspot()`: raw ESP-AT SoftAP path
  (`AT+CWMODE=2` + `AT+CWSAP`) that prompts for SSID/password and offers a
  Stop/Keep choice that restores STA mode (`AT+CWMODE=1`) on exit.
- Tests: `test_esp32_feature_map.c` extended with the new mapping (24
  features) and `test_wifi_ux_restructure.c` extended with the General-menu
  entry, capability-gate and declaration assertions (34 source assertions).
