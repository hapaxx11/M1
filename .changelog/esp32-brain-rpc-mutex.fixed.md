**ESP32: brain CD3 (m1-native) WiFi scan and all M1_RPC features broken** — the
  M1 Link SPI mutex was never initialised (its init function had no call site), so
  every M1 Link transfer failed before clocking a byte and AP scans always ended
  with "AP scan failed". The lock now creates the mutex lazily on first use.
