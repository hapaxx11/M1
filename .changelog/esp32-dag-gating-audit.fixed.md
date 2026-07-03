**WiFi/Bluetooth: comprehensive ESP32 capability-gating audit for dag T-800 firmware** — Following
  the Station Scan "start failed" fix, audited every remaining ESP32-dependent scene delegate for
  proper capability gating. Added `DELEGATE_FEATURE` gating (matching the existing Bad-BT/BLE-HID
  pattern) to: WiFi Sniffers (All/Beacon/Probe/Deauth/EAPOL/Pwnagotchi/SAE), MAC Track, Signal
  Monitor (`ESP32_FEATURE_PKTMON`); 802.15.4 Zigbee/Thread scan (`ESP32_FEATURE_802154`); Net Scan
  Ping/ARP/SSH/Telnet/Port (`ESP32_FEATURE_NETSCAN`); Bluetooth BLE Scan, BLE Advertise
  (`ESP32_FEATURE_BLE_SCAN`/`ESP32_FEATURE_BLE_ADV`); and BLE Sniffers Analyzer/Generic/Flipper/
  AirTag (`ESP32_FEATURE_BLE_SCAN`). Features that already implement a dual AT/binary-SPI dispatch
  path (deauth, beacon, karma, evil portal, probe flood, AP clone, rickroll, PMKID grab, Networks/
  Survey/Wardrive AP scan, all BLE Spam variants) and pure local placeholder stubs (AirTag Monitor,
  BLE Wardrive, BLE Detectors, AirTag Spoof, BT Config) were confirmed correct and left unchanged.
