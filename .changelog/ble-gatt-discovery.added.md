**Bluetooth: GATT Discovery** — Imported the SiN360 NimBLE GATT client as a
  new Bluetooth scene (`BtSceneGattDiscovery`).  Scans for nearby BLE peers,
  connects to a chosen device, enumerates services / characteristics /
  descriptors, and exposes per-characteristic tools: read details, write text
  or hex values (with response or without), and subscribe to notifications or
  indications.  Discovery results are exported to `0:/bt/gatt.csv` and
  notification streams to `0:/bt/gatt_notify.csv`.  Runtime-gated by the new
  `M1_ESP32_CAP_BLE_GATT` capability bit, which is reported by SiN360 firmware
  via `CMD_GET_STATUS`; on AT/legacy ESP32 firmware the menu item shows the
  standard "not supported" screen and pops immediately.
