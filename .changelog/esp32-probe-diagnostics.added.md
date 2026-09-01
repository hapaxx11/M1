**Settings Dashboard: ESP32 probe diagnostics** — A new Dashboard page (4/5)
shows the result of the last ESP32 firmware detection probe (which stage
resolved, the M1_RPC PING transport return code and byte count, the resolved
capability bitmap, and whether the host AT task was running at probe time). This
makes the "ESP32 Unknown (fallback)" failure (issue #719) observable on-device
without a debugger.
