# Phase Checklist — SiN360 Binary SPI WiFi/BLE Integration

## PR Metadata
- **PR Title**: `feat: integrate SiN360 v0.9.0.6/0.7 binary SPI WiFi and BLE subsystem`
- **PR Description**: Port sincere360/M1_SiN360 v0.9.0.6/0.7 WiFi and BLE features (binary SPI command protocol, WiFi sniffer/attack tools, BLE wardrive/spam/detect, evil portal, network scanners, MAC tracker, wardrive) into Hapax. Replaces the AT-command WiFi/BLE implementation with a new binary 64-byte SPI packet protocol (`m1_esp32_cmd.c/h`) that pairs with the sincere360/M1_SiN360_ESP32 companion firmware. AT-dependent features (NTP sync, WiFi connect, BT device management) are gated by existing compile flags and remain compilable but dormant until equivalent support is added to the ESP32 firmware.

## Phases

### Phase 1 — Add `m1_esp32_cmd.c/h` (binary SPI command layer)
- **Description**: Add the new `m1_esp32_cmd.c` and `m1_esp32_cmd.h` files from sincere360. These provide `m1_esp32_send_cmd()` and `m1_esp32_simple_cmd()` — a 64-byte binary packet API over the existing SPI3 hardware. No existing files are modified in this phase.
- **Status**: ✅ Complete
- **Commit**: `feat: add m1_esp32_cmd binary SPI command layer from SiN360`

### Phase 2 — Replace `m1_wifi.c` + `m1_wifi.h`
- **Description**: Replace both files with sincere360's binary SPI versions. Remove AT-layer includes (`ctrl_api.h`, `esp_app_main.h`, `m1_wifi_cred.h`, `m1_clock_util.h`). Retain legacy AT-based functions (`wifi_is_connected`, `wifi_sync_rtc`, `wifi_ntp_background_sync`, `wifi_saved_networks`, `wifi_show_status`, `wifi_disconnect`) as stubs behind `M1_APP_WIFI_CONNECT_ENABLE` so callers in `m1_menu.c`, `m1_system.c`, `m1_http_client.c`, `m1_app_api.c` still compile.
- **Status**: ✅ Complete
- **Commit**: `feat: replace m1_wifi with SiN360 binary SPI version`

### Phase 3 — Replace `m1_bt.c` + `m1_bt.h`
- **Description**: Replace both files with sincere360's binary SPI versions. Remove AT-layer includes. Retain legacy function stubs for `bluetooth_saved_devices()`, `bluetooth_info()`, `bluetooth_set_badbt_name()` behind `M1_APP_BT_MANAGE_ENABLE` / `M1_APP_BADBT_ENABLE` so `m1_bt_scene.c` continues to compile. The new `m1_bt.c` includes its own BLE spam (sour apple/SwiftPair/Samsung/Flipper) via binary SPI — these become the primary spam implementations.
- **Status**: ✅ Complete
- **Commit**: `feat: replace m1_bt with SiN360 binary SPI version`

### Phase 4 — Update `m1_wifi_scene.c` for new WiFi menus
- **Description**: Expand the WiFi scene menu to expose the new tools: Sniffers submenu (All/Beacon/Probe/Deauth/EAPOL/SAE/Pwnagotchi), Signal Monitor, Station Scan, Attacks submenu (Deauth/Beacon/Clone/Rickroll/Evil Portal/Probe Flood/Karma/Karma+Portal), Network Scanners submenu (Ping/ARP/SSH/Telnet/Ports), MAC Track, Wardrive, Station Wardrive, General Tools submenu. Remove the old AT-based "Deauther" scene (replace with new `wifi_attack_deauth()`).
- **Status**: ✅ Complete
- **Commit**: `feat: expand wifi scene menu with SiN360 WiFi tools`

### Phase 5 — Update `m1_bt_scene.c` for new BLE menus
- **Description**: Expand the BT scene menu to expose: BLE Scan, BLE Config, BLE Sniffers submenu (Analyzer/Generic/Flipper/AirTag/Monitor/Flock), BLE Wardrive (regular + continuous), BLE Detectors (Skimmers/Flock/Meta/AirTag), BLE Spam (Sour Apple/SwiftPair/Samsung/Flipper/All/AirTag Spoof). Replace the `m1_ble_spam.c`-based "BLE Spam" scene with the new built-in spam functions from `m1_bt.c`. Keep Bad-BT (uses HID, separate from BLE advertising).
- **Status**: ✅ Complete
- **Commit**: `feat: expand bt scene menu with SiN360 BLE tools`

### Phase 6 — Update `cmake/m1_01/CMakeLists.txt`
- **Description**: Add `m1_esp32_cmd.c` to the build. Note: `m1_deauth.c` and `m1_ble_spam.c` remain in cmake for now (they compile cleanly as long as their headers are present); they become effectively dead code when the new wifi/bt scenes no longer call them.
- **Status**: ✅ Complete
- **Commit**: `build: add m1_esp32_cmd.c to cmake build`

### Phase 7 — Update CLAUDE.md forks tracker + changelog
- **Description**: Update the sincere360/M1_SiN360 row to reflect v0.9.0.7, add the new companion ESP32 firmware repo `sincere360/M1_SiN360_ESP32`, add an integration note to the CLAUDE.md ESP32 section, and add a changelog fragment.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_
