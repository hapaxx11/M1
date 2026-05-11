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
| [hapaxx11/esp32-at-monstatek-m1](https://github.com/hapaxx11/esp32-at-monstatek-m1) | Fork of neddy299 — adds `CMD_GET_STATUS` binary capability probe (eliminates `AT+CMD?` timeout) |

All repos are forks of Espressif's official
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

### hapaxx11 (neddy299 fork, CMD_GET_STATUS) releases

| Version | Features |
|---------|----------|
| **v1.0.1-caps** | All neddy299 v1.0.1 features + `CMD_GET_STATUS` binary opcode (opcode `0x02`) — self-reports `cap_bitmap = 0x14412` (`STA_SCAN` \| `DEAUTH` \| `WIFI_JOIN` \| `BLE_HID` \| `802154`), `fw_name = "AT-neddy299-1.0.1"` |

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

The `cap_bitmap` field carries `M1_ESP32_CAP_*` bits **directly** — each bit
corresponds to a named feature, regardless of how the firmware implements it
internally (binary SPI commands or AT text commands).

### Wire bits — `cap_bitmap`

Set the bit for each capability your firmware supports; leave all other bits clear.

| Bit | `M1_ESP32_CAP_*` constant | Commands covered |
|-----|--------------------------|-----------------|
| 0 | `M1_ESP32_CAP_WIFI_SCAN` | `CMD_WIFI_SCAN_START/NEXT/STOP` |
| 1 | `M1_ESP32_CAP_STA_SCAN` | `CMD_STA_SCAN_START/NEXT/STOP` |
| 2 | `M1_ESP32_CAP_BLE_SCAN` | `CMD_BLE_SCAN_START/NEXT/STOP/NEXT_RAW` |
| 3 | `M1_ESP32_CAP_BLE_ADV` | `CMD_BLE_ADV_START/STOP/RAW` |
| 4 | `M1_ESP32_CAP_DEAUTH` | `CMD_DEAUTH_START/STOP/MULTI` |
| 5 | `M1_ESP32_CAP_BEACON` | `CMD_BEACON_START/STOP/SET_FLAGS` |
| 6 | `M1_ESP32_CAP_PROBE_FLOOD` | `CMD_PROBE_FLOOD_START/STOP` |
| 7 | `M1_ESP32_CAP_KARMA` | `CMD_KARMA_START/STOP/STATUS/PORTAL_START` |
| 8 | `M1_ESP32_CAP_PKTMON` | `CMD_PKTMON_START/NEXT/STOP/SET_CHAN` |
| 9 | `M1_ESP32_CAP_PORTAL` | `CMD_PORTAL_*` + `CMD_SSID_*` |
| 10 | `M1_ESP32_CAP_WIFI_JOIN` | `CMD_WIFI_JOIN/DISCONNECT` |
| 11 | `M1_ESP32_CAP_WIFI_SET_MAC` | `CMD_WIFI_SET_MAC` |
| 12 | `M1_ESP32_CAP_WIFI_SET_CHAN` | `CMD_WIFI_SET_CHANNEL` |
| 13 | `M1_ESP32_CAP_NETSCAN` | `CMD_NETSCAN_START/NEXT/STOP` |
| 14 | `M1_ESP32_CAP_BLE_HID` | AT BLE HID keyboard (Bad-BT) |
| 15 | `M1_ESP32_CAP_BT_MANAGE` | AT BT device management |
| 16 | `M1_ESP32_CAP_802154` | AT IEEE 802.15.4 / Zigbee / Thread |
| 17-63 | — | Reserved for future use |

### Capability matrix by firmware variant

Both SiN360 (via `CMD_GET_STATUS`) and AT firmware (via the stock `AT+CMD?`
listing) self-report their capabilities at runtime.  The table below shows the
expected `cap_bitmap` for each tracked variant.  The hapaxx11 fork of neddy299
implements `CMD_GET_STATUS` directly, so it self-reports via probe 1 and the
`AT+CMD?` fallback (probe 2) is not needed for it.

| Command family | SiN360 | bedge117 / dag | neddy299 (hapaxx11 fork) |
|----------------|:------:|:--------------:|:------------------------:|
| WiFi AP scan | ✅ | — | — |
| Station scan | ✅ | — | ✅ |
| Packet monitor | ✅ | — | — |
| Deauth | ✅ | — | ✅ |
| Beacon spam | ✅ | — | — |
| Probe flood | ✅ | — | — |
| Karma | ✅ | — | — |
| Portal | ✅ | — | — |
| Network scanners | ✅ | — | — |
| WiFi join/disconnect | — | ✅ | ✅ |
| BLE scan / advertise | ✅ | — | — |
| **BLE HID (Bad-BT)** | — | ✅ | ✅ |
| **IEEE 802.15.4** | — | ✅ | ✅ |
| Classic BT management | — | — | — |

AT capability mapping audit (tracked firmware set): the AT commands currently
mapped to capability bits by the runtime `AT+CMD?` probe are `AT+CWJAP`
(WiFi join), `AT+BLEHIDINIT` (BLE HID), `AT+ZIGSNIFF` (802.15.4),
`AT+DEAUTH` (deauth), and `AT+STASCAN` (station scan).  Adding a new
mapping is a single-line edit to `s_at_cmd_cap_map[]` in
`m1_csrc/m1_esp32_caps.c`.

### Probe sequence

When the M1 initialises the ESP32, it performs a two-step capability probe:

1. **Binary CMD_GET_STATUS** (opcode `0x02`): tried first.  SiN360 binary-SPI
   firmware and AT-based firmware that implements the binary extension (e.g.
   `hapaxx11/esp32-at-monstatek-m1`, a fork of neddy299) respond here with the
   41-byte capability payload.  Unextended AT firmware (bedge117, dag, stock
   neddy299) does not implement this opcode and will time out or return
   `RESP_ERR`.

2. **Stock `AT+CMD?`** (AT text command): tried only when step 1 fails — i.e.
   for AT firmware that does not implement the binary extension (bedge117, dag)
   and the AT task (`get_esp32_main_init_status()`) is active.  `AT+CMD?` is
   part of the standard ESP-AT command set
   ([reference](https://docs.espressif.com/projects/esp-at/en/latest/esp32/AT_Command_Set/Basic_AT_Commands.html#at-cmd))
   and is supported unchanged by every tracked AT firmware variant.

   The response lists every AT command the firmware understands.  A small
   mapping table on the STM32 (`s_at_cmd_cap_map[]` in `m1_esp32_caps.c`)
   translates the presence of specific commands into `M1_ESP32_CAP_*` bits:

   | AT command | Capability bit |
   |------------|----------------|
   | `AT+CWJAP` | `M1_ESP32_CAP_WIFI_JOIN` |
   | `AT+BLEHIDINIT` | `M1_ESP32_CAP_BLE_HID` |
   | `AT+ZIGSNIFF` | `M1_ESP32_CAP_802154` |
   | `AT+DEAUTH` | `M1_ESP32_CAP_DEAUTH` |
   | `AT+STASCAN` | `M1_ESP32_CAP_STA_SCAN` |

   Adding support for a new AT-side feature is a single-line edit to this
   table — no curated fallback profile macros are required.

3. **Fail-closed default**: applied when both steps 1 and 2 fail.  The
   capability bitmap is left at zero and the firmware name is reported as
   `"Unknown (fallback)"`.  Feature gates that check specific
   `M1_ESP32_CAP_*` bits will all return false and the "Feature not
   supported" UI will appear.  Granting capabilities we cannot verify risks
   crashing on firmware that does not implement the underlying command.

The capability cache persists for the lifetime of the STM32 firmware.  ESP32
capabilities are a property of the firmware variant installed on the
coprocessor and cannot change across a routine `m1_esp32_deinit()` /
`m1_esp32_init()` cycle, so probing once per STM32 boot avoids paying the
`AT+CMD?` timeout (~5 s on AT firmware that does not respond to
`CMD_GET_STATUS`) on every entry into a WiFi/BT/IEEE scene.  The cache is
cleared automatically on STM32 reset; `m1_esp32_caps_reset()` is exposed for
callers that need to force a re-probe (e.g. immediately after an in-field
OTA reflash of the ESP32 coprocessor).

### Adding CMD_GET_STATUS to a custom ESP32 firmware

Respond to opcode `0x02` with a 41-byte `m1_esp32_status_payload_t` payload:

- `proto_ver = 1`
- `cap_bitmap` — set the `M1_ESP32_CAP_*` bit for each capability your
  firmware supports; leave all other bits clear.  The bits are transport-agnostic:
  set the same bit regardless of whether the feature uses binary SPI opcodes or
  AT text commands.
- `fw_name` — a short null-terminated version string (e.g. `"SiN360-0.9.7"` or
  `"AT-neddy299-1.0.1"`); unused bytes are zero-padded.

A complete reference implementation for AT-based firmware is in
[`hapaxx11/esp32-at-monstatek-m1`](https://github.com/hapaxx11/esp32-at-monstatek-m1):
the portable header `main/include/at_m1_status.h` defines the constants, struct,
and `at_m1_status_build_payload()` helper; the SPI receive loop in
`main/interface/spi/at_spi_task_esp32_series.c` intercepts opcode `0x02` before
the AT framework sees it.  Other AT firmware variants can copy `at_m1_status.h`,
adjust `M1_ESP32_THIS_FW_CAP_BITMAP` and `M1_ESP32_THIS_FW_NAME`, and add the
same opcode check to their SPI receive loop.

> **Rule for STM32 firmware contributors:** new ESP32-dependent features MUST gate
> on the exact capability bits they need (`m1_esp32_require_cap` /
> `m1_esp32_has_cap` with one or more `M1_ESP32_CAP_*` bits OR'd together),
> **not** on a compile flag or firmware name string.
> See `m1_csrc/m1_esp32_caps.h` for the full API.

### `AT+CMD?` — runtime probe for AT firmware

AT-based firmware variants that do not implement the `CMD_GET_STATUS` binary
extension (probe 1) fall through to this step.  The M1 leverages the stock
ESP-AT command `AT+CMD?` to enumerate every AT command the firmware advertises
and maps a small set of known commands to `M1_ESP32_CAP_*` capability bits.
This works against unextended AT firmware — stock ESP-AT, bedge117, dag — without
requiring any custom extension on the ESP32 side.

**Response format** (from the ESP-AT
[reference](https://docs.espressif.com/projects/esp-at/en/latest/esp32/AT_Command_Set/Basic_AT_Commands.html#at-cmd)):

```
+CMD:<index>,"<command name>",<test>,<query>,<set>,<exec>
...
OK
```

Example (abbreviated):

```
+CMD:0,"AT",0,0,0,1
+CMD:1,"AT+CWJAP",1,1,1,1
+CMD:2,"AT+BLEHIDINIT",1,1,1,1
+CMD:3,"AT+ZIGSNIFF",1,1,1,0
...
OK
```

**Implementation** — the M1:

1. Allocates an 8 KB response buffer from the FreeRTOS heap.  Stock ESP-AT
   advertises ~150 commands at roughly 30–50 bytes each (~4.5–7.5 KB total);
   8 KB leaves margin for future command additions and for the custom
   commands added by tracked AT firmware variants (bedge117 / dag).
   The buffer is sized to fit the full `AT+CMD?` listing before
   `spi_AT_send_recv()` stops at the trailing `OK\r\n`.
2. Calls `spi_AT_send_recv("AT+CMD?\r\n", ...)`.
3. Confirms the response is well-formed via
   `m1_esp32_caps_at_cmd_response_valid()` (looks for at least one `+CMD:`
   line).
4. Walks `s_at_cmd_cap_map[]` and, for each entry, searches the response for
   the quoted command name (e.g. `"AT+CWJAP"`).  The surrounding quotes are
   significant — they prevent prefix collisions such as `AT+CWJAP` matching
   `AT+CWJAPCFG`.
5. Frees the response buffer.
6. Records the firmware name as `"AT (probed)"` and the OR'd bitmap.

The parser (`m1_esp32_caps_parse_at_cmd_list()`) is a pure-logic, header-inline
helper that takes the response buffer and the mapping table as inputs — fully
host-testable with no transport dependencies.

**Adding a new AT-side capability** is a single-line addition to
`s_at_cmd_cap_map[]` in `m1_csrc/m1_esp32_caps.c`:

```c
static const m1_esp32_at_cmd_cap_entry_t s_at_cmd_cap_map[] = {
    { "AT+CWJAP",      M1_ESP32_CAP_WIFI_JOIN },
    { "AT+BLEHIDINIT", M1_ESP32_CAP_BLE_HID   },
    { "AT+ZIGSNIFF",   M1_ESP32_CAP_802154    },
    { "AT+DEAUTH",     M1_ESP32_CAP_DEAUTH    },
    { "AT+STASCAN",    M1_ESP32_CAP_STA_SCAN  },
    /* Add new entries here.  The command name must include the "AT+" prefix
     * and match exactly the string the firmware emits in the AT+CMD? response. */
};
```

> **Rule for STM32 firmware contributors:** new ESP32-dependent features MUST gate
> on the exact capability bits they need (`m1_esp32_require_cap` /
> `m1_esp32_has_cap` with one or more `M1_ESP32_CAP_*` bits OR'd together),
> **not** on a compile flag or firmware name string.
> See `m1_csrc/m1_esp32_caps.h` for the full API.
