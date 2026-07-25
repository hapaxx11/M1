<!-- See COPYING.txt for license details. -->

# ESP32-C6 Coprocessor Firmware

## Overview

The Monstatek M1 uses an ESP32-C6 coprocessor for WiFi, Bluetooth, BLE, and
IEEE 802.15.4 (Zigbee/Thread) connectivity.  The STM32H573 host communicates
with the ESP32-C6 via **SPI AT commands** at runtime and uses **UART** (ROM
bootloader protocol) for firmware flashing.

Espressif's stock AT firmware downloads are **UART-only** and will **NOT** work
with the M1.  A custom SPI-configured build is required.

> **Naming note — two unrelated bedge117 ESP32 firmwares:** bedge117 has
> authored **two separate, non-interoperable** ESP32-C6 coprocessor firmwares
> for the M1, and Hapax previously referred to both loosely as "C3"/"bedge117",
> which conflated them. To disambiguate:
> - **CD3-AT** — [`bedge117/esp32-at-monstatek-m1`](https://github.com/bedge117/esp32-at-monstatek-m1),
>   the original ESP-AT-based firmware (SPI transport layered on Espressif's
>   `esp-at` command stack). `neddy299`, `dagnazty`, and `hapaxx11`'s own
>   `esp32-at-monstatek-m1` forks are all forks of this **CD3-AT lineage**.
>   Detected at runtime via the generic `AT+CMD?` probe (see
>   [AT+CMD? — runtime probe for AT firmware](#at-cmd----runtime-probe-for-at-firmware)).
> - **CD3** — [`bedge117/m1-esp32-brain`](https://github.com/bedge117/m1-esp32-brain),
>   a different, newer codebase: native ESP-IDF (no AT stack at all), using the
>   custom binary `M1_RPC` protocol. Detected via the `M1_RPC` PING/GET_STATUS
>   probe. This is **not** a fork of CD3-AT and shares no source code with it —
>   only the SPI-HD transport wiring and GPIO pin assignment are the same.
>
> Do not use "C3" or "bedge117" alone to mean either firmware — always say
> **CD3-AT** (AT-based) or **CD3** (native binary RPC) so readers know which
> codebase and protocol are being discussed.

## Source Repository

| Repository | Description |
|-----------|-------------|
| [bedge117/esp32-at-monstatek-m1](https://github.com/bedge117/esp32-at-monstatek-m1) | **CD3-AT (primary)** — custom ESP32-C6 SPI AT firmware for M1 |
| [neddy299/esp32-at-monstatek-m1](https://github.com/neddy299/esp32-at-monstatek-m1) | CD3-AT fork with WiFi deauthentication (`AT+DEAUTH`, `AT+STASCAN`) |
| [dagnazty/esp32-at-monstatek-m1](https://github.com/dagnazty/esp32-at-monstatek-m1) | CD3-AT fork (dag / T-800) — additional AT command extensions |
| [hapaxx11/esp32-at-monstatek-m1](https://github.com/hapaxx11/esp32-at-monstatek-m1) | CD3-AT fork of neddy299 — adds `CMD_GET_STATUS` binary capability probe (eliminates `AT+CMD?` timeout) |
| [bedge117/m1-esp32-brain](https://github.com/bedge117/m1-esp32-brain) | **CD3 native binary RPC** — separate codebase, ESP-IDF native WiFi/BLE/802.15.4, no AT stack, `M1_RPC` protocol |

All AT-based repos above are forks of Espressif's official
[esp-at](https://github.com/espressif/esp-at) project, customised for the M1's
SPI transport and pin mapping. `bedge117/m1-esp32-brain` (CD3) is **not** an
`esp-at` fork — it is a from-scratch native ESP-IDF application.

## Releases

### bedge117 (CD3-AT) releases

| Version | Features |
|---------|----------|
| **v1.0.0** | Base SPI AT firmware — correct SPI pin mapping, status register offset fix, USB Serial/JTAG disabled, SPI Mode 1 |
| **v2.0.2** | + BLE HID keyboard (`AT+BLEHIDINIT` / `AT+BLEHIDKB`), + IEEE 802.15.4 sniffer (`AT+ZIGSNIFF`) with Thread frame version filter fix |

### neddy299 (CD3-AT Deauth fork) releases

| Version | Features |
|---------|----------|
| **v1.0.1** | WiFi deauthentication (`AT+DEAUTH`), station scanning (`AT+STASCAN`) |

### hapaxx11 (CD3-AT, neddy299 fork, CMD_GET_STATUS) releases

| Version | Features |
|---------|----------|
| **v1.0.1-caps** | All neddy299 v1.0.1 features + `CMD_GET_STATUS` binary opcode (opcode `0x02`) — self-reports `cap_bitmap = 0x14412` (`STA_SCAN` \| `DEAUTH` \| `WIFI_JOIN` \| `BLE_HID` \| `802154`), `fw_name = "AT-neddy299-1.0.1"` |

### dagnazty (CD3-AT, dag / T-800) releases

| Version | Features |
|---------|----------|
| See [GitHub Releases](https://github.com/dagnazty/esp32-at-monstatek-m1/releases) | Additional AT command extensions on top of the CD3-AT (bedge117) base |

### bedge117/m1-esp32-brain (CD3 native binary RPC — separate codebase from CD3-AT) releases

| Version | Notable changes |
|---------|----------------|
| See [GitHub Releases](https://github.com/bedge117/m1-esp32-brain/releases) | Native ESP-IDF WiFi/BLE/802.15.4; SPI-slave architecture; M1_RPC binary protocol; M1-to-M1 ESP-NOW peer link; GPIO API |


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

### dagnazty (dag) custom AT commands

These commands are defined in [`dagnazty/esp32-at-monstatek-m1`](https://github.com/dagnazty/esp32-at-monstatek-m1)
(`at_custom_wifi_cmd.c`, `at_custom_hid_cmd.c`, `at_custom_zigbee_cmd.c`).
They are NOT available in CD3-AT (bedge117/neddy299), or SiN360 firmware.
The companion STM32 fork [`dagnazty/M1_T-1000`](https://github.com/dagnazty/M1_T-1000)
uses these commands via SPI AT text protocol.

> **Hapax T-800 transport note (Phase 2 resolved — 2026-06-17):** The dag T-800 ESP32
> firmware (hapaxx11/M1-T-800, the Hapax-tracked fork) uses **SPI AT text** as its
> primary transport, matching Hapax's `spi_AT_send_recv()` interface.  A binary RPC layer
> exists in `main/rpc/` (magic `0x4D31`, i.e. "M1" LE) and is labeled "phase 1 dual-mode"
> but is not yet the primary path.  The T-800 AT commands below are therefore callable
> from Hapax firmware via `spi_AT_send_recv()` when the T-800 ESP32 is flashed.
>
> **Capability gating:** T-800 firmware is fingerprinted by the presence of both
> `M1_ESP32_CAP_WIFI_JOIN` and `M1_ESP32_CAP_BEACON` bits (3-way discriminator in
> `m1_esp32_caps.c`).  Use `m1_esp32_has_cap(M1_ESP32_CAP_BEACON)` to gate T-800-only
> features.
>
> **Hapax integration status:** `AT+CWLAP` + `AT+M1PMKID` are integrated as the
> "PMKID Grab" scene (`WifiSceneAttackPmkidAt`) in the WiFi Attacks menu.  All other
> T-800 commands remain documented here for future integration.

**WiFi attacks:**

| Command | Parameters | Description |
|---------|-----------|-------------|
| `AT+M1DEAUTH` | `="<bssid>",<ch>` | Deauth single AP — broadcast deauth frames on specified channel |
| `AT+M1DEAUTHALL` | _(none)_ | Scan all visible APs and deauth all simultaneously (channel-hopping) |
| `AT+M1DEAUTHSTOP` | _(none)_ | Stop ongoing deauth |
| `AT+M1BEACON` | `=<start>,<ch>,"<ssid>","<bssid>"` | Beacon flood — broadcast fake AP beacon on specified channel |
| `AT+M1KARMA` | `=<start>,"<ssid>"` | Karma attack — respond to probe requests with matching fake AP |
| `AT+M1HSCAP` | `="<bssid>",<ch>,<timeout_s>` | PMKID / WPA handshake capture for specified AP |
| `AT+M1EVILTWIN` | `=<start>,"<ssid>",<ch>` | Evil Twin rogue AP with captive portal on specified channel |
| `AT+M1PROBE` | `=<start>,<ch>` | Probe flood — send probe requests on specified channel |
| `AT+M1WIFISTATS` | _(none)_ | Query per-channel packet statistics from monitor mode |
| `AT+M1PMKID` | `="<bssid>",<ch>` | Dedicated PMKID capture (EAPOL handshake, does not require client) |

**WiFi monitoring:**

| Command | Parameters | Description |
|---------|-----------|-------------|
| `AT+M1MONITOR` | `=<start>,<ch>` | Enable/disable promiscuous monitor mode on specified channel |

**BLE HID keyboard (replaces AT+BLEHIDINIT/AT+BLEHIDKB in dag firmware):**

| Command | Parameters | Description |
|---------|-----------|-------------|
| `AT+HIDKBINIT` | `=<enable>` | Initialise BLE HID keyboard service |
| `AT+HIDKBSEND` | `=<modifier>,<k1>,<k2>,<k3>,<k4>,<k5>,<k6>` | Send HID keyboard report (7-byte USB HID format) |

> **Note:** `AT+HIDKBINIT` / `AT+HIDKBSEND` replace the bedge117-style
> `AT+BLEHIDINIT` / `AT+BLEHIDKB` naming in the dag firmware variant.

**IEEE 802.15.4 / Zigbee (same semantics as bedge117/neddy299):**

| Command | Parameters | Description |
|---------|-----------|-------------|
| `AT+ZIGSNIFF` | `=<start>,<channel>` | Start/stop 802.15.4 promiscuous sniffing on specified channel |

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
3. Select a firmware source (`C3 ESP32 AT`, `Deauth ESP32 AT` — both are
   **CD3-AT** AT-based firmware; there is currently no default download source
   configured for the native **CD3** `m1-esp32-brain` firmware).
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

See the [`esp32-coprocessor`](../.github/skills/esp32-coprocessor/SKILL.md) skill ("ESP32 Build — How to Build from Claude Code") for the
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
| 17 | `M1_ESP32_CAP_BLE_GATT` | `CMD_BLE_GATT_*` client operations |
| 18 | `M1_ESP32_CAP_PMKID` | Dedicated PMKID capture — reserved `M1_RPC_OFF_PMKID_CAPTURE` msg ID; **not yet dispatched** in current CD3 (`bedge117/m1-esp32-brain`) releases as of the 2026-07 review — see note below |
| 19 | `M1_ESP32_CAP_HANDSHAKE` | WPA handshake / EAPOL capture with pcap — `M1_RPC_OFF_HS_START/STATUS/GET/STOP` **are dispatched** in current CD3 releases, but the firmware does not yet set this bit in its self-reported `cap_bitmap` — see note below |
| 20 | `M1_ESP32_CAP_OTA` | ESP32 firmware OTA self-update — reserved `M1_RPC_SYS_OTA_BEGIN/DATA/END` msg IDs; **not yet dispatched** in current CD3 releases — see note below |
| 21-63 | — | Reserved for future use |

> **CD3 (`bedge117/m1-esp32-brain`) capability status — reviewed 2026-07-21
> against public source (commit `74ace433`):** the `M1_RPC` wire protocol
> (`components/m1_rpc/include/m1_rpc.h`) *reserves* message IDs for PMKID
> capture and OTA self-update, and `m1_csrc/m1_esp32_caps.h` defines
> corresponding `M1_ESP32_CAP_PMKID`/`M1_ESP32_CAP_OTA` bits for when they
> ship. As of that review, `main/main.c`'s `dispatch_request()` has **no
> case** for `M1_RPC_OFF_PMKID_CAPTURE` or any `M1_RPC_SYS_OTA_*` message —
> both fall through to the `default:` handler and are NAK'd with
> `M1_RPC_ERR_UNSUPPORTED`. Handshake capture (`M1_RPC_OFF_HS_*`) **is**
> dispatched and functional, but the firmware's advertised capability
> bitmap (`M1_FW_CAPS` in `main.c`) is currently only
> `WIFI_SCAN | WIFI_JOIN | DEAUTH | PKTMON` — it does not yet set
> `M1_CAP_HANDSHAKE`, so `m1_esp32_has_cap(M1_ESP32_CAP_HANDSHAKE)` will
> return false against a real device even though the underlying RPC calls
> would succeed if issued directly. Do not build UI/menu entries that assume
> PMKID or OTA are usable against CD3 today; re-verify against the firmware's
> self-reported `cap_bitmap` (not this table) before relying on any of bits
> 18-20.

### Capability matrix by firmware variant

Both SiN360 (via `CMD_GET_STATUS`) and AT firmware (via the stock `AT+CMD?`
listing) self-report their capabilities at runtime.  CD3 firmware
(`bedge117/m1-esp32-brain`) self-reports via the M1_RPC probe.  The table
below shows the *reference/target* `cap_bitmap` for each tracked variant —
i.e. the feature set the firmware family is designed to eventually expose,
which for CD3 is **not** the same as what a given release currently
self-reports (see the capability-status note above the wire-bits table).

| Command family | SiN360 | CD3-AT / dag T-800 | neddy299 (hapaxx11 fork) | CD3 (m1-esp32-brain) |
|----------------|:------:|:-------------------:|:------------------------:|:--------------------:|
| WiFi AP scan | ✅ | — | — | ✅ |
| Station scan | ✅ | — | ✅ | ✅ |
| Packet monitor | ✅ | — | — | ✅ |
| Deauth | ✅ | — | ✅ | ✅ |
| Beacon spam | ✅ | — | — | ✅ |
| Probe flood | ✅ | — | — | ✅ |
| Karma | ✅ | — | — | ✅ |
| Portal | ✅ | — | — | ✅ |
| Network scanners | ✅ | — | — | — |
| WiFi join/disconnect | — | ✅ | ✅ | ✅ |
| BLE scan / advertise | ✅ | — | — | ✅ |
| **BLE HID (Bad-BT)** | — | ✅ | ✅ | ✅ |
| **IEEE 802.15.4** | — | ✅ | ✅ | ✅ |
| Classic BT management | — | — | — | — |
| **PMKID capture** | — | — | — | 🚧 reserved, not dispatched |
| **Handshake capture** | — | — | — | ✅ (dispatched; cap bit not yet self-reported) |
| **OTA self-update** | — | — | — | 🚧 reserved, not dispatched |

AT capability mapping audit (tracked firmware set): the AT commands currently
mapped to capability bits by the runtime `AT+CMD?` probe are `AT+CWJAP`
(WiFi join), `AT+BLEHIDINIT` (BLE HID), `AT+ZIGSNIFF` (802.15.4),
`AT+DEAUTH` (deauth), and `AT+STASCAN` (station scan).  Adding a new
mapping is a single-line edit to `s_at_cmd_cap_map[]` in
`m1_csrc/m1_esp32_caps.c`.

### Probe sequence

When the M1 initialises the ESP32, it performs a multi-step capability probe:

1. **Binary CMD_PING** (opcode `0x01`): tried first.  Confirms that the
   connected firmware speaks the SiN360/binary-SPI protocol (magic `0xAB`/`0xCD`).
   CD3 firmware (magic `0x4D31`) does **not** respond to this ping; it is
   correctly detected by probe 3 below.

2. **Binary CMD_GET_STATUS** (opcode `0x02`): tried after CMD_PING.  SiN360
   binary-SPI firmware and AT-based firmware that implements the binary
   extension (e.g. `hapaxx11/esp32-at-monstatek-m1`) respond here with the
   41-byte capability payload.  Unextended AT firmware (CD3-AT base, dag,
   stock neddy299) does not implement this opcode and will time out or
   return `RESP_ERR`.  If CMD_PING succeeded but CMD_GET_STATUS failed, the
   firmware is classified as SiN360 and the SiN360 fallback profile is
   applied.

3. **M1_RPC PING** (magic `0x4D31` "M1"): tried only when neither CMD_PING nor
   CMD_GET_STATUS succeeded (i.e. the firmware is not SiN360-style binary-SPI).
   Detects CD3 firmware (`bedge117/m1-esp32-brain`), which uses the M1_RPC
   binary protocol with a different frame format (see [M1_RPC Protocol](#m1_rpc-protocol)
   below).  If PING succeeds, M1_RPC GET_STATUS is immediately issued to
   retrieve the capability bitmap; if GET_STATUS fails, the CD3 conservative
   profile macro (`M1_ESP32_CAP_PROFILE_CD3`) is applied.

4. **Stock `AT+CMD?`** (AT text command): tried only when steps 1–3 all fail
   — i.e. for AT firmware that does not implement the binary extension
   (CD3-AT base, dag) and the AT task (`get_esp32_main_init_status()`) is active.
   `AT+CMD?` is part of the standard ESP-AT command set
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

5. **Fail-closed default**: applied when all steps 1–4 fail.  The
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

### <a name="m1_rpc-protocol"></a>M1_RPC protocol — CD3 firmware (bedge117/m1-esp32-brain)

CD3 firmware uses a structured binary RPC protocol over the **same SPI-HD
transport** as the AT firmware (same GPIO pins, same status register layout,
same CS/HANDSHAKE signalling), but with a different frame format.  There is no
AT stack in CD3.

**Frame format:**

```
[ magic:2 LE ][ version:1 ][ msg_type:1 ][ msg_id:2 LE ][ payload_len:2 LE ]
[ payload:0..N bytes ]
[ CRC16:2 LE ]
```

| Field | Size | Value |
|-------|------|-------|
| `magic` | 2 | `0x4D31` LE ("M1") — wire bytes `[0x31, 0x4D]` |
| `version` | 1 | `0x01` |
| `msg_type` | 1 | `0x01` = REQ, `0x02` = RESP, `0x05` = NAK |
| `msg_id` | 2 LE | Command ID (see below) |
| `payload_len` | 2 LE | Byte count of the following payload |
| `payload` | 0..N | Command-specific payload |
| `CRC16` | 2 LE | CRC-16/CCITT (poly `0x1021`, init `0xFFFF`) over header+payload |

**System command IDs:**

| `msg_id` | Name | Request payload | Response payload |
|----------|------|-----------------|-----------------|
| `0x0001` | PING | 4-byte cookie | Echo of cookie |
| `0x0002` | GET_STATUS | none | `m1_esp32_rpc_devstatus_t` (41 bytes) |

**GET_STATUS response payload (`m1_esp32_rpc_devstatus_t`, 41 bytes):**

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 1 | `proto_ver` | `0x01` |
| 1 | 8 | `cap_bitmap[8]` | `M1_ESP32_CAP_*` bits as LE `uint64_t` packed into 8 bytes |
| 9 | 32 | `fw_name` | Null-terminated ASCII firmware identifier (e.g. `"m1-native"`) |

The GET_STATUS payload is wire-identical to `m1_esp32_status_payload_t` (used
by the SiN360 CMD_GET_STATUS path) on LE targets, so the same
`m1_esp32_caps_parse_payload()` helper decodes both.

**SPI transport:** CD3 uses `spi_slave_hd` (half-duplex) mode over the **same
SPI-HD transport as the AT firmware** — identical GPIO pin assignment
(`ESP32_SPI3_NSS_Pin` CS, `ESP32_HANDSHAKE_Pin` response-ready), command/
address/dummy phases, status register, and `WRBUF`/`RDDMA` DMA windows, all
driven by the AT RTOS task (`spi_trans_control_task` in `esp_app_main.c`).  A
`spi_slave_hd` slave **cannot** answer a plain full-duplex `HAL_SPI_TransmitReceive`,
so the M1_RPC PING/GET_STATUS probe is issued through
`spi_AT_send_recv_bin()` (the binary-safe variant of `spi_AT_send_recv()`),
**not** the full-duplex `m1_esp32_send_cmd_raw()` path.  The host writes the
M1_RPC request frame (length-prefixed, not NUL-terminated), waits for
HANDSHAKE, and reads the response frame with its true byte length preserved.

> **Binary-safe receive (PR #669 deferred fix):** The legacy AT receive path
> delivered every slave response as a NUL-terminated C string (`strcpy()`).
> M1_RPC frames contain embedded `0x00` bytes in their header/CRC, so that
> path truncated them and CD3 was misdetected as `Unknown (fallback)`.  The
> receive copy now uses a length-based `esp32_spi_bin_copy()`
> (`m1_csrc/esp32_spi_bin.h`, host-tested by `tests/test_esp32_spi_bin.c`), and
> the CD3 probe runs **after** the `AT+CMD?` text probe so a working AT
> firmware is never sent a binary frame.

**Pure-logic helpers** for building and validating M1_RPC frames are in
`m1_csrc/m1_esp32_caps.h` (static inline, no HAL deps):
- `m1_esp32_rpc_crc16()` — CRC-16/CCITT computation
- `m1_esp32_rpc_build_req()` — assemble a request frame into a buffer
- `m1_esp32_rpc_parse_resp()` — validate a response (magic, version, CRC, msg_id)
- `m1_esp32_rpc_caps_get()` — unpack an 8-byte LE cap_bitmap array to `uint64_t`

`m1_esp32_send_cmd_raw()` (`m1_csrc/m1_esp32_cmd.c`) remains available as the
full-duplex raw path used by the SiN360 binary protocol and ESP-NOW HAL, but
is no longer used for the CD3 M1_RPC probe.

### ESP-NOW peer link — CD3 M1_RPC command range `0x0600..0x0605`

CD3 implements M1-to-M1 ESP-NOW peer link in the `components/esp_now_link`
component.  The STM32 controls it via six M1_RPC message IDs:

| `msg_id` | Name | REQ payload | RESP payload |
|----------|------|-------------|--------------|
| `0x0600` | `M1_RPC_NOW_START` | ch(1) + name string | status(1) + mac(6) |
| `0x0601` | `M1_RPC_NOW_STOP` | none | status(1) |
| `0x0602` | `M1_RPC_NOW_ANNOUNCE` | none | status(1) |
| `0x0603` | `M1_RPC_NOW_PEERS_GET` | none | count(1) + [mac(6)+rssi(1)+namelen(1)+name]×N |
| `0x0604` | `M1_RPC_NOW_SEND` | mac(6) + data | status(1) |
| `0x0605` | `M1_RPC_NOW_RECV_GET` | none | count(1) + [mac(6)+len(2 LE)+data]×N |

**ESP-NOW radio frame format** (over the air, not the SPI frame):

```
[ 'M' (0x4D) ][ '1' (0x31) ][ type:1 ][ payload:0..240 ]
```

| Type | Name | Payload |
|------|------|---------|
| `0x00` | `ANNOUNCE` | Device name string (up to `ENL_NAME_MAX = 23` bytes) |
| `0x01` | `DATA` | User message bytes (up to `ENL_MSG_MAX = 240` bytes) |

Peer table: up to `ENL_MAX_PEERS = 16` entries (LRU eviction).  Discovery is
broadcast (`FF:FF:FF:FF:FF:FF`); the ESP32 buffers up to 32 inbound messages.
The STM32 polls `NOW_RECV_GET` to drain the ring buffer.

**STM32-side SPI transport constraint:**  The fixed 64-byte SPI transaction
leaves at most 48 bytes of usable RPC payload per call (after the 16-byte
M1_RPC header+CRC overhead).  `NOW_SEND` prefixes a 6-byte MAC, so the
maximum ESP-NOW application data per SPI call is **42 bytes** — not the
240-byte ESP-NOW protocol limit.  Full-size payloads require multi-transaction
RPC chunking (not yet implemented).

**Capability bit:** `M1_ESP32_CAP_ESPNOW` (bit 21, `m1_csrc/m1_esp32_caps.h`).
CD3 implements the handlers but does not yet self-report this bit in
`M1_FW_CAPS`; the feature gate will fail closed until CD3 adds it.

**Key design decisions:**
- *No encryption* — `encrypt = false` always, matching CD3's `esp_now_link`.
  Visual confirmation codes (4-digit `CRC32(mac_A‖mac_B) % 10000`) provide
  MITM awareness.  CCMP support will be added if a future CD3 release
  standardises it.
- *Channel coordination* — The **acknowledging device (responder) always hops**
  to the initiator's channel at `NOW_START` time.  If the responder does not
  hop, the initiator treats it as a declined connection.  No public
  protocol-level standard exists; this is our application-layer convention.
- *File transfer* — Stop-and-wait ARQ over the peer link, implemented in
  `m1_csrc/espnow_file_transfer.c/h` with streaming-to-SD (FatFS) and
  incremental CRC32.  Chunk size must not exceed 42 bytes with the current
  SPI transport.

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

### Memory footprint estimates — for developer use only

`bss_bytes` and `free_heap_bytes` are **not** part of the `CMD_GET_STATUS` wire
protocol.  Instead, the M1 always derives them from compile-time constants
(`M1_ESP32_FALLBACK_*` in `m1_csrc/m1_esp32_caps.h`) based on source-code
analysis of the known Hapax-fork ESP32 firmware releases.

The discriminator is a four-way check applied in priority order:

| Priority | Discriminator | Profile | BSS estimate | Free heap estimate |
|----------|--------------|---------|-------------|---------------------|
| 1 | `HANDSHAKE` **and** `OTA` both set | **CD3** native (bedge117/m1-esp32-brain) | ≈ 185 KB (est.) | ≈ 175 KB (est.) |
| 2 | `WIFI_JOIN` **and** `BEACON` both set | **dag T-800** AT | ≈ 290 KB | ≈ 105 KB |
| 3 | `WIFI_JOIN` set, `BEACON` absent | **CD3-AT** (bedge117/neddy299) | ≈ 284 KB | ≈ 112 KB |
| 4 | `WIFI_JOIN` absent | **SiN360** binary-SPI | ≈ 200 KB | ≈ 160 KB |

> **Caveat:** priority 1 assumes a CD3 build that self-reports both
> `HANDSHAKE` and `OTA`.  As of the 2026-07-21 source review, no published
> CD3 release sets either bit (see the capability-status note in
> [Runtime Capability Detection](#wire-bits--cap_bitmap) above), so in
> practice CD3 devices detected via the M1_RPC probe currently fall back to
> `M1_ESP32_CAP_PROFILE_CD3` (applied when M1_RPC PING succeeds but
> GET_STATUS does not, or as a manual override) rather than matching this
> discriminator row from a real `cap_bitmap`.  The row remains correct for
> whenever a CD3 release starts self-reporting those bits.

CD3 figures are estimates pending actual hardware measurement (native ESP-IDF
WiFi/BLE/802.15.4 without AT overhead has lower BSS and higher available heap
than the AT firmware stack).  Update `M1_ESP32_FALLBACK_BSS_CD3` and
`M1_ESP32_FALLBACK_HEAP_CD3` in `m1_csrc/m1_esp32_caps.h` once a memory map
from a production CD3 build is available.

These values are accessible at runtime via `m1_esp32_caps_bss_bytes()` and
`m1_esp32_caps_free_heap()` and are intended for developer diagnostics (OOM
triage, buffer sizing decisions) — not user-visible display.  See
the [`esp32-coprocessor`](../.github/skills/esp32-coprocessor/SKILL.md) skill ("Memory Footprint Estimates") for guidance on updating them
when new firmware releases are analysed.
