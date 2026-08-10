**ESP32: native CD3 RPC dispatch fixes** — Brain CD3 firmware now routes WiFi
  station scan, probe/monitor flows, set-MAC, disconnect/shutdown, and BLE
  advertise through the M1_RPC wrappers, and exposes the host-side WiFi
  disconnect capability so the related menu actions are available.
