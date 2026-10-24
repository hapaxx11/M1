**ESP32: RPC-task watchdog supervision suspended around the Bluetooth Direct
  boot probe** — the multi-second ESP32 bring-up / capability probe runs on the
  watchdog-supervised RPC task and could starve its heartbeat even with yielding
  waits; supervision is now suspended around the probe and the heartbeat
  baseline reset afterward, mirroring the ESP32 flash-op pattern.
