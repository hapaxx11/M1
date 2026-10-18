<!-- See COPYING.txt for license details. -->

# ESP32-C6 Coprocessor Firmware

The M1 STM32 communicates with the ESP32-C6 over SPI at runtime. The firmware
installed on the ESP32 must match the M1 firmware's transport; a mismatch makes
ESP32-backed features appear offline but does not damage either device. Firmware
images are flashed over UART.

## Hapax compatibility

Hapax supports the following ESP32 transports:

| ESP32 firmware | Transport | Status |
|----------------|-----------|--------|
| [SiN360](https://github.com/sincere360/M1_SiN360_ESP32) | 64-byte binary SPI | Supported |
| [CD3 Brain](https://github.com/bedge117/m1-esp32-brain) | M1_RPC, full-duplex 512-byte SPI cells | Supported |
| [MtkCore Legacy-SPI compatibility](https://github.com/Monstatek/MonstaTek-Esp32-Core) | M1_RPC / M1 Link | Supported where the adapter is present |
| ESP-AT-based M1 experiments | AT text over half-duplex SPI | Retired; unsupported |
| Unknown firmware | — | Fails closed; no transport is selected |

The AT-based `esp32-at-monstatek-m1` lineage, including the archived
bedge117, neddy299, dagnazty, and Hapax forks, is not supported. Do not flash
those images for Hapax. They are distinct from CD3 Brain: CD3 is a native
ESP-IDF firmware with no AT command stack. Hapax does not probe for or dispatch
AT commands.

## Stock and C3 firmware matching

The compatibility guidance in the
[C3 compatibility reference](https://github.com/bedge117/M1/blob/main/COMPATIBILITY.md)
is about genuine stock and C3 M1 firmware; those ESP32 images are not generic
substitutes for Hapax-supported images.

| Main firmware | Matching ESP32 image | Notes |
|---------------|----------------------|-------|
| Genuine Monstatek stock v0.8.0.0 or v0.8.0.2 | Monstatek factory `network_adapter` (ESP-Hosted) | Use the image supplied for that stock release. |
| Genuine Monstatek stock v0.8.0.4 | Monstatek factory `StealthHybrid` (ESP-Hosted) | Stock changed ESP32 firmware between v0.8.0.2 and v0.8.0.4. |
| C3 v0.8.0.0-C3.107 and later | [`m1-esp32-brain` v1.0.0 or later](https://github.com/bedge117/m1-esp32-brain/releases) | In qMonstatek, choose **Download latest (SPI brain)**. |
| Hapax | SiN360 or a supported M1_RPC image from the table above | Stock ESP-Hosted images are not a Hapax transport. |

The retired ESP-AT experiment is compatible with neither genuine Monstatek
stock nor current C3. If the ESP32 reports offline after changing the main
firmware, restore the matching ESP32 image for that main firmware. For genuine
stock, use qMonstatek's **Factory Restore** so the ESP image matches the stock
version; do not use an ESP-AT image.

## Transport details

- **SiN360 binary SPI:** 64-byte `CMD_*` packets, using the dedicated binary
  command path.
- **CD3 Brain M1_RPC:** full-duplex SPI, fixed 512-byte cells, `M1` magic,
  CRC-16/CCITT-FALSE, and pipelined request/response transactions. The host
  issues IDLE polls while the ESP32 prepares a response.
- **MtkCore Legacy-SPI compatibility:** the adapter uses the M1 Link/M1_RPC
  framing. Its reported feature set is limited to capabilities implemented by
  that adapter.
- **Native M1 SPI v1:** a separate MtkCore codec/client exists in the host
  source, but production transport selection does not activate it.
- **ESP-Hosted:** used by genuine stock firmware; it is not currently a
  Hapax-supported transport.

Capability reporting is used only after a supported transport is identified.
Unknown firmware profiles fail closed rather than being treated as AT firmware.
For implementation details, see `m1_csrc/esp32_feature_map.c`,
`m1_csrc/m1_esp32_caps.c`, and `m1_csrc/m1_esp32_rpc.c`.

## Flashing and recovery

Use the firmware and update workflow supplied for the main firmware you are
running. ESP32 factory images start at address `0x0`; do not substitute an image
from another firmware family. Hapax's ESP32 updater expects its supported image
format and checksum sidecar; see the updater UI and
`m1_csrc/m1_esp32_fw_update.c` for the current requirements.
