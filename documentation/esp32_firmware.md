<!-- See COPYING.txt for license details. -->

# ESP32-C6 Coprocessor Firmware

## Overview

The Monstatek M1 uses an ESP32-C6 coprocessor for WiFi, Bluetooth, BLE, and
IEEE 802.15.4 (Zigbee/Thread) connectivity.  The STM32H573 host communicates
with the ESP32-C6 via **SPI AT commands** at runtime and uses **UART** (ROM
bootloader protocol) for firmware flashing.

Espressif's stock AT firmware downloads are **UART-only** and will **NOT** work
with the M1.  A custom SPI-configured build is required.

## Source Repository

| Repository | Description |
|-----------|-------------|
| [bedge117/esp32-at-monstatek-m1](https://github.com/bedge117/esp32-at-monstatek-m1) | **Primary** — C3 custom ESP32-C6 SPI AT firmware for M1 |
| [neddy299/esp32-at-monstatek-m1](https://github.com/neddy299/esp32-at-monstatek-m1) | Fork with WiFi deauthentication (`AT+DEAUTH`, `AT+STASCAN`) |
| [dagnazty/esp32-at-monstatek-m1](https://github.com/dagnazty/esp32-at-monstatek-m1) | Fork (dag) — additional AT command extensions |

Both repos are forks of Espressif's official
[esp-at](https://github.com/espressif/esp-at) project, customised for the M1's
SPI transport and pin mapping.

## Releases

### bedge117 (C3) releases

| Version | Features |
|---------|----------|
| **v1.0.0** | Base SPI AT firmware — correct SPI pin mapping, status register offset fix, USB Serial/JTAG disabled, SPI Mode 1 |
| **v2.0.2** | + BLE HID keyboard (`AT+BLEHIDINIT` / `AT+BLEHIDKB`), + IEEE 802.15.4 sniffer (`AT+ZIGSNIFF`) with Thread frame version filter fix |

### neddy299 (Deauth) releases

| Version | Features |
|---------|----------|
| **v1.0.1** | WiFi deauthentication (`AT+DEAUTH`), station scanning (`AT+STASCAN`) |

### dagnazty (dag) releases

| Version | Features |
|---------|----------|
| See [GitHub Releases](https://github.com/dagnazty/esp32-at-monstatek-m1/releases) | Additional AT command extensions on top of bedge117 base |

## Custom AT Commands

### Base (v1.0.0)

Standard Espressif AT command set with SPI transport fixes.  No custom
commands — all standard WiFi, BLE, and TCP/IP AT commands work.

### BLE HID (v2.0.0+)

| Command | Description |
|---------|-------------|
| `AT+BLEHIDINIT` | Configure BLE HID keyboard appearance and GATT services |
| `AT+BLEHIDKB=<modifier>,<key1>,...,<key6>` | Send HID keyboard report |

Used by Bad-BT (Bluetooth keystroke injection) on M1.

### IEEE 802.15.4 Sniffer (v2.0.1+)

| Command | Description |
|---------|-------------|
| `AT+ZIGSNIFF=1,<channel>` | Start promiscuous 802.15.4 sniffing |
| `AT+ZIGSNIFF=0` | Stop sniffing |
| `AT+ZIGSNIFF?` | Query sniffer status |

Outputs `+ZIGFRAME:` unsolicited responses with parsed MAC headers.
Protocol classification: Zigbee (Z), Thread (T), Unknown (U).

### WiFi Deauthentication (neddy299 v1.0.1)

| Command | Description |
|---------|-------------|
| `AT+DEAUTH=<mode>,<ch>,"<sta_mac>","<bssid>"` | Start deauth attack |
| `AT+DEAUTH=0` | Stop deauth attack |
| `AT+DEAUTH?` | Query status → `+DEAUTH:(<active>,<ch>,<mode>,<num>)` |
| `AT+STASCAN=1,<ch>,"<bssid>"` | Start station scan on AP |
| `AT+STASCAN=0` | Stop station scan |
| `AT+STASCAN?` | Query results → `+STASCAN:("<mac>")` per station |

## Key Customisations vs Stock esp-at

1. **SPI pin mapping** for M1 hardware (stock defaults are wrong)
2. **Status register offset fix** — slave writes transmit status to shared
   buffer offset 0 (where M1 master reads) in addition to offset 4
3. **USB Serial/JTAG disabled** — prevents GPIO 12/13 interference with SPI
   MOSI/MISO
4. **SPI Mode 1** (CPOL=0, CPHA=1) matching M1's STM32 SPI master

## SPI Pin Mapping

| Signal | ESP32-C6 GPIO |
|--------|---------------|
| SCLK | 7 |
| MOSI | 12 |
| MISO | 13 |
| HANDSHAKE | 14 |
| CS | 15 |

## Firmware Image Format

| File | Description | Flash offset |
|------|-------------|-------------|
| `factory_ESP32C6-SPI.bin` | Full factory image (bootloader + partition table + app) | `0x000000` |
| `factory_ESP32C6-SPI.md5` | MD5 checksum — **uppercase hex, exactly 32 bytes, no newline** | — |
| `esp-at.bin` | Application partition only (for partial updates) | `0x060000` |

The factory image is the recommended format for M1 flashing.

## Flashing Methods

### Method 1: Via M1's ESP32 Update Menu (SD Card)

1. Place `factory_ESP32C6-SPI.bin` and `factory_ESP32C6-SPI.md5` on the M1's
   SD card (any directory).
2. Navigate to **Settings → ESP32 update → Image File** and select the `.bin`.
3. Set **Start Address** to `0x000000` (factory image includes bootloader).
4. Select **Firmware Update** to begin flashing.
5. Battery must be ≥ 50%.

The M1 puts the ESP32-C6 into ROM bootloader mode by holding IO9 (GPIO 9) low
during reset, then flashes via UART at 921600 baud using the esp-serial-flasher
library.

> **Note:** WiFi is unavailable during flashing because the ESP32 is in
> bootloader mode.  The OTA download feature downloads firmware to SD card
> first, then the user flashes from SD card as a separate step.

### Method 2: Via M1's ESP32 Update → Download (OTA)

1. Connect to WiFi via **WiFi → Scan + Connect**.
2. Navigate to **Settings → ESP32 update → Download**.
3. Select a firmware source (C3 ESP32 AT, Deauth ESP32 AT).
4. Select a release version.
5. The firmware `.bin` and `.md5` are downloaded to `0:/ESP32_FW/` on SD card.
6. Return to **ESP32 update → Image File** to select and flash.

### Method 3: Via esptool (External, USB/UART)

```bash
python -m esptool --chip esp32c6 --port <PORT> --baud 460800 \
    write_flash 0x0 factory_ESP32C6-SPI.bin
```

### Method 4: Via qMonstatek Desktop App

The [qMonstatek](https://github.com/bedge117/qMonstatek) companion app supports
DFU flashing over USB.

## Building from Source

Requires ESP-IDF v5.1.2.  Clone the source repo, run `install.sh`/`install.bat`,
then build with module `ESP32C6-SPI`.

```bash
# Set environment
export IDF_PATH=<repo>/esp-idf
export ESP_AT_PROJECT_PLATFORM=PLATFORM_ESP32C6
export ESP_AT_MODULE_NAME=ESP32C6-SPI

# Build
python $IDF_PATH/tools/idf.py -DIDF_TARGET=esp32c6 build
```

The factory image is generated at `build/factory/factory_ESP32C6-SPI.bin`.

Generate the MD5 sidecar:
```python
import hashlib
with open('build/factory/factory_ESP32C6-SPI.bin', 'rb') as f:
    md5 = hashlib.md5(f.read()).hexdigest().upper()
with open('build/factory/factory_ESP32C6-SPI.md5', 'wb') as f:
    f.write(md5.encode('ascii'))
```

See `CLAUDE.md` § "ESP32 Build — How to Build from Claude Code" for the
Windows/PowerShell build procedure.

---

## Runtime Capability Detection

Starting from Hapax v0.9.0 (firmware build that includes `m1_esp32_caps.c`),
the M1 queries the connected ESP32 firmware for its capability descriptor at
first use via the `CMD_GET_STATUS` (opcode `0x02`) SPI command.

### CMD_GET_STATUS payload format (protocol version 1)

The 41-byte response payload returned by supporting ESP32 firmware:

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 1 | `proto_ver` | Must be `0x01` |
| 1 | 8 | `cap_bitmap` | `M1_ESP32_CAP_*` capability bits (little-endian `uint64_t`) |
| 9 | 32 | `fw_name` | Null-terminated firmware identifier string |

The `cap_bitmap` field carries `M1_ESP32_CAP_*` bits **directly** — no derivation
step is needed on the STM32 side.  Any firmware variant, whether it implements
features through binary-SPI extensions or AT text commands, sets the same bit for
the same capability.  This means a feature always has exactly one flag position,
regardless of how the ESP32 implements it internally.

### Wire bits — `cap_bitmap`

Set the bit for each feature your firmware supports; leave all other bits clear.

| Bit | `M1_ESP32_CAP_*` constant | Feature |
|-----|--------------------------|---------|
| 0 | `M1_ESP32_CAP_WIFI_SCAN` | WiFi AP scan |
| 1 | `M1_ESP32_CAP_WIFI_STA_SCAN` | WiFi station discovery |
| 2 | `M1_ESP32_CAP_WIFI_SNIFF` | Packet monitor / sniffer |
| 3 | `M1_ESP32_CAP_WIFI_ATTACK` | Deauth, beacon spam, karma, evil portal attacks |
| 4 | `M1_ESP32_CAP_WIFI_NETSCAN` | Ping / ARP / SSH / port scan |
| 5 | `M1_ESP32_CAP_WIFI_EVIL_PORTAL` | Evil portal |
| 6 | `M1_ESP32_CAP_WIFI_CONNECT` | WiFi station connect / NTP sync |
| 7 | `M1_ESP32_CAP_BLE_SCAN` | BLE device scan |
| 8 | `M1_ESP32_CAP_BLE_ADV` | BLE advertising (GAP) |
| 9 | `M1_ESP32_CAP_BLE_SPAM` | BLE beacon spam variants |
| 10 | `M1_ESP32_CAP_BLE_SNIFF` | BLE packet sniffers |
| 11 | `M1_ESP32_CAP_BLE_HID` | BLE HID keyboard (Bad-BT) |
| 12 | `M1_ESP32_CAP_BT_MANAGE` | Classic BT device management |
| 13 | `M1_ESP32_CAP_802154` | IEEE 802.15.4 / Zigbee / Thread |
| 14-63 | — | Reserved for future use |

### Capability matrix by firmware variant

Both SiN360 and AT firmware variants with the `feat/cmd_get_status` extension
respond to one of the two capability probes and self-report their capabilities.
The table below shows the expected `cap_bitmap` for each variant.  AT firmware
**without** `feat/cmd_get_status` triggers the fallback path (see below).

| Feature | SiN360 | bedge117 / dag | neddy299 |
|---------|:------:|:--------------:|:--------:|
| WiFi AP scan | ✅ | — | — |
| Station scan | ✅ | — | ✅ |
| Packet sniffer | ✅ | — | — |
| Attacks (deauth/beacon/karma) | ✅ | — | ✅ |
| Network scanners | ✅ | — | — |
| Evil portal | ✅ | — | — |
| WiFi connect / NTP | — | — | — |
| BLE scan / advertise | ✅ | — | — |
| BLE spam variants | ✅ | — | — |
| BLE sniffers | ✅ | — | — |
| **Bad-BT / BLE HID** | — | ✅ | ✅ |
| **IEEE 802.15.4** | — | ✅ | ✅ |
| Classic BT management | — | — | — |

> **Note:** WiFi connect and classic BT management use binary SPI commands in
> the M1 firmware (`m1_wifi.c`, `m1_bt.c`) that AT firmware does not handle.
> Those features will be restored once the implementation is ported to the AT
> command layer.

### Probe sequence and fallback behaviour

When the M1 initialises the ESP32, it performs a two-step capability probe:

1. **Binary CMD_GET_STATUS** (opcode `0x02`): tried first.  SiN360 binary-SPI
   firmware always responds.  AT firmware does not understand binary opcodes and
   will time out or return `RESP_ERR`.

2. **AT+GETSTATUSHEX** (AT text command): tried only when step 1 fails and the AT
   task (`get_esp32_main_init_status()`) is active.  AT firmware variants that
   implement the `feat/cmd_get_status` extension will respond here with a
   hex-encoded capability payload.

3. **Fallback** (`M1_ESP32_CAP_PROFILE_AT_BEDGE117`): applied when both steps 1
   and 2 fail.  This ensures Bad-BT and IEEE 802.15.4 remain accessible on AT
   firmware variants without the `feat/cmd_get_status` extension (bedge117 ≤
   v2.0.2, neddy299 ≤ v1.0.1, dag pre-AT-Custom-Status).

The capability cache is reset by `m1_esp32_caps_reset()` (called from
`m1_esp32_deinit()`) so the next `m1_esp32_init()` / `esp32_main_init()` cycle
re-queries the connected firmware.

### Adding CMD_GET_STATUS to a custom ESP32 firmware

Respond to opcode `0x02` with a 41-byte `m1_esp32_status_payload_t` payload:

- `proto_ver = 1`
- `cap_bitmap` — set the `M1_ESP32_CAP_*` bit for each feature your firmware
  supports; leave all other bits clear.  Use the same bit position for the same
  feature regardless of whether your implementation uses AT text commands or
  binary SPI extensions internally.
- `fw_name` — a short null-terminated version string (e.g. `"SiN360-0.9.7"` or
  `"AT-bedge117-2.0.2"`); unused bytes are zero-padded.

> **Rule for STM32 firmware contributors:** new ESP32-dependent features MUST gate
> on a capability bit (`m1_esp32_require_cap` / `m1_esp32_has_cap`), **not** on a
> compile flag or firmware name string.  See `m1_csrc/m1_esp32_caps.h` for the API.

### AT-Custom-Status (`AT+GETSTATUSHEX`) — alternative for AT firmware

AT-based firmware variants cannot respond to binary SPI opcodes.  Instead, AT
firmware that implements the `feat/cmd_get_status` extension provides the same
capability information via an AT text command:

```
AT+GETSTATUSHEX
+GETSTATUSHEX:01<16-hex-chars-cap_bitmap><64-hex-chars-fw_name>
OK
```

The M1 automatically tries `AT+GETSTATUSHEX` as a secondary probe when the binary
`CMD_GET_STATUS` request fails.  The probe uses `spi_AT_send_recv()` to send the
AT command and `m1_esp32_caps_parse_at_hex()` (a pure-logic header helper) to
decode the hex response into the standard 41-byte `m1_esp32_status_payload_t`
layout, which is then parsed by the same `m1_esp32_caps_parse_payload()` used for
binary responses.

**AT+GETSTATUSHEX response format:**

The hex string encodes the same 41-byte structure as the binary payload:

| Hex chars | Field | Description |
|-----------|-------|-------------|
| `[0..1]` | `proto_ver` | Must be `01` (1 byte) |
| `[2..17]` | `cap_bitmap` | `M1_ESP32_CAP_*` bits, little-endian `uint64_t` (8 bytes) |
| `[18..81]` | `fw_name` | Firmware identifier, null-padded to 32 bytes |

Both uppercase and lowercase hex characters are accepted by the M1 parser.

**For AT firmware authors:** implement `AT+GETSTATUSHEX` to return a
hex-encoded `m1_esp32_status_payload_t`.  Set `cap_bitmap` to a `uint64_t`
little-endian using the same `M1_ESP32_CAP_*` bit positions as the binary
firmware.  Refer to the `at_custom_status.c` in the `feat/cmd_get_status` branch
of [hapaxx11/esp32-at-monstatek-m1](https://github.com/hapaxx11/esp32-at-monstatek-m1)
as a reference implementation (note: update the cap_bitmap field from `uint32_t`
to `uint64_t` to match protocol version 1).

> **Rule for STM32 firmware contributors:** new ESP32-dependent features MUST gate
> on a capability bit (`m1_esp32_require_cap` / `m1_esp32_has_cap`), **not** on a
> compile flag or firmware name string.  See `m1_csrc/m1_esp32_caps.h` for the API.

