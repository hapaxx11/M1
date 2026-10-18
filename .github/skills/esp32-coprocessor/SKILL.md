---
name: esp32-coprocessor
description: ESP32-C6 coprocessor transports, supported firmware, capability probing, and lifecycle rules. Load for WiFi, Bluetooth, BLE, 802.15.4, or ESP32 work.
---

# ESP32-C6 Coprocessor

See [documentation/esp32_firmware.md](../../../documentation/esp32_firmware.md)
for the supported firmware list and stock/C3 compatibility matrix.

## Supported transports

Hapax supports the SiN360 64-byte binary-SPI protocol, CD3 Brain's full-duplex
M1_RPC/M1 Link, and MtkCore's Legacy-SPI compatibility adapter where present.
ESP-AT-based M1 experiments are retired and unsupported. Do not add AT probing,
AT command dispatch, or an AT fallback for unknown firmware. Unknown firmware
must fail closed.

Genuine Monstatek stock firmware uses a version-matched ESP-Hosted image, not
one of Hapax's supported transports. C3 v0.8.0.0-C3.107 and later uses
`m1-esp32-brain` v1.0.0 or later. The older `esp32-at-monstatek-m1`
experiment is incompatible with both genuine stock and current C3.

## Transport selection and feature routing

- `esp32_firmware_transport()` identifies supported profiles from the
  capability bitmap: MtkCore Legacy-SPI and CD3 Brain map to RPC; SiN360 maps
  to binary SPI; unknown profiles map to `ESP32_TRANSPORT_NONE`.
- Feature code must check the active supported transport and use its matching
  implementation. Never use a generic `else` branch for AT; explicitly handle
  RPC and binary-SPI, and reject unsupported transports.
- The native M1 SPI v1 codec/client is host-side only. Production transport
  selection does not activate it.
- `m1_esp32_rpc_features.c/.h` provides feature-level RPC wrappers on top of
  `m1_esp32_rpc.c/.h`; the framing and feature payload logic are host-tested.
- CD3 Brain uses full-duplex 512-byte M1 Link cells and a pipelined response:
  the host sends IDLE filler transactions while waiting for the matching
  response. `spi_m1link_send_recv_bin()` in `esp_app_main.c` supplies the
  on-target SPI primitive. Preserve this path when changing SPI lifecycle code.

## ESP32 lifecycle

`m1_esp32_init()` owns STM32-side SPI, GPIO, and interrupt setup.
`m1_esp32_deinit()` tears down that hardware and is required on every exit from
a blocking delegate that initialized the ESP32.

Do not revive `esp32_main_init()` for AT task creation. If a code path requires
capability detection, ensure the HAL is initialized before probing. Any
blocking delegate using the ESP32 must call `m1_esp32_deinit()` on all exit
paths, including initialization failures.

Background ESP32 operations may hold transport ownership across calls. Follow
the existing owner acquire/release pattern and do not deinitialize SPI while a
background owner is active. See `m1_esp32_hal.c` and
`m1_esp32_rpc.c` for the lifecycle and transaction locks.

## Adding or changing a supported feature

1. Confirm the operation exists on the target firmware using repository source
   or an authoritative protocol definition; do not infer support from a
   firmware name alone.
2. Add or update its capability mapping and feature-level RPC/binary-SPI
   implementation. Unsupported transports must not fall through to a different
   wire protocol.
3. Add host-side tests for the pure protocol/dispatch behavior under `tests/`.
4. Build the firmware after source changes and inspect the linker RAM summary.

For host-tested M1 Link, RPC payload, and capability details, see
`tests/test_esp32_m1link.c`, `tests/test_esp32_rpc.c`, and
`tests/test_esp32_caps.c`.
