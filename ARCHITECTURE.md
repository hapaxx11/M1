<!-- See COPYING.txt for license details. -->

# M1 Project Architecture

## Overview

The M1 firmware is organized into the following main components:

| Directory | Description |
|-----------|-------------|
| `Core/` | Application entry, FreeRTOS tasks, HAL configuration |
| `m1_csrc/` | M1 application logic (display, NFC, RFID, IR, Sub-GHz, CAN, firmware update, scenes) |
| `Battery/` | Battery monitoring |
| `NFC/` | NFC (13.56 MHz) support |
| `lfrfid/` | LF RFID (125 kHz) support — 26 protocol decoders |
| `Sub_Ghz/` | Sub-GHz radio support — 99 protocol decoders, protocol registry |
| `Infrared/` | IR transmit/receive |
| `Esp_spi_at/` | ESP32 SPI AT command interface (host-side SPI master) |
| `Esp32_serial_flasher/` | ESP32 firmware flashing support (UART bootloader) |

> **ESP32-C6 firmware** is sourced from
> [`bedge117/esp32-at-monstatek-m1`](https://github.com/bedge117/esp32-at-monstatek-m1)
> (custom SPI AT build for M1).  See
> [`documentation/esp32_firmware.md`](documentation/esp32_firmware.md) for details.
| `USB/` | USB CDC and MSC classes |
| `Drivers/` | STM32 HAL, CMSIS, u8g2, and other drivers |
| `FatFs/` | FAT file system for storage |
| `Middlewares/` | FreeRTOS |
| `lib/furi/` | Furi compatibility layer (FuriString, FURI_LOG, furi_assert/check) |
| `ir_database/` | Pre-built IR remote files (Flipper `.ir` format) for SD card — 1,412 files |
| `subghz_database/` | Curated Sub-GHz signal files (Flipper `.sub` format) for SD card — 313 files |
| `subghz_playlist/` | Sub-GHz playlist files (`.txt` format) for SD card |
| `cad/step/` | Enclosure 3D CAD models (STEP format) |
| `tools/` | Build utilities — `append_crc32.py` (CRC injection), `png_to_xbm_array.py` (logo converter) |
| `scripts/` | Helper scripts |
| `cmake/` | CMake build configuration modules |
| `documentation/` | Project documentation (hardware specs, schematics, build guides) |

## Build System

- **CMake + Ninja** (primary): Uses `CMakeLists.txt` and `CMakePresets.json`
- **STM32CubeIDE:** Uses `.project` and `.cproject`
- **Make:** Uses `Makefile` (Linux)

## GitHub Infrastructure

Hapax is the only M1 firmware fork with a GitHub-first development model.  All
build, test, release, and documentation workflows are fully automated through
GitHub services:

| Service | Purpose | Configuration |
|---------|---------|---------------|
| **GitHub Actions** — `ci.yml` | Build check on pull requests to `main` | `.github/workflows/ci.yml` |
| **GitHub Actions** — `build-release.yml` | Auto-create GitHub Release with firmware artifacts on merge to `main` | `.github/workflows/build-release.yml` |
| **GitHub Actions** — `tests.yml` | Host-side unit tests (Unity + ASan/UBSan) on push/pull request events for matching paths | `.github/workflows/tests.yml` |
| **GitHub Actions** — `static-analysis.yml` | Cppcheck + MISRA analysis (on-demand) | `.github/workflows/static-analysis.yml` |
| **GitHub Actions** — `docs.yml` | Auto-deploy Doxygen API docs to GitHub Pages | `.github/workflows/docs.yml` |
| **GitHub Pages** — Web Updater | Browser-based firmware flashing tool at [`hapaxx11.github.io/M1`](https://hapaxx11.github.io/M1/) | `web-updater/` |
| **GitHub Pages** — API Docs | Doxygen-generated documentation | Auto-deployed by `docs.yml` |
| **GitHub Releases** | Versioned firmware downloads (`.bin`, `_wCRC.bin`) | Created by `build-release.yml` |
| **GitHub Security Advisories** | Vulnerability reporting | `.github/SECURITY.md` |

Other forks (Monstatek stock, bedge117/C3, sincere360/SiN360) distribute firmware
as manual local builds — typically shared as binary uploads on Discord or GitHub
Releases created by hand.  None have CI/CD pipelines, automated testing, or
browser-based flashing tools.

## Scene-Based Architecture

All modules with submenus use a stack-based scene manager:

- **Sub-GHz:** Custom scene manager (`m1_subghz_scene.h/c`) with 17 scenes and
  radio-specific event handling (protocol decode, hopper, raw capture).
- **All other modules:** Shared generic scene framework (`m1_scene.h/c`) with
  blocking delegates.  Scene files: `m1_<module>_scene.c`.
- **BadUSB / Apps:** Single-function modules, direct function calls (no scene manager).

### Module Scene Files

| Module | Scene file | Items |
|--------|-----------|-------|
| Sub-GHz | `m1_subghz_scene.c` + 11 sub-scenes | 11 menu items |
| RFID | `m1_rfid_scene.c` | 4–5 items |
| NFC | `m1_nfc_scene.c` | 7 items |
| Infrared | `m1_infrared_scene.c` | 3 items |
| GPIO | `m1_gpio_scene.c` | 5–6 items |
| WiFi | `m1_wifi_scene.c` | 4–6 items |
| Bluetooth | `m1_bt_scene.c` | 3–7 items |
| Games | `m1_games_scene.c` | 6 items |
| Settings | `m1_settings_scene.c` | 7 items + nested sub-menus |

## Saved Item Actions

Every module that loads files from SD card must provide a standardized set of
saved-item actions following the Flipper `*_scene_saved_menu.c` pattern.  This is
the **canonical UX standard** for the project, superseding other UX preferences
when they conflict.

**Core verbs** (required): Emulate/Send, Info, Rename, Delete.

See [`CLAUDE.md`](CLAUDE.md) § "Saved Item Actions Pattern" for the full
specification, optional verbs, implementation patterns per module, and rules
for new modules.

## Modularization Approach

When firmware source files grow to mix pure logic (parsers, protocol codecs,
data conversion, filter logic) with hardware-coupled code (HAL, RTOS, display),
the preferred approach is to **extract the pure logic into standalone `.c`/`.h`
compilation units** with clean interfaces.

**Successful extractions:**
- `Sub_Ghz/subghz_key_encoder.c/h` — KEY→RAW encoding from `m1_sub_ghz.c`
- `Sub_Ghz/subghz_raw_line_parser.c/h` — RAW_Data parsing from `m1_sub_ghz.c`
- `Sub_Ghz/subghz_raw_decoder.c/h` — offline decode from `m1_subghz_scene_saved.c`
- `Sub_Ghz/subghz_playlist_parser.c/h` — path remapping from `m1_subghz_scene_playlist.c`

**Decoupling technique:** When extracted logic needs hardware-side operations,
use a callback function pointer (`SubGhzRawDecodeTryFn`-style).  The caller
provides a thin adapter; the module never touches hardware directly.

See [`CLAUDE.md`](CLAUDE.md) § "Preferred Modularization Pattern" for the full
specification and rules.

## Test Architecture

Host-side unit tests live in `tests/` and build via CMake with the vendored
Unity framework (`tests/unity/`).  ASan + UBSan are enabled by default.

**Structure:**
- `tests/test_*.c` — one file per test suite (20+ suites)
- `tests/stubs/` — minimal header stubs for HAL, RTOS, FatFS, and peripheral
  types needed to compile firmware `.c` files on the host
- `tests/CMakeLists.txt` — one `add_executable` + `add_test` per suite

**Pattern:** Stub-based extraction — identify pure-logic functions in firmware
source files, stub out their HAL/RTOS header dependencies, and test with Unity.
This pattern covers file parsers (`flipper_rfid.c`, `flipper_ir.c`,
`flipper_nfc.c`, `flipper_subghz.c`), protocol codecs (`subghz_blocks_*`,
`lfrfid_manchester.c`), data conversion (`datatypes_utils.c`), and utility
logic (`m1_fw_source.c` asset filtering, `m1_json_mini.c`).

See [`CLAUDE.md`](CLAUDE.md) § "Preferred Unit Testing Pattern" for the full
specification with code examples and rules.
