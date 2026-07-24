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
  no desktop software required.  **Hapax original:** the updater also
  automatically installs SD card assets (IR/SubGHz signal databases, 1,700+
  files) directly to the device over USB — skip, overwrite, or cherry-pick
  what gets installed.  No ZIP extraction, no SD card reader needed.
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

> **🔧 [Flash your M1 — open the Web Updater](https://hapaxx11.github.io/M1/)** *(work in progress)*
>
> Browser-based flashing if Hapax is already installed. Plug in via USB-C, open
> Chrome/Edge, pick a release, and flash. For a first install from stock firmware,
> use the DFU/qMonstatek method below.

## Highlights vs Stock Firmware

| Feature | Monstatek Stock (v0.8.0.1) | Hapax |
|---------|---------------------------|-------|
| Sub-GHz protocols | ~20 | **105** |
| LF-RFID protocols | ~10 | **26** |
| Flipper `.sub`/`.rfid`/`.nfc`/`.ir` import | ✗ | ✓ |
| Scene-based UI architecture | ✗ | ✓ (all modules) |
| Sub-GHz tools (spectrum, RSSI, scanner, signal ID, weather, brute force, playlist) | ✗ | ✓ |
| CAN bus support | ✗ | ✓ (FDCAN1) |
| OTA firmware download (device WiFi → GitHub Releases) | ✗ | ✓ |
| PicoPass / iCLASS NFC | ✗ | ✓ |
| On-device MFKey32 MIFARE key recovery | ✗ | ✓ |
| AES-256 encryption API | ✗ | ✓ |
| Bad-BT (Bluetooth HID) | ✗ | ✓ |
| WiFi sniffers, attacks, recon & net scan (ESP32 required) | ✗ | ✓ (27 tools) |
| BLE sniffers, wardrive, spam & detectors (ESP32 required) | ✗ | ✓ (20 tools) |
| IR remote database | — | **1,412** files included |
| Sub-GHz signal database | — | **313** files included |
| Sub-GHz playlist database | — | Included (Tesla, doorbells, fans) |
| Browser-based flashing | ✗ | ✓ ([Web Updater](https://hapaxx11.github.io/M1/)) |
| Browser SD card population (no reader needed) | ✗ | ✓ (Hapax original) |
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
- **105 protocol decoders** — Princeton, CAME, Nice Flo, Keeloq, Security+ 1.0/2.0, Linear, Holtek, Hormann, Marantec, Somfy, Ansonic, BETT, Clemsa, Doitrand, FireFly, CAME Twee/Atomo, Nice Flor S, Alutech AT-4N, Centurion, Kinggates Stylo, Megacode, Mastercode, Chamberlain 7/8/9-bit, Liftmaster 10-bit, Dooya, Honeywell, Intertechno, Elro, Nord ICE, Acurite (incl. 592TXR/986), Bresser, Oregon v1/v2/v3, LaCrosse, Scher-Khan, Toyota, Auriol AHFL, GT-WT-02, Kedsum-TH, ThermoPro TX-4, LaCrosse TX141THBv2, Wendox W6726, DITEC GOL4, Honeywell WDB, X10, FireCracker/CM17A, TX-8300, POCSAG pager decode, and more
- **Spectrum Analyzer** — visual RF spectrum display with zoom, pan, and peak detection
- **RSSI Meter** — real-time signal strength with bar graph and peak tracking
- **Frequency Scanner** — sweep and find active frequencies above threshold
- **Signal Identifier (RF Rosetta)** — passive signal identification: fingerprints captured signals by physical characteristics (band, modulation, timing, repetition) and scores against a protocol database with security metadata; works on both sub-GHz and 2.4 GHz domains (BLE/WiFi/802.15.4 via ESP32-C6)
- **Weather Station** — decode Oregon v2, Acurite 606TX/609TXC/592TXR/986, LaCrosse TX141THBv2, Auriol, GT-WT-02, Kedsum-TH, ThermoPro TX-4, Solight TE44, Vauno EN8822C, Emos E601x sensors
- **Brute Force** — brute-force RF code transmitter (Princeton, CAME, Nice FLO, Linear, Holtek)
- **Playlist Player** — load `.txt` playlist files from `SubGHz/playlist/` and transmit each `.sub` file sequentially; supports repeat count, progress display, and Flipper path remapping
- **Proto Pirate** — rolling-code analysis toolkit: live capture, offline `.sub` file decode, and timing tuner comparing captured pulse widths against 25 automotive/garage protocol definitions (KeeLoq, Star Line, CAME, Nice FLO, etc.)
- **Add Manually** — select a protocol, enter a hex key, and transmit a single-burst RF signal
- **Create from Scratch** — protocol picker with per-field editors for KeeLoq/Star Line/Jarolift (serial, button, counter, manufacturer key)
- **Radio Settings** — adjustable TX power, custom frequency entry (300–928 MHz)

### NFC Enhancements
- **Tag Info** — manufacturer lookup, SAK decode, technology identification
- **T2T Page Dump** — read and display Type 2 Tag memory pages
- **Clone & Emulate** — copy and replay NFC tags
- **PicoPass/iCLASS** — read, authenticate, and emulate HID iCLASS cards (DES key diversification)
- **NFC Fuzzer** — protocol testing tool
- **MIFARE Classic Crypto1** support
- **On-device MFKey32 key recovery** — after capturing two reader authentication nonces via Detect Reader, recovers the MIFARE Classic sector key entirely on-device using a memory-bounded Crapto-1 solver; saves recovered keys in Proxmark-compatible dictionary format

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
- **External transmitter (optional)** — drive an external HX-53 IR LED on the expansion header (PA9, 5 V) for higher output via **Settings → LCD and Notifications → External IR**; receive/learn stays on the onboard sensor

### BadUSB
- **DuckyScript interpreter** — run keystroke injection scripts from SD card
- Supports `STRING`, `DELAY`, `GUI`, `CTRL`, `ALT`, `SHIFT`, key combos, and `REPEAT`
- Place `.txt` scripts in `BadUSB/` on the SD card

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
- Snake, Tetris, T-Rex Runner, Pong, 2048, Dice — built-in games accessible from the menu
- **Music Player** — plays Flipper Music Format (`.fmf`) files from `SD:/Music/`

### WiFi

> **Requires compatible ESP32 firmware** — either [SiN360 ESP32](https://github.com/sincere360/M1_SiN360_ESP32/releases) (binary SPI, full feature set) or [dag T-800](https://github.com/dagnazty/ESP32-C6-ESP-AT_M1) (AT commands, partial feature set). See ESP32 note below.

**Sniffers:**
- Packet sniffers: All, Beacon, Probe, Deauth, EAPOL, SAE/WPA3, Pwnagotchi

**Attacks:**
- Deauth, Beacon Spam, AP Clone, Rickroll, Evil Portal, Probe Flood, Karma, Karma+Portal, PMKID Grab

**Recon:**
- Station Scan, 2.4G Survey, MAC Track, Wardrive, Station Wardrive, Signal Monitor

**Network Scanners:**
- Ping, ARP, SSH, Telnet, Port Scan

**General:**
- Networks (scan & connect), Status, Saved Networks (AES-256 encrypted on SD card)
- Firmware Download — browse and download Hapax releases directly to SD card
- Set SSID/MAC/channel, Evil Portal HTML config, save/load/clear AP lists

### NFC/RFID Field Detector
- Detect external 13.56 MHz NFC reader fields and ~125 kHz RFID reader fields
- Useful for identifying hidden readers
- Accessible from the **NFC → Field Detect** menu entry

### Signal Generator
- Continuous square-wave output via the buzzer pin (GPIO/speaker)
- 18 frequency presets from 200 Hz to 8 kHz; UP/DOWN to change, OK to toggle on/off
- Accessible from the **GPIO → Signal Gen** menu entry

### Bluetooth & BLE

> **Requires compatible ESP32 firmware** — either [SiN360 ESP32](https://github.com/sincere360/M1_SiN360_ESP32/releases) (binary SPI) or [dag T-800](https://github.com/dagnazty/ESP32-C6-ESP-AT_M1) (AT commands, BLE Spam only). See ESP32 note below.

**BLE Sniffers:** Analyzer, Generic, Flipper, AirTag Sniff/Monitor, Flock

**BLE Wardrive:** Regular, Continuous, Flock

**BLE Spam:** BLE Spam (unified All/Apple/Google/Microsoft), Sour Apple, SwiftPair, Samsung, Flipper, All, AirTag Spoof

**BLE Detectors:** Skimmers, Flock, Meta

**BLE Config:** Advertise, BLE settings

**Bad-BT (Bluetooth HID):**
- **Wireless DuckyScript** — same scripting as BadUSB but over Bluetooth HID
- Pairs with target device wirelessly, no cable needed

> **Note:** Bad-BT (HID) is under active development. Bluetooth pairing and keystroke delivery
> depend on the target device's BLE HID support.

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

The [`ir_database/`](ir_database/) directory contains **1,412** infrared remote files for popular devices.

**Categories:** TV (413), AC (238), Audio — receivers, soundbars & speakers (292), Fan (155), Projector (122), LED lighting (167), Streaming devices (25).

Top-level files per category are M1-curated "universal" remotes (tested on hardware). Brand subdirectories contain model-specific files imported from the [Flipper-IRDB](https://github.com/Lucaslhm/Flipper-IRDB) community database (CC0 license). See [`ir_database/SOURCES.md`](ir_database/SOURCES.md) for full attribution.

### Sub-GHz Signal Database

The [`subghz_database/`](subghz_database/) directory contains **313** curated Sub-GHz `.sub` signal files.

**Categories:** Outlet switches (179), Doorbells (81), Weather stations (39), Smart home remotes (10), Fans (4).

Imported from the [UberGuidoZ/Flipper](https://github.com/UberGuidoZ/Flipper) community repository (GPLv3). See [`subghz_database/SOURCES.md`](subghz_database/SOURCES.md) for full attribution.

### Sub-GHz Playlist Database

The [`subghz_playlist/`](subghz_playlist/) directory contains ready-to-use Sub-GHz playlist files.

**Categories:** Tesla charge port openers, Doorbells, Fans.

Imported from [UberGuidoZ/Flipper](https://github.com/UberGuidoZ/Flipper) (GPLv3). See [`subghz_playlist/SOURCES.md`](subghz_playlist/SOURCES.md) for attribution.

### Getting the databases onto your SD card

Copy the directories manually: `ir_database/` contents → `IR/`, `subghz_database/` contents → `SubGHz/`, `subghz_playlist/` contents → `SubGHz/playlist/`.

## Hardware

- **MCU:** STM32H573VIT6 (Cortex-M33, 250 MHz, 2 MB dual-bank flash, 640 KB RAM)
- **Display:** 128×64 monochrome (ST7586s)
- **WiFi/BT:** ESP32-C6 coprocessor (binary SPI protocol — see ESP32 firmware note below)
- **RF:** Si4463 sub-GHz transceiver (300–928 MHz)
- **NFC:** ST25R3916 (13.56 MHz)
- **RFID:** 125 kHz ASK/PSK reader with T5577 write support
- **IR:** TSOP38238 receiver + IR LED transmitter
- **CAN:** FDCAN1 on J7 header (requires external transceiver)
- **USB:** USB-C (CDC + MSC composite)
- **Storage:** microSD card
- **Hardware revision:** 2.x

> **ESP32 firmware required:** Hapax supports multiple ESP32-C6 coprocessor firmware
> variants. The most capable, ranked by number of capabilities (CAPS) supported
> out of the 21 currently defined:
>
> | Firmware | Caps supported | Notes |
> |----------|:---------------:|-------|
> | **[CD3 native binary RPC (bedge117/m1-esp32-brain)](https://github.com/bedge117/m1-esp32-brain)** | 17 / 21 (profile macro; see caveat) | Native ESP-IDF, no AT stack; not a fork of the AT-based firmware below. Includes WiFi join + 802.15.4. PMKID capture and ESP32 OTA self-update are **reserved protocol message IDs that are not yet implemented** in shipped releases (see caveat); WPA handshake capture is implemented but not yet self-reported via the capability bitmap. |
> | **[SiN360 ESP32](https://github.com/sincere360/M1_SiN360_ESP32/releases)** | 13 / 21 | Binary SPI; full sniffer/recon/station-scan/BLE feature set; no PMKID/handshake capture or OTA. |
> | **[dag T-800](https://github.com/dagnazty/ESP32-C6-ESP-AT_M1)** | 10 / 21 | AT commands over SPI; WiFi attacks (deauth, beacon spam, karma, evil portal, probe flood, PMKID grab), BLE Spam, AP scanning, network joining. No packet-monitor sniffers, station scan, or advanced BLE features. |
>
> **Caveat on CD3's "17/21":** that figure is the *reference/target* capability
> profile macro (`M1_ESP32_CAP_PROFILE_CD3`), used only as a conservative
> fallback when a CD3 device can't be live-probed. As of the 2026-07-21 review
> of public CD3 source, no shipped release actually self-reports OTA or PMKID
> support at runtime — see [`documentation/esp32_firmware.md`](documentation/esp32_firmware.md#wire-bits--cap_bitmap)
> for the verified per-message-ID status. Do not rely on CD3 OTA or PMKID
> until a release advertises those capability bits.
>
> See [`documentation/esp32_firmware.md`](documentation/esp32_firmware.md#capability-matrix-by-firmware-variant)
> for the full firmware comparison, AT command reference, and per-capability
> matrix. Other variants exist (**CD3-AT** base, neddy299 deauth, hapaxx11-caps)
> for development and testing. **CD3-AT** (the AT-based `bedge117/esp32-at-monstatek-m1`
> lineage) and **CD3** (the native `bedge117/m1-esp32-brain` binary-RPC firmware)
> are two separate, non-interoperable codebases from the same author — see
> [`documentation/esp32_firmware.md`](documentation/esp32_firmware.md#source-repository)
> for the naming disambiguation.
>
> **Note:** the compile-time CAPS profile macros (`M1_ESP32_CAP_PROFILE_*`) are
> only a conservative fallback used when a connected firmware can't be probed
> dynamically (e.g. `CMD_GET_STATUS`/`M1_RPC GET_STATUS` is unavailable or
> unimplemented). Whenever a firmware self-reports its capability bitmap at
> runtime, that live bitmap is always used instead of the fallback profile.
>
> Flash via **Settings → ESP32 Update** (OTA over SPI) or via esptool — no hardware changes required. The stock Espressif UART-based AT firmware is **not** compatible.
>
> Download the latest SiN360 binary from the [SiN360 ESP32 releases page](https://github.com/sincere360/M1_SiN360_ESP32/releases).

## Building

> **Most users don't need to build firmware.** CI automatically builds and
> publishes every non-docs-only merge to `main` as a GitHub Release. See [Flashing](#flashing)
> below to install a release.

### Prerequisites

- **ARM GCC 14.2+** with CMake and Ninja
- **Python 3** (for post-build CRC injection)

### Build with CMake

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The `POST_BUILD` step automatically runs `tools/append_crc32.py` to inject CRC and
Hapax metadata into the binary.

See [`DEVELOPMENT.md`](DEVELOPMENT.md) for detailed build environment setup,
alternative build methods (STM32CubeIDE, Make, macOS), and
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
| **Unity + ASan/UBSan** | `tests.yml` | 123 test files, 5,000+ test functions (Sub-GHz, WiFi, NFC, RFID, IR, BLE, crypto, and more) | Enforced (blocks PR) |
| **Doxygen** | `docs.yml` | Application source | Auto-deploy to Pages |

## Flashing

All firmware releases are published automatically to
[GitHub Releases](https://github.com/hapaxx11/M1/releases) by the CI/CD pipeline.
You never need to compile firmware yourself — just pick a method below.

### Via Web Updater (recommended for updates)

> 🚧 **Work in progress** — the Web Updater is functional for basic flashing but
> is still being refined.  SD card asset population and error recovery may not
> work in all cases.

The Web Updater is hosted on GitHub Pages and fetches firmware directly from GitHub
Releases.  Requires Hapax firmware already running on the M1 (connects over USB
Serial via the Hapax RPC interface).  For a first install from stock firmware, use
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

> 🚧 **Work in progress** — OTA download is functional but still being stabilised.
> Requires an ESP32 firmware that supports WiFi joining (SiN360 or dag T-800).

The M1 can download firmware updates over WiFi directly from GitHub Releases:

1. Connect to WiFi (WiFi → Networks → join a network)
2. Go to **Settings → FW Update → Download**
3. Browse available releases and select one to download

The firmware image is saved to the SD card (`Firmware/` directory).  After download,
you flash it via **Settings → FW Update → Install from SD** as a separate step —
the device does not reflash itself automatically.

### Via DFU Mode (recovery / first install)
1. Power off the M1 (Settings → Power → Power Off → Right Button)
2. Hold **Up + OK** for 5 seconds to enter DFU mode (screen stays dark)
3. Connect via USB-C
4. Use the DFU Flash page in [qMonstatek](https://github.com/bedge117/qMonstatek)

To exit DFU mode without flashing, hold **Right + Back** to reboot.

### ST-Link Connection

Connect to the GPIO header (pins 1-18):

| ST-Link | M1 GPIO Pin | Function |
|---------|-------------|----------|
| VCC (3.3V) | Pin 9 (+3.3v) | Power |
| GND | Pin 8 or 18 (GND) | Ground |
| SWDIO | Pin 11 (PA13) | Data |
| SWCLK | Pin 10 (PA14) | Clock |

### Quick Development Workflow

1. **Connect ST-Link** to GPIO pins
2. **Connect USB** for power and serial console
3. **Open serial terminal** (PuTTY/Tera Term) at **9600 baud** - keep open for logs
4. **Build firmware:**
   ```bash
   ./build
   ```
5. **Flash with STM32CubeProgrammer:**
    - Click **"Connect"**
    - Click **"Open File"** → Select `distribution/M1_v*.hex`
    - Click **"Program"**
6. **Reset via ST-Link:**
   - Click **"Reset"** button in STM32CubeProgrammer
   - **OR** use CLI command `reboot` in serial terminal

**Pro tip:** Keep the serial terminal open during testing to see debug messages in real-time.

If the device does not boot after programming:
- Use **Under Reset + Hardware reset** connect mode in STM32CubeProgrammer.
- If PC reads near `0xFFFFFFFE`, the mapped boot vector is invalid (often from flashing an image that does not match post-build CRC metadata). Rebuild with `./build clean` and reflash `distribution/M1_v*.hex`.

## Entering DFU Mode (Hardware Strap)

If you need to flash the firmware directly via USB using STM32CubeProgrammer (without an ST-Link), you must boot the device into DFU Mode using the hardware strap. The software menu option has been removed for reliability.

1. **Unplug the USB cable** from the M1.
2. **Press and hold the UP button** on the D-pad.
3. While holding UP, **plug the USB cable back in**.
4. You will hear a loud **"tick"** from the speaker. This confirms the hardware strap was detected and the device is now in DFU mode.
5. In STM32CubeProgrammer, select **USB** from the dropdown menu and click Connect.

**To exit DFU mode:** Simply unplug the USB cable and plug it back in without holding any buttons to boot into normal firmware.

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
