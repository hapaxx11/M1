<!-- See COPYING.txt for license details. -->

# M1 Hapax Firmware

Enhanced firmware for the [Monstatek M1](https://monstatek.com) multi-tool device, forked
from the [original firmware](https://github.com/Monstatek/M1) with major feature additions,
Flipper Zero file compatibility, a modern scene-based UI architecture, and stability
improvements.

> **This is a community project and is not affiliated with or endorsed by Monstatek.**

## GitHub-First — What Makes Hapax Different

Hapax uses a **GitHub-centered workflow** for development, releases, and
documentation. GitHub is the primary home for source code, builds, releases,
project discussion, and related project resources:

- **Automated CI/CD** — pushes/merges to `main` trigger a GitHub Actions
  build and publish a versioned GitHub Release with firmware artifacts,
  except for changes excluded by the workflow's `paths-ignore` rules (for
  example, docs/database/IDE/workflow-only updates). No manual compilation,
  no "here's a .bin I built on my laptop."
- **[Web Updater](https://hapaxx11.github.io/M1/)** — a GitHub Pages-hosted
  browser-based flashing tool.  Plug in via USB, pick a release, and flash —
  no desktop software required.
- **[API Documentation](https://hapaxx11.github.io/M1/docs/)** — Doxygen-generated
  source documentation, auto-deployed on every push to `main` that touches firmware
  source files.
- **OTA from the device** — the M1 itself can browse GitHub Releases over WiFi,
  download firmware, and install it — all without a PC.
- **Automated testing** — host-side unit tests run automatically via GitHub
  Actions on pull requests and pushes to `main` for relevant changes; Doxygen
  docs are deployed on source/docs updates, and static analysis is available
  as a manual, on-demand workflow.
- **Transparent development** — all code, issues, pull requests, security
  advisories, and discussions live on GitHub.  Nothing is hidden behind
  invite-only servers or private channels.

[![CI Build](https://github.com/hapaxx11/M1/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/hapaxx11/M1/actions/workflows/ci.yml)
[![Unit Tests](https://github.com/hapaxx11/M1/actions/workflows/tests.yml/badge.svg?branch=main)](https://github.com/hapaxx11/M1/actions/workflows/tests.yml)
[![Latest Release](https://img.shields.io/github/v/release/hapaxx11/M1?include_prereleases&label=latest)](https://github.com/hapaxx11/M1/releases/latest)

> **🔧 [Flash your M1 right now — open the Web Updater](https://hapaxx11.github.io/M1/)**
>
> No software to install for updates or reflashing if Hapax is already installed. Plug in
> via USB-C, open Chrome/Edge, and flash the latest firmware in seconds. For a first
> install from stock firmware, use the DFU/qMonstatek method below.

## Highlights vs Stock Firmware

| Feature | Monstatek Stock (v0.8.0.1) | Hapax |
|---------|---------------------------|-------|
| Sub-GHz protocols | ~20 | **100** |
| LF-RFID protocols | ~10 | **26** |
| Flipper `.sub`/`.rfid`/`.nfc`/`.ir` import | ✗ | ✓ |
| Scene-based UI architecture | ✗ | ✓ (all modules) |
| Sub-GHz tools (spectrum, RSSI, scanner, weather, brute force, playlist) | ✗ | ✓ |
| CAN bus support | ✗ | ✓ (FDCAN1) |
| OTA firmware download (device WiFi → GitHub Releases) | ✗ | ✓ |
| PicoPass / iCLASS NFC | ✗ | ✓ |
| AES-256 encryption API | ✗ | ✓ |
| Bad-BT (Bluetooth HID) | ✗ | ✓ |
| IR remote database | — | **1,412** files included |
| Sub-GHz signal database | — | **313** files included |
| Sub-GHz playlist database | — | Included (Tesla, doorbells, fans) |
| Browser-based flashing | ✗ | ✓ ([Web Updater](https://hapaxx11.github.io/M1/)) |
| CI/CD auto-releases | ✗ | ✓ (GitHub Actions, every merge to main) |
| Automated unit tests | ✗ | ✓ (GitHub Actions, Unity + ASan/UBSan) |
| Static analysis (cppcheck) | ✗ | ✓ (GitHub Actions, on-demand) |
| Auto-deployed API docs | ✗ | ✓ (Doxygen → GitHub Pages) |

## What's New in Hapax

### Flipper Zero Compatibility
- Import and use Flipper Zero `.sub`, `.rfid`, `.nfc`, and `.ir` files directly
- Drop Flipper files onto the SD card and use them on the M1
- Flipper Music Format (`.fmf`) playback via the Music Player
- Furi compatibility layer for near-direct protocol porting from Flipper/Momentum

### Sub-GHz Enhancements
- **100 protocol decoders** — Princeton, CAME, Nice Flo, Keeloq, Security+ 1.0/2.0, Linear, Holtek, Hormann, Marantec, Somfy, Ansonic, BETT, Clemsa, Doitrand, FireFly, CAME Twee/Atomo, Nice Flor S, Alutech AT-4N, Centurion, Kinggates Stylo, Megacode, Mastercode, Chamberlain 7/8/9-bit, Liftmaster 10-bit, Dooya, Honeywell, Intertechno, Elro, Acurite (incl. 592TXR/986), Bresser, Oregon v1/v2/v3, LaCrosse, Scher-Khan, Toyota, Auriol AHFL, GT-WT-02, Kedsum-TH, ThermoPro TX-4, LaCrosse TX141THBv2, Wendox W6726, DITEC GOL4, Honeywell WDB, X10, FireCracker/CM17A, TX-8300, POCSAG pager decode, and more
- **Spectrum Analyzer** — visual RF spectrum display with zoom, pan, and peak detection
- **RSSI Meter** — real-time signal strength with bar graph and peak tracking
- **Frequency Scanner** — sweep and find active frequencies above threshold
- **Weather Station** — decode Oregon v2, Acurite 606TX/609TXC/592TXR/986, LaCrosse TX141THBv2, Auriol, GT-WT-02, Kedsum-TH, ThermoPro TX-4, Solight TE44, Vauno EN8822C, Emos E601x sensors
- **Brute Force** — brute-force RF code transmitter (Princeton, CAME, Nice FLO, Linear, Holtek)
- **Playlist Player** — load `.txt` playlist files from `SubGHz/playlist/` and transmit each `.sub` file sequentially; supports repeat count, progress display, and Flipper path remapping
- **Add Manually** — select a protocol, enter a hex key, and transmit a single-burst RF signal
- **Radio Settings** — adjustable TX power, custom frequency entry

### NFC Enhancements
- **Tag Info** — manufacturer lookup, SAK decode, technology identification
- **T2T Page Dump** — read and display Type 2 Tag memory pages
- **Clone & Emulate** — copy and replay NFC tags
- **PicoPass/iCLASS** — read, authenticate, and emulate HID iCLASS cards (DES key diversification)
- **NFC Fuzzer** — protocol testing tool
- **MIFARE Classic Crypto1** support

### RFID Enhancements
- **26 protocol decoders** — EM4100 (+ 32/16-bit variants), H10301, HID Generic, HID ExGeneric, Indala26, Indala224, AWID, Pyramid, Paradox, IOProx, FDX-A, FDX-B, Viking, Electra, Gallagher, Jablotron, PAC/Stanley, Securakey, GProx II, Noralsy, Idteck, Keri, Nexwatch, InstaFob
- **Clone Card** — write to T5577 tags
- **Erase Tag** — reset T5577 to factory
- **T5577 Info** — read tag configuration
- **RFID Fuzzer** — protocol testing tool
- **Manchester decoder** with carrier auto-detection (ASK/PSK)

### Infrared
- **Universal Remote Database** — pre-built remotes for Samsung, LG, Sony, Vizio, Bose, Denon, and more (see [`ir_database/`](ir_database/))
- **Learn & Save** — record IR signals and save to SD card
- **Import** Flipper Zero `.ir` files

### BadUSB
- **DuckyScript interpreter** — run keystroke injection scripts from SD card
- Supports `STRING`, `DELAY`, `GUI`, `CTRL`, `ALT`, `SHIFT`, key combos, and `REPEAT`
- Place `.txt` scripts in `BadUSB/` on the SD card

### Bad-BT (Bluetooth HID)
- **Wireless DuckyScript** — same scripting as BadUSB but over Bluetooth HID
- Pairs with target device wirelessly, no cable needed

> **Note:** Bad-BT is under active development. Bluetooth pairing and keystroke delivery
> depend on the target device's BLE HID support.

### CAN Bus (FDCAN)
- **CAN Commander** — sniff, send, and analyse CAN bus traffic via the J7 (X10) header
- **Sniffer** — real-time CAN frame display with baud rate cycling (125 k / 250 k / 500 k / 1 Mbps)
- **Send Frame** — build and transmit arbitrary CAN frames
- Supports standard 11-bit CAN IDs (Classic CAN)
- **Requires external CAN transceiver** — recommended: [Waveshare SN65HVD230 CAN Board](https://www.waveshare.com/sn65hvd230-can-board.htm) (3.3 V, ESD protected)

> **Note:** The M1 does not include an on-board CAN transceiver. See [`HARDWARE.md`](HARDWARE.md) for wiring instructions.

### External Apps
- **ELF app loader** — load and run third-party apps from SD card
- Browse and launch `.m1app` files from the Apps menu
- Download ready-to-use apps and the App SDK at **[m1-sdk](https://github.com/bedge117/m1-sdk)**

### Games & Entertainment
- Snake, Tetris, T-Rex Runner, Pong, Dice — built-in games accessible from the menu
- **Music Player** — plays Flipper Music Format (`.fmf`) files from `SD:/Music/`

### WiFi
- **Scan** — discover nearby access points
- **Connect** — join networks with password entry
- **Saved Networks** — manage stored WiFi credentials (AES-256 encrypted on SD card)
- **Status** — view connection state, IP address, signal strength
- **NTP Time Sync** — automatic time synchronization via `pool.ntp.org` on WiFi connect
- **Firmware Download** — browse and download firmware images from GitHub Releases
  (Monstatek Official, Hapax Fork, or C3/bedge117) directly to SD card for flashing.
  Sources are user-configurable via `System/fw_sources.txt`.

### NFC/RFID Field Detector
- Detect external 13.56 MHz NFC reader fields and ~125 kHz RFID reader fields
- Useful for identifying hidden readers
- Accessible from the **NFC → Field Detect** menu entry

### Signal Generator
- Continuous square-wave output via the buzzer pin (GPIO/speaker)
- 18 frequency presets from 200 Hz to 8 kHz; UP/DOWN to change, OK to toggle on/off
- Accessible from the **GPIO → Signal Gen** menu entry

### Bluetooth Device Manager
- Scan, save, and manage BLE devices
- View device info and connection details

### Dual Boot
- Two firmware banks with safe boot validation
- Swap between banks from the menu or via the companion app
- CRC verification before boot — falls back to working bank on corruption

### Security & Crypto
- **AES-256-CBC encryption** — device-derived keys (from STM32H5 UID) or user-provided custom keys
- WiFi credentials encrypted at rest on SD card
- Crypto API available to external apps via `m1_crypto.h`

### Other Improvements
- **Scene-based UI** — all modules use a stack-based scene manager with push/pop navigation
- **RPC protocol** for [qMonstatek](https://github.com/bedge117/qMonstatek) companion app communication
- **Settings persistence** — LCD brightness, southpaw mode, ISM band region, preferences saved to SD card
- **Southpaw mode** — swap left/right button functions
- **Safe NMI handler** — proper ECC fault recovery instead of hard fault
- **Watchdog improvements** — task-level suspend/resume for long operations
- **CI/CD pipeline** — automated build, test, and release on every merge to `main` via GitHub Actions.  Hapax is the only M1 fork with automated builds and releases.

## Companion App

**[qMonstatek](https://github.com/bedge117/qMonstatek)** — community-maintained Windows desktop app (developed by bedge117; not part of Hapax). Connect your M1 via USB to mirror the device screen, manage SD card files, configure WiFi, update the ESP32 coprocessor firmware, and flash firmware over USB — including DFU mode for first-time installation from stock firmware.

For firmware updates on a device already running Hapax, the browser-based **[Web Updater](https://hapaxx11.github.io/M1/)** requires no software at all. qMonstatek is the recommended path for first-time installation from stock or for users who prefer a desktop UI.

Download from the [qMonstatek releases page](https://github.com/bedge117/qMonstatek/releases).

## Included Databases

### IR Remote Database

The [`ir_database/`](ir_database/) directory contains **1,412** infrared remote files for popular devices. Copy them to `IR/` on the M1's SD card to use with the Universal Remote feature.

**Categories:** TV (413), AC (238), Audio — receivers, soundbars & speakers (292), Fan (155), Projector (122), LED lighting (167), Streaming devices (25).

Top-level files per category are M1-curated "universal" remotes (tested on hardware). Brand subdirectories contain model-specific files imported from the [Flipper-IRDB](https://github.com/Lucaslhm/Flipper-IRDB) community database (CC0 license). See [`ir_database/SOURCES.md`](ir_database/SOURCES.md) for full attribution.

### Sub-GHz Signal Database

The [`subghz_database/`](subghz_database/) directory contains **313** curated Sub-GHz `.sub` signal files. Copy them to `SubGHz/` on the M1's SD card for use with the Sub-GHz Saved feature.

**Categories:** Outlet switches (179), Doorbells (81), Weather stations (39), Smart home remotes (10), Fans (4).

Imported from the [UberGuidoZ/Flipper](https://github.com/UberGuidoZ/Flipper) community repository (GPLv3). See [`subghz_database/SOURCES.md`](subghz_database/SOURCES.md) for full attribution.

### Sub-GHz Playlist Database

The [`subghz_playlist/`](subghz_playlist/) directory contains ready-to-use Sub-GHz playlist files. Copy them to `SubGHz/playlist/` on the SD card for use with the Playlist Player.

**Categories:** Tesla charge port openers, Doorbells, Fans.

Imported from [UberGuidoZ/Flipper](https://github.com/UberGuidoZ/Flipper) (GPLv3). See [`subghz_playlist/SOURCES.md`](subghz_playlist/SOURCES.md) for attribution.

### KeeLoq Manufacturer Key Setup (Rolling-Code Replay)

KeeLoq, Jarolift, and Star Line `.sub` files that include a `Manufacture:` field can
be **replayed** by the M1 using counter-mode re-encryption — but this requires the
manufacturer's 64-bit master key to be present on the SD card.

Place a file named `keeloq_mfcodes` at `SubGHz/keeloq_mfcodes` on the M1's SD card.
The M1 accepts two file formats:

**Compact format** (one entry per line):
```
0123456789ABCDEF:1:BFT
FEDCBA9876543210:2:CAME
```
Where the first field is the 64-bit master key in uppercase hex, the second is the
learning type (1 = Simple, 2 = Normal, 3 = Secure), and the third is the manufacturer
name exactly as it appears in the `.sub` file's `Manufacture:` field.

**RocketGod SubGHz Toolkit format** (multi-line, direct copy supported):
```
Manufacturer: BFT
Key (Hex):    0123456789ABCDEF
Key (Dec):    81985529216486895
Type:         1
------------------------------------
```
The M1 parser accepts this format directly — you can copy the `keeloq_keys.txt`
output from the toolkit and rename it to `keeloq_mfcodes`.

#### How to get the keys

**Option A — RocketGod SubGHz Toolkit (Flipper Zero required):**
1. Install [RocketGod's SubGHz Toolkit](https://github.com/RocketGod-git/RocketGods-SubGHz-Toolkit) on your Flipper Zero.
2. Open the toolkit → *Decrypt KeeLoq Manufacturer Codes* → export `keeloq_keys.txt`.
3. Either copy `keeloq_keys.txt` directly to `SubGHz/keeloq_mfcodes` on the M1's SD card,
   **or** convert it to compact format using the included script:
   ```bash
   python3 scripts/convert_keeloq_keys.py keeloq_keys.txt keeloq_mfcodes
   ```
   Then copy `keeloq_mfcodes` to `SubGHz/` on the M1's SD card.

**Option B — Manual / community-sourced keys:**
Create a plain-text file in compact format with the manufacturer keys you need and place
it at `SubGHz/keeloq_mfcodes` on the SD card.

> **Note:** The M1 loads `keeloq_mfcodes` at the start of each Sub-GHz session.
> If the file is absent, KeeLoq rolling-code `.sub` files are still decoded and displayed
> but cannot be replayed (the device will show "No MFR Key" in the signal info screen).

## Hardware

- **MCU:** STM32H573VIT6 (Cortex-M33, 250 MHz, 2 MB dual-bank flash, 640 KB RAM)
- **Display:** 128×64 monochrome (ST7586s)
- **WiFi/BT:** ESP32-C6 coprocessor (SPI AT interface — not UART)
- **RF:** Si4463 sub-GHz transceiver (300–928 MHz)
- **NFC:** ST25R3916 (13.56 MHz)
- **RFID:** 125 kHz ASK/PSK reader with T5577 write support
- **IR:** TSOP38238 receiver + IR LED transmitter
- **CAN:** FDCAN1 on J7 header (requires external transceiver)
- **USB:** USB-C (CDC + MSC composite)
- **Storage:** microSD card
- **Hardware revision:** 2.x

> **ESP32 note:** The ESP32-C6 communicates with the STM32 via **SPI** AT commands at
> runtime, not UART. Stock Espressif UART-based AT firmware downloads will **not** work.
> The ESP32 firmware must be built with `CONFIG_AT_BASE_ON_SPI=y`. See
> [`DEVELOPMENT.md`](DEVELOPMENT.md) for details.

## Building

### Prerequisites

- **ARM GCC 14.2+** with CMake and Ninja, or
- **STM32CubeIDE 1.17+** (tested with 1.17.0 and 2.1.0)
- **Python 3** (for post-build CRC injection)

### Build with CMake (recommended)

```bash
# Configure
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release

# Build (post-build CRC injection runs automatically)
cmake --build build
```

The CMake `POST_BUILD` step automatically runs `tools/append_crc32.py` to inject CRC
and Hapax metadata into the binary.  For non-CMake builds (STM32CubeIDE), run manually:

```bash
python tools/append_crc32.py build/M1_Hapax_v<VERSION>.bin \
    --output build/M1_Hapax_v<VERSION>_wCRC.bin \
    --hapax-revision 1 --verbose
```

Replace `<VERSION>` with the version from `m1_fw_update_bl.h` (e.g. `0.9.0.1`).

### Build with STM32CubeIDE

Open the project directory in STM32CubeIDE and build.

### Build with Make (Linux)

```bash
make
```

Output: `./artifacts/`

See [`DEVELOPMENT.md`](DEVELOPMENT.md) for detailed build environment setup and
[`documentation/mbt.md`](documentation/mbt.md) for SRecord/CRC tooling.

### Running Tests

Host-side unit tests run on x86 with Address Sanitizer and Undefined Behavior Sanitizer:

```bash
cmake -B build-tests -S tests -DCMAKE_BUILD_TYPE=Debug
cmake --build build-tests
ctest --test-dir build-tests --output-on-failure
```

## Code Quality

Hapax is the only M1 firmware fork with automated quality checks.  All of these run
as GitHub Actions workflows:

| Tool | CI Workflow | Scope | Mode |
|------|-------------|-------|------|
| **cppcheck** | `static-analysis.yml` | `m1_csrc/`, `Sub_Ghz/protocols/` | On-demand (`workflow_dispatch`) |
| **cppcheck MISRA-C** | `static-analysis.yml` | `m1_csrc/` | On-demand (`workflow_dispatch`) |
| **Unity + ASan/UBSan** | `tests.yml` | `bit_util.c` (expandable) | Enforced (blocks PR) |
| **Doxygen** | `docs.yml` | Application source | Auto-deploy to Pages |

## Flashing

All firmware releases are published automatically to
[GitHub Releases](https://github.com/hapaxx11/M1/releases) by the CI/CD pipeline.
You never need to compile firmware yourself — just pick a method below.

### Via Web Updater (recommended for updates)

The fastest way to reflash — no software to install.  The Web Updater is hosted on
GitHub Pages and fetches firmware directly from GitHub Releases.  Requires Hapax
firmware already running on the M1 (the Web Updater connects over USB Serial, which
needs the Hapax RPC interface).  For a first install from stock firmware, use
**Via DFU Mode** below.

1. Open the **[M1 Web Updater](https://hapaxx11.github.io/M1/)** in Chrome or Edge
2. Power on the M1 normally so it boots to the regular UI
3. Connect via USB-C
4. Click **Connect**, select the M1 serial device, pick a firmware release, and flash

Requires a browser with Web Serial support (Chrome 89+ or Edge 89+).
Do **not** use DFU mode for the Web Updater. If the screen stays dark, the device is in DFU
mode and will usually not appear as a serial port; use the **Via DFU Mode (recovery / first
install)** section below instead.

### Via WiFi (OTA)
Connect to WiFi, then go to **Settings → FW Update → Download** to browse and install
firmware images from GitHub Releases directly to SD card.

### Via DFU Mode (recovery / first install)
1. Power off the M1 (Settings → Power → Power Off → Right Button)
2. Hold **Up + OK** for 5 seconds to enter DFU mode (screen stays dark)
3. Connect via USB-C
4. Use the DFU Flash page in [qMonstatek](https://github.com/bedge117/qMonstatek)

To exit DFU mode without flashing, hold **Right + Back** to reboot.

### Via SWD
Use an ST-Link or J-Link debugger with STM32CubeIDE or OpenOCD.

## SD Card Layout

```
0:/
├── BadUSB/          DuckyScript .txt files
├── Firmware/        Downloaded firmware images (created by Download feature)
├── IR/              Infrared remote .ir files (see ir_database/)
│   └── Learned/     IR signals recorded by the M1
├── Music/           Flipper Music Format .fmf files
├── NFC/             NFC tag .nfc files
├── RFID/            RFID tag .rfid files
├── SubGHz/          Sub-GHz signal .sub files (see subghz_database/)
│   ├── keeloq_mfcodes  KeeLoq manufacturer key store (optional, see above)
│   └── playlist/    Playlist .txt files (see subghz_playlist/)
├── System/          System configuration files
│   └── fw_sources.txt  Firmware download sources (auto-generated, user-editable)
├── apps/            External .m1app applications
├── settings.ini     M1 settings (auto-generated)
└── wifi_cred.ini    Saved WiFi credentials (AES-256 encrypted, auto-generated)
```

## Upgrading & Compatibility

### Sub-GHz saved files — pre-v0.9.0.124 files are not emulatable

> ⚠ **If you saved Sub-GHz signals using any Hapax firmware build earlier than
> v0.9.0.124, those files must be deleted and recaptured.**

Any `.sub` or `.sgh` file that was saved by the Hapax firmware **before v0.9.0.124**
contains a zeroed key (`Key: 0x0`) and a blank frequency field due to two bugs that
were fixed together in v0.9.0.124:

1. **Zero key bug** — the legacy save code path did not copy the decoded key value
   into the signal struct before writing to disk. Every file it produced has
   `Key: 00 00 00 00 00 00 00 00`, which causes emulation to transmit all-zero
   pulses — the gate or remote will not respond.
2. **Blank frequency bug** — `snprintf("%.2f MHz", ...)` is a no-op under
   `--specs=nano.specs` (newlib-nano) without `-u _printf_float`, so the
   `Freq:` field in the Signal Info screen was empty and the saved value was not
   useful for diagnosis.

**Files NOT affected by this:**

| File source | Status |
|---|---|
| Captured and saved on **Hapax v0.9.0.124 or later** | ✅ Correct — key, bits, TE, and frequency all written correctly |
| `.sub` / `.sgh` files from **C3.12 or SiN360** firmware | ✅ Correct — those firmwares had working save paths; load and emulate fine on Hapax |
| **Stock Monstatek v0.8.0.x** — files captured on-device | ✅ If the stock firmware wrote a file at all, the key field is correct |
| Files from the bundled **`subghz_database/`** signal library | ✅ Pre-validated Flipper `.sub` format; unaffected |
| Files captured and saved on **Hapax before v0.9.0.124** | ❌ Key is 0x0 — delete and recapture using v0.9.0.124+ |

**How to check a file:** Open Sub-GHz → Saved, select the file, press OK → Info.
If "Key: 0x0" appears, the file is corrupted by this bug and must be recaptured.

## Contributing

Contributions are welcome. Please see [`.github/CONTRIBUTING.md`](.github/CONTRIBUTING.md) for guidelines.

If you're building a companion app or tool that communicates with the M1, the RPC protocol
is implemented in `m1_csrc/m1_rpc.c` and `Core/Src/cli_app.c`.

## License

This project is licensed under the GNU General Public License v3.0 — see [COPYING.txt](COPYING.txt) for details.

Sub-GHz and LF-RFID protocol decoders are derived from the [Flipper Zero firmware](https://github.com/flipperdevices/flipperzero-firmware) (GPLv3). Database files are sourced from [Flipper-IRDB](https://github.com/Lucaslhm/Flipper-IRDB) (CC0) and [UberGuidoZ/Flipper](https://github.com/UberGuidoZ/Flipper) (GPLv3). See [`README_License.md`](README_License.md) for full component attribution.
