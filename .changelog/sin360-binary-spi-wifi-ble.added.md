**WiFi & Bluetooth: SiN360 binary SPI subsystem** — Ported sincere360/M1_SiN360
  v0.9.0.6/0.7 WiFi and BLE features using a new 64-byte binary SPI command protocol
  (`m1_esp32_cmd.c/h`). Requires the `sincere360/M1_SiN360_ESP32` ESP32 companion
  firmware (flash via OTA updater or esptool, no hardware changes).

  **New WiFi tools** (via WiFi → Sniffers / Attacks / Net Scan / General):
  - Packet sniffers: All, Beacon, Probe, Deauth, EAPOL, SAE/WPA3, Pwnagotchi
  - Signal Monitor, Station Scan, MAC Track, Wardrive, Station Wardrive
  - Attacks: Deauth, Beacon Spam, AP Clone, Rickroll, Evil Portal, Probe Flood, Karma, Karma+Portal
  - Network scanners: Ping, ARP, SSH, Telnet, Port Scan
  - General config: join WiFi, set MACs/channel, EP SSID/HTML, save/load/clear AP lists

  **New BLE tools** (via Bluetooth → Sniffers / Spam / Wardrive / Detectors):
  - BLE Sniffers: Analyzer, Generic, Flipper, AirTag Sniff/Monitor, Flock
  - BLE Wardrive (regular, continuous, Flock), BLE Spam (Sour Apple, SwiftPair,
    Samsung, Flipper, All, AirTag Spoof), Detectors (Skimmers, Flock, Meta)
  - BLE Advertise and BLE Config retained; Bad-BT (HID) unchanged

  WiFi connect, NTP sync, and Bad-BT show a "not available with SiN360 FW"
  message — these will be restored once the SiN360 ESP32 firmware adds support.
