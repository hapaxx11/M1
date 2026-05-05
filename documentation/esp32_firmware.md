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

The 45-byte response payload returned by supporting ESP32 firmware:

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 1 | `proto_ver` | Must be `0x01` |
| 1 | 8 | `at_cmd_bitmap` | Standard AT command bits (little-endian `uint64_t`) |
| 9 | 4 | `ext_bitmap` | Binary-SPI extension bits (little-endian `uint32_t`) |
| 13 | 32 | `fw_name` | Null-terminated firmware identifier string |

The two bitmaps serve different roles:

- **`at_cmd_bitmap`** — each bit represents one standard ESP-AT text command that the
  firmware supports (`M1_AT_CMD_*` constants, see below).  Set the bit if the firmware
  responds to that command; leave it clear if not.
- **`ext_bitmap`** — each bit represents a binary-SPI-only extension command that has no
  equivalent in the stock AT command set (`M1_EXT_CMD_*` constants, see below).  SiN360
  and future binary-SPI firmwares use this field; AT-only firmwares leave it zero.

The STM32 calls `m1_esp32_caps_derive(at_cmd_bitmap, ext_bitmap)` internally to map
these two wire bitmaps to the high-level `M1_ESP32_CAP_*` capability bits used by the
rest of the firmware.

### Wire bits — standard AT commands (`at_cmd_bitmap`)

| Bit | Constant | AT command(s) |
|-----|----------|---------------|
| 0 | `M1_AT_CMD_AT` | `AT` — basic connectivity check |
| 1 | `M1_AT_CMD_GMR` | `AT+GMR` — firmware version |
| 2 | `M1_AT_CMD_CWMODE` | `AT+CWMODE` — WiFi station/AP/dual mode |
| 3 | `M1_AT_CMD_CWLAP` | `AT+CWLAP` — WiFi AP scan → `M1_ESP32_CAP_WIFI_SCAN` |
| 4 | `M1_AT_CMD_CWJAP` | `AT+CWJAP` — WiFi connect → `M1_ESP32_CAP_WIFI_CONNECT` |
| 5 | `M1_AT_CMD_CWQAP` | `AT+CWQAP` — WiFi disconnect |
| 6 | `M1_AT_CMD_CIPSTAMAC` | `AT+CIPSTAMAC` — query MAC address |
| 7 | `M1_AT_CMD_CWSTARTSMART` | `AT+CWSTARTSMART` — SmartConfig start |
| 8 | `M1_AT_CMD_CWSTOPSMART` | `AT+CWSTOPSMART` — SmartConfig stop |
| 9 | `M1_AT_CMD_BLEINIT` | `AT+BLEINIT` — BLE initialisation |
| 10 | `M1_AT_CMD_BLESCANPARAM` | `AT+BLESCANPARAM` — BLE scan parameters |
| 11 | `M1_AT_CMD_BLESCAN` | `AT+BLESCAN` — BLE device scan → `M1_ESP32_CAP_BLE_SCAN` |
| 12 | `M1_AT_CMD_BLEGAPADV` | `AT+BLEGAPADV` — BLE GAP advertise params → `M1_ESP32_CAP_BLE_ADV` |
| 13 | `M1_AT_CMD_BLEADVSTART` | `AT+BLEADVSTART` — start BLE advertising |
| 14 | `M1_AT_CMD_BLEADVSTOP` | `AT+BLEADVSTOP` — stop BLE advertising |
| 15 | `M1_AT_CMD_BLEGATTCPRIMSRV` | `AT+BLEGATTCPRIMSRV` — GATT primary services |
| 16 | `M1_AT_CMD_BLEGATTCCHAR` | `AT+BLEGATTCCHAR` — GATT characteristics |
| 17 | `M1_AT_CMD_BLEGATTCWR` | `AT+BLEGATTCWR` — GATT write |
| 18 | `M1_AT_CMD_BLEGATTCNTFY` | `AT+BLEGATTCNTFY` — GATT notify |
| 19-63 | — | Reserved for future use |

### Wire bits — binary-SPI extensions (`ext_bitmap`)

| Bit | Constant | Feature | High-level cap derived |
|-----|----------|---------|------------------------|
| 0 | `M1_EXT_CMD_STA_SCAN` | WiFi station discovery | `M1_ESP32_CAP_WIFI_STA_SCAN` |
| 1 | `M1_EXT_CMD_WIFI_SNIFF` | Packet monitor / sniffer | `M1_ESP32_CAP_WIFI_SNIFF` |
| 2 | `M1_EXT_CMD_WIFI_ATTACK` | Deauth, beacon spam, karma | `M1_ESP32_CAP_WIFI_ATTACK` |
| 3 | `M1_EXT_CMD_WIFI_NETSCAN` | Ping / ARP / SSH / port scan | `M1_ESP32_CAP_WIFI_NETSCAN` |
| 4 | `M1_EXT_CMD_WIFI_PORTAL` | Evil portal | `M1_ESP32_CAP_WIFI_EVIL_PORTAL` |
| 5 | `M1_EXT_CMD_BLE_SPAM` | BLE beacon spam variants | `M1_ESP32_CAP_BLE_SPAM` |
| 6 | `M1_EXT_CMD_BLE_SNIFF` | BLE packet sniffers | `M1_ESP32_CAP_BLE_SNIFF` |
| 7 | `M1_EXT_CMD_BLE_HID` | BLE HID keyboard (Bad-BT) | `M1_ESP32_CAP_BLE_HID` |
| 8 | `M1_EXT_CMD_BT_MANAGE` | BT device management | `M1_ESP32_CAP_BT_MANAGE` |
| 9 | `M1_EXT_CMD_802154` | IEEE 802.15.4 / Zigbee / Thread | `M1_ESP32_CAP_802154` |
| 10-31 | — | Reserved for future use | — |

### Capability matrix by firmware variant

| Feature | SiN360 | bedge117 C3 | neddy299 |
|---------|:------:|:-----------:|:--------:|
| WiFi AP scan | ✅ | ✅ | ✅ |
| Station scan | ✅ | — | ✅ |
| Packet sniffer | ✅ | — | — |
| Attacks (deauth/beacon/karma) | ✅ | — | ✅ |
| Network scanners | ✅ | — | — |
| Evil portal | ✅ | — | — |
| **WiFi connect / NTP** | — | ✅ | ✅ |
| BLE scan / advertise | ✅ | — | — |
| BLE spam variants | ✅ | — | — |
| BLE sniffers | ✅ | — | — |
| **Bad-BT / BLE HID** | — | ✅ | ✅ |
| **BT device management** | — | ✅ | ✅ |
| **IEEE 802.15.4** | — | ✅ | ✅ |

### Fallback behaviour (older firmware without CMD_GET_STATUS)

If the ESP32 firmware returns `RESP_ERR` or times out on `CMD_GET_STATUS`,
the M1 derives a capability bitmap from the compile-time
`M1_APP_WIFI_CONNECT_ENABLE` / `M1_APP_BADBT_ENABLE` / `M1_APP_BT_MANAGE_ENABLE`
flags.  SiN360 binary-SPI capabilities (scan, sniff, attack, BLE spam/sniff) are
always included in the fallback.  This preserves current behaviour exactly — no
feature silently disappears when the runtime handshake fails.

### Adding CMD_GET_STATUS to a custom ESP32 firmware

Respond to opcode `0x02` with a 45-byte `m1_esp32_status_payload_t` payload:

- `proto_ver = 1`
- `at_cmd_bitmap` — set the `M1_AT_CMD_*` bit for each standard AT command your
  firmware actually handles; leave all other bits clear.
- `ext_bitmap` — set the `M1_EXT_CMD_*` bit for each binary-SPI extension your
  firmware supports; leave all other bits clear.
- `fw_name` — a short null-terminated version string (e.g. `"SiN360-0.9.7"` or
  `"AT-bedge117-2.0.2"`); unused bytes are zero-padded.

The STM32 host automatically maps these two bitmaps to the internal
`M1_ESP32_CAP_*` capability bits via `m1_esp32_caps_derive()`.

> **Rule for STM32 firmware contributors:** new ESP32-dependent features MUST gate
> on a capability bit (`m1_esp32_require_cap` / `m1_esp32_has_cap`), **not** on a
> compile flag or firmware name string.  See `m1_csrc/m1_esp32_caps.h` for the API.

