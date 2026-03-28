<!-- See COPYING.txt for license details. -->

# Changelog

All notable changes to the M1 project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.9.0.3] - 2026-03-28

### Added

- **Sub-GHz Signal History** — the Read/Record screen now keeps a scrollable history of all
  decoded signals (up to 50 entries). Previously only the most recent decoded signal was
  displayed. Press UP during recording to open the history list, UP/DOWN to scroll, OK to
  view signal details, BACK to return to the live view. Duplicate consecutive signals are
  counted instead of creating separate entries.

- **Protocol-aware signal display** — decoded signals now show extended protocol fields
  (serial number, rolling code, button ID) when available. Protocols that populate these
  fields include KeeLoq, Security+ 2.0, Somfy Telis, Chamberlain, StarLine, FAAC SLH,
  Hörmann, and Schrader TPMS.

- **History count badge** — the live recording view shows a `[N]` badge indicating how
  many unique signals have been captured so far.

- **Signal detail view** — selecting a signal from the history list shows full protocol
  details including frequency, RSSI, timing element, and protocol-specific fields.

- **Frequency hopping / auto-detect** — enable "Hopping" in the Config screen to
  automatically cycle through 6 common frequencies (310, 315, 318, 390, 433.92, 868.35 MHz)
  during recording. The radio dwells 150ms per frequency and uses RSSI-based detection
  (threshold −70 dBm) to stay on a frequency when signal activity is detected. Each
  history entry records the frequency it was captured on. The display shows "Scanning..."
  with the current frequency in the top-right corner.

- **Save individual signals from history** — in the signal detail view (reached by pressing
  OK on a history entry), press DOWN to save the signal as a Flipper-compatible `.sub` file.
  A virtual keyboard prompts for the filename with a default based on the protocol name and
  key value. Saved files appear in `/SUBGHZ/` on the SD card and are compatible with Flipper
  Zero for replay or analysis.

### Changed

- **Continuous signal decoding** — the decoder now runs continuously during recording,
  capturing every decoded signal into the history buffer. Previously, decoding stopped
  after the first successful protocol match.

- **`SubGHz_Dec_Info_t` extended** with `serial_number`, `rolling_code`, and `button_id`
  fields, copied from the decoder control struct after each successful protocol decode.

- **`subghz_reset_data()` now clears extended fields** (`n32_serialnumber`,
  `n32_rollingcode`, `n8_buttonid`) to prevent stale data from persisting across decodes.

## [0.9.0.2] - 2026-03-27

### Fixed

- **SD card firmware update now works from stock Monstatek firmware** — the `_SD.bin` binary
  now includes a self-referential CRC at offset `0xFFC14` (the stock `FW_CRC_ADDRESS`) that
  satisfies the stock firmware's post-flash `bl_crc_check()` verification. Previously, the
  Hapax CRC extension magic sentinel (`0x43524332`) occupied that offset, causing the stock
  firmware to always fail the CRC comparison with "Update failed!".

### Changed

- **CRC extension block shifted from offset 20 → 24** in the 1 KB reserved area. The 4-byte
  stock-compatible CRC now occupies offset 20 (`0xFFC14`), and the Hapax CRC extension block
  (CRC2 magic + size + CRC) starts at offset 24 (`0xFFC18`). Hapax metadata moved from
  offset 32 → 36. All firmware readers (RPC, boot screen, CRC verifier) use the header
  constants and adapt automatically.

- **`bl_verify_bank_crc()` now supports both CRC formats** — when the CRC2 extension magic
  is not found at offset 24, the function falls back to the stock Monstatek CRC format
  (plain CRC at offset 20, covering the firmware + config struct). This enables correct
  rollback verification for both stock and Hapax firmware images in the inactive bank.

- **Rollback path uses `bl_verify_bank_crc()`** instead of the raw `bl_crc_check()` call
  in `m1_system.c`, which only understood the stock CRC format.

- **`append_crc32.py` now injects three CRC values**: the Hapax CRC (firmware-only region),
  the stock-compatible self-referential CRC (GF(2) linear solver), and the trailing file CRC.
  The self-referential CRC is verified immediately after computation to catch solver errors.

## [0.9.0.1] - 2026-03-27

### Changed

- **CI: Release title now "M1 Hapax vX.Y.Z"** — auto-generated GitHub release titles and
  release-body headings changed from `"M1 Firmware — v…"` to `"M1 Hapax v…"`, matching
  SiN360's `"SiN360 v0.9.0.4"` naming convention. The `release_name` workflow input still
  overrides the title when supplied manually.

- **Version scheme aligned with SiN360**: `FW_VERSION_MINOR` bumped from `8` to `9` and
  `FW_VERSION_RC` / `M1_HAPAX_REVISION` both start at `1`, matching SiN360's `0.9.x.x`
  versioning approach. First public release is `v0.9.0.1`; subsequent releases increment
  automatically. Release tag is a clean `v0.9.0.{RC}` (no `-Hapax.X` suffix in the tag
  or filename).

- **CI auto-increment**: The `build-release.yml` workflow now queries the latest published
  GitHub release tag matching `v0.9.0.*`, extracts the RC number, and patches only
  `FW_VERSION_RC` and `M1_HAPAX_REVISION` in `m1_fw_update_bl.h` before compiling.
  `CMakeLists.txt` is never patched — `CMAKE_PROJECT_NAME` is derived automatically from
  those header values at CMake configure time. Each CI build automatically produces the next
  sequential version with no manual edits required. Local builds use the source-file
  defaults (RC=1).

- **CMake project name fully dynamic**: `CMAKE_PROJECT_NAME` (`M1_Hapax_v{MAJOR}.{MINOR}.{BUILD}.{RC}`)
  is derived entirely at CMake configure time by reading the four `FW_VERSION_*` macros from
  `m1_fw_update_bl.h`. `CMakeLists.txt` never needs to be edited for a version bump — only
  the header is the single source of truth. All output files — ELF, BIN, HEX, and `_SD.bin`
  — derive their versioned names from this variable, eliminating the separate
  `M1_RELEASE_NAME` variable.

### Fixed

- **`m1_fw_update_bl.c` — SD-card flash now succeeds**: `bl_flash_binary()` was calling
  `bl_crc_check()`, which reads the "stored CRC" from `FW_CRC_ADDRESS` (`0x080FFC14`).
  Our binary places the CRC2 magic sentinel (`0x43524332`) there, not the CRC value itself,
  so `bl_crc_check()` always returned `BL_CODE_CHK_ERROR` and flashing appeared to succeed
  but then reported failure. Fixed by replacing the call with `bl_verify_bank_crc()`, which
  correctly interprets the CRC extension block (magic/size/crc at offsets 0/4/8 from
  `FW_CRC_EXT_BASE = 0x080FFC14`).

- **Documentation: `documentation/hardware_schematics.md`** — removed `NFC_CS` and `NFC_IRQ`
  from the 125 kHz LF-RFID signal table where they were incorrectly listed; added a note
  clarifying they belong to the 13.56 MHz HF/NFC transceiver on the main board.

- **signal_gen.c**: Replace undeclared `BUTTON_EVENT_HOLD` with `BUTTON_EVENT_LCLICK` (long-press
  enum value) for UP/DOWN navigation in the signal generator screen; fixes CI build failure.

### Changed

- **Documentation: `documentation/hardware_schematics.md`** — expanded from partial 2-sheet
  extract to full 7-sheet hardware inventory. Added IC designators for NFC board (U2–U7,
  Q4), crystal refs (Y1/Y2), SWD/USB-UART header pinouts, LCD1 spec (128×64, 1/65 duty),
  J4/J5/J6/J7 connector details, IR subsystem (U6, LD1/LD2, Q3), LED indicators (D11, IC1),
  complete power-rail summary, passive component groupings (R/C/L), test points, and
  mounting holes. Capability summary updated with IR TX/RX, RGB LED, FDCAN, and corrected
  component references throughout.

### Added

- **CI: Manual build-and-release workflow** (`.github/workflows/build-release.yml`):
  `workflow_dispatch`-only GitHub Actions workflow that installs the ARM GCC toolchain,
  builds the firmware with CMake + Ninja, injects CRC32 + Hapax metadata via
  `append_crc32.py`, and publishes a GitHub Release with `.elf`, `.hex`, raw `.bin`, and
  `_wCRC.bin` artifacts. Tag and release title are auto-derived from source versions but
  can be overridden as inputs.

### Changed

- **Renamed `--c3-revision` → `--hapax-revision`** in `tools/append_crc32.py` and all
  callers (`CMakeLists.txt`, `README.md`, `CLAUDE.md`). The old `--c3-revision` alias is
  preserved for backward compatibility with existing local scripts.
- **Updated Hapax metadata magic sentinel** from `0x43334D44` ("C3MD") to `0x48415058`
  ("HAPX") in `tools/append_crc32.py` and `m1_csrc/m1_fw_update_bl.h`. The C3 name was a
  carry-over from the merged C3 fork; the new value reflects the Hapax identity. Renamed
  the `c3_magic` / `c3_addr` local variables in `m1_csrc/m1_rpc.c` to `hapax_magic` /
  `hapax_addr` accordingly.

- **Music Player** (`Games` menu): plays Flipper Music Format (`.fmf`) files from
  `SD:/Music/`. Full FMF parser (BPM, Duration, Octave, Notes), buzzer-based playback
  with progress bar and per-note display. BACK button aborts playback.
- **Field Detect** (`NFC` menu): passive NFC (13.56 MHz) and LF-RFID (125 kHz) field
  detector. Displays real-time bar indicators for both technologies and beeps on first
  detection. Powered by `m1_field_detect.c`.
- **Signal Generator** (`GPIO` menu): continuous square-wave output via the buzzer timer.
  18 frequency presets from 200 Hz to 8 kHz; UP/DOWN to change frequency, OK to
  toggle on/off. Implemented in `signal_gen.c`.
- **Buzzer continuous-tone API**: `m1_buzzer_tone_start(freq)` and `m1_buzzer_tone_stop()`
  — start/stop a tone without an auto-stop timer, used by Music Player and Signal Gen.

### Changed

- Documentation governance: added "Documentation Update Rules" to `CLAUDE.md` and expanded
  `.github/GUIDELINES.md` with project-specific CHANGELOG format guidance; all future agent
  sessions are required to update `CHANGELOG.md`, `README.md`, and
  `documentation/flipper_import_agent.md` as part of every change

- Documentation: expanded `HARDWARE.md` with MCU specs, RAM, speed, and peripheral table;
  added secondary-reference note in `CLAUDE.md` pointing to new hardware schematics doc

### Added

- `documentation/hardware_schematics.md`: schematic-derived hardware reference covering ICs,
  power subsystem, RF subsystems, interface signals, and a capability assessment table mapping
  hardware evidence to firmware support status — intended as a secondary reference for AI
  agents determining potential device support beyond what official firmware exposes

**Flipper Zero Compatibility**
- Import and use Flipper Zero `.sub`, `.rfid`, `.nfc`, and `.ir` files directly from SD card
- Generic Flipper key-value file parser (`flipper_file.c`)

**Sub-GHz**
- 73 protocol decoders (up from stock): Princeton, CAME, Nice Flo, Keeloq, Security+ 1.0/2.0,
  Linear, Holtek, Hormann, Marantec, Somfy, Ansonic, BETT, Clemsa, Doitrand, Dickert, FireFly,
  CAME Twee/Atomo, Nice Flor S, Alutech AT-4N, Centurion, Kinggates Stylo, Megacode,
  Mastercode, Chamberlain 7/8/9-bit, Liftmaster 10-bit, Dooya, Honeywell, Intertechno, Elro,
  Acurite, Ambient Weather, Bresser 3ch/5in1/6in1, TFA Dostmann, Nexus-TH, ThermoPro TX-2,
  GT-WT03, Oregon v2, LaCrosse, Infactory, Scher-Khan Magicar/Logicar, Toyota, and all
  remaining Flipper gap protocols (17 newly added to reach 73 total)
- Spectrum Analyzer — visual RF spectrum display
- RSSI Meter — real-time signal strength indicator
- Frequency Scanner — find active frequencies
- Weather Station — decode Oregon v2, Acurite, LaCrosse, Infactory sensors
- Radio Settings — adjustable TX power, custom frequency entry
- Extended band support — 150, 200, 250 MHz bands added
- Record screen shows "Recording..." during capture, clears on stop
- Record complete screen shows "Recording complete" with Play/Reset/Save
- Transmit screen shows "Transmitting..." with OK to replay

**LF-RFID**
- 20+ protocol decoders: HID Generic, Indala, AWID, Pyramid, Paradox, IOProx, FDX-A/B,
  Viking, Electra, Gallagher, Jablotron, PAC/Stanley, G-Prox II, Keri, Nexwatch, Noralsy,
  Securakey, IDTECK, HID Extended, EM4100, H10301, and more
- Clone Card — write to T5577 tags
- Erase Tag — reset T5577 to factory defaults
- T5577 Info — read tag configuration block
- RFID Fuzzer — protocol testing tool
- Manchester decoder with carrier auto-detection (ASK/PSK)

**NFC**
- Tag Info — manufacturer lookup, SAK decode, technology identification
- T2T Page Dump — read and display Type 2 Tag memory pages
- Clone & Emulate — copy and replay NFC tags
- NFC Fuzzer — protocol testing tool
- MIFARE Classic Crypto1 support

**Infrared**
- Universal Remote Database — pre-built remotes for Samsung, LG, Sony, Vizio, Bose, Denon,
  and more (see `ir_database/`)
- Learn & Save — record IR signals and save to SD card
- Import Flipper Zero `.ir` files
- New database files use "IR library file" header matching Flipper format

**BadUSB**
- DuckyScript interpreter — run keystroke injection scripts from SD card
- Supports `STRING`, `DELAY`, `GUI`, `CTRL`, `ALT`, `SHIFT`, key combos, and `REPEAT`
- Place `.txt` scripts in `BadUSB/` on the SD card

**Bad-BT (Bluetooth)**
- Wireless DuckyScript — same scripting as BadUSB but over Bluetooth HID

**External Apps**
- ELF app loader — browse and launch `.m1app` files from the Apps menu

**WiFi**
- Scan — discover nearby access points
- Connect — join networks with password entry
- Saved Networks — manage stored WiFi credentials
- Status — view connection state, IP address, signal strength
- ESP32-C6 coprocessor via SPI AT interface (SPI Mode 1)

**NFC/RFID Field Detector**
- Detect external 13.56 MHz NFC and ~125 kHz RFID reader fields

**Bluetooth Device Manager**
- Scan, save, and manage BLE devices

**Dual Boot**
- Two firmware banks with CRC validation before boot
- Swap banks from menu or companion app; falls back to working bank on corruption
- DFU boot mode: hold Up+OK at power-on to enter STM32 ROM USB DFU bootloader

**RPC / Companion App**
- RPC protocol for [qMonstatek](https://github.com/bedge117/qMonstatek) desktop companion app
- Device info, firmware flash over USB, screen mirror, SD card browsing, WiFi management,
  ESP32 firmware update; `hapax_revision` field distinguishes Hapax builds from stock

**System / Stability**
- FreeRTOS heap increased from 120 KB to 256 KB
- Settings persistence — LCD brightness, southpaw mode, and preferences saved to SD card
- Southpaw mode — swap left/right button functions
- Safe NMI handler — proper ECC double-fault recovery with protected flash read helper
- Watchdog suspend/resume around long operations (flash, UART, firmware update)
- ESP32 serial flasher: MD5 verification, RX flush before retries, watchdog reset during TX
- Post-build CRC + Hapax metadata injection via `tools/append_crc32.py` (`--hapax-revision`)

### Fixed

- Crash on RFID read caused by rogue `pulse_handler` call in `TIM1_CC` ISR

---

## [1.0.0] - 2026-02-05

### Added

- Initial open source release
- STM32H573VIT6 support (2MB Flash, 100LQFP)
- Hardware revision 2.x support
