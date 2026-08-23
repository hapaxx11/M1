**ESP32: brain-CD3 M1_RPC probe raced the host AT task on SPI3 (issue #719 Phase 1)** —
On-device diagnostics (added previously) showed the host AT task
(`spi_trans_control_task`) already running whenever the brain-CD3 M1_RPC PING
was attempted, and the full-duplex M1 Link transfer never took the shared
SPI3 mutex, so the two could corrupt each other's framing and the brain was
permanently misdetected as `ESP32 Unknown (fallback)` with an all-zero
capability bitmap. The M1_RPC brain probe now runs *before* the host AT task
is started, so a brain-only device is probed without ever touching the AT
task, and `m1link_hal_xfer()` now takes the same SPI3 mutex the AT task uses
whenever that task exists, so the two transports can no longer overlap even
if the AT task was already started by an earlier feature.
