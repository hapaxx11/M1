<!-- See COPYING.txt for license details. -->

# Changelog

All notable changes to the M1 project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- **Sub-GHz Scene Manager** — new Flipper-inspired scene architecture for the
  Sub-GHz UI.  Replaces the monolithic 6,456-line `m1_sub_ghz.c` state machine
  with a clean stack-based scene system where each screen owns its own
  `on_enter` / `on_event` / `on_exit` / `draw` callbacks.
  - **Scene framework**: `m1_subghz_scene.h/.c` — scene stack push/pop/replace,
    event dispatch, `SubGhzApp` context struct
  - **Button bar**: `m1_subghz_button_bar.h/.c` — standardized 3-column bottom
    bar, status bar, and RSSI bar renderers (fixed x-positions, consistent fonts)
  - **Menu scene**: streamlined 5-item entry menu (Read, Read Raw, Saved,
    Frequency Analyzer, Add Manually)
  - **Read scene**: protocol-aware receive with always-visible RSSI bar,
    scrollable signal history, green LED + beep on decode
  - **Read Raw scene**: raw RF capture with oscilloscope waveform, IDLE →
    RECORDING → STOPPED state machine with context-dependent bottom bar
  - **Receiver Info scene**: full signal detail (protocol, key, bits, TE, freq,
    RSSI, serial/rolling code) with Save and Send actions
  - **Config scene**: consistent UP/DOWN = select, LEFT/RIGHT = change value
    with visual `< value >` indicators; BACK always exits
  - **Save flow**: SaveName (VKB filename entry), SaveSuccess (auto-dismiss
    confirmation with green LED), NeedSaving ("Unsaved signals?" dialog)
  - **Saved scene**: file browser with Emulate/Rename/Delete action menu
  - **Freq Analyzer scene**: wrapper for existing `sub_ghz_frequency_reader()`
- **Sub-GHz entry point**: `sub_ghz_scene_entry()` — single function called
  from the main menu, replaces the 11-item submenu with the scene manager
- **Bridge functions**: `*_ext()` wrappers in `m1_sub_ghz.c` expose static
  radio control, RSSI read, protocol TX, and waveform functions to scene files

### Changed

- **C3 fork sync** — merged bedge117/M1 C3.12 (`bedge117/main`, `8842866`)
  using `ours` strategy to record ancestry.  C3 includes RTC/NTP sync,
  BadUSB forced type, PicoPass/iCLASS support, and 56 Sub-GHz protocols —
  most already present in Hapax or planned for cherry-pick.  No files
  changed; merge recorded for branch comparison and future cherry-picks.

- **Upstream sync** — merged Monstatek/M1 upstream (`monstatek/main`) using
  `ours` strategy to resolve GitHub's "behind" indicator.  Upstream commits
  (`bb619f9`, `217ca99`) add documentation files and a v0.8.0.1 Release
  binary — none applicable to the Hapax fork (ESP32 firmware is UART-based,
  Release/ binary already gitignored, documentation superseded by Hapax
  docs).  No files changed; merge recorded for branch comparison only.

### Added

- **Enclosure STEP files** — added 6 enclosure 3D CAD models (STEP AP214,
  rev 2025-02-26) under `cad/step/`.  Includes back case, front panel,
  front shell, mid case, direction button, and mid/back button.

### Changed

- Renamed `hardware/` directory to `cad/` to avoid case-insensitive name
  collision with `HARDWARE.md` (which upstream Monstatek/M1 places at
  repo root).

### Fixed

- **Sub-GHz recording stability** — removed protocol decoder polling
  (`subghz_decenc_read()`) from the `Q_EVENT_SUBGHZ_RX` handler in
  `subghz_record_gui_message()`.  Running the decoder plus a full display
  redraw on every RX batch competed with time-critical SD-card writes and
  could cause ring-buffer overflows and device reboots under sustained RF
  activity.  Decoding still works normally in Read and Weather Station modes.
  (Ref: bedge117/M1 C3.4 crash fix.)

### Changed

- **Documentation: Public Forks Tracker** — added a "Public Forks Tracker"
  section to `CLAUDE.md` with a table of all known Monstatek/M1 public forks,
  their latest commit hashes/dates (UTC), last-reviewed timestamps, and
  instructions for agents to discover new forks and keep the table current.

## [0.9.0.10] - 2026-04-02

### Removed

- **Release/ folder** — removed legacy STM32CubeIDE v0.8.0.0 build artifacts
  (binaries, makefiles, object lists) inherited from the upstream Monstatek repo.
  Hapax uses CMake + Ninja with `build/` output and GitHub Releases for distribution.
  Added `Release/` to `.gitignore` to prevent re-introduction.
- **RC version floor** — removed the temporary RC ≥ 10 floor guard from
  `build-release.yml` now that v0.9.0.10 has been published.

### Changed

- **Automatic releases on merge to main** — `build-release.yml` now triggers on
  every push to `main` (including PR merges) in addition to manual dispatch.
  Each merge auto-increments the version RC from the latest published release
  and creates a new GitHub Release with firmware artifacts.
- **Automatic pre-release flag** — versions with `FW_VERSION_MAJOR` < 1 are
  automatically marked as pre-release.  Manual dispatch can still force the
  flag for any version via the workflow input.

## [0.9.0.9] - 2026-04-01

### Added

- **CI build check** (`ci.yml`) — automated firmware build runs on every PR
  targeting `main` and on direct pushes to `main`. Ensures the project compiles
  and produces valid ARM artifacts before a PR can be merged. Pair with GitHub
  branch-protection "required status checks" to enforce the gate.

### Fixed

- Fixed CI build failure caused by `lib/furi/core/string.h` (FuriString shim)
  shadowing the system `<string.h>` header.  Removed `lib/furi/core` from the
  CMake include path; all furi headers are already reachable via the `lib/furi`
  include path using the `core/` prefix.

### Changed

- Updated Hapax H-crossbar mountain logo across all three display sizes:
  - **128x64** (splash screen): Wider H crossbar with vertical pillars clearly
    separated from the mountain slopes
  - **40x32** (boot logo): Mountain now floats on black background instead of
    sitting in a filled white rectangle; H crossbar with visible vertical pillars
  - **26x14** (status bar icon): H crossbar detail visible even at this small size
- Added `tools/png_to_xbm_array.py` — bidirectional PNG ↔ u8g2 XBM byte array
  converter for future logo editing. Supports img2c, c2img, and export-all modes.

## [0.9.0.8] - 2026-03-31

### Added

- **CAN Commander** — FDCAN1 support via J7 (X10) connector (PD0 RX / PD1 TX).
  Requires external 3.3 V CAN transceiver (recommended: Waveshare SN65HVD230 CAN Board).
  - **Sniffer** — real-time CAN bus monitor with scrolling message display, baud rate
    cycling (125 k / 250 k / 500 k / 1 Mbps), and buffer clear
  - **Send Frame** — manual CAN frame builder (ID, DLC, data) with one-tap transmit
  - **Saved** — placeholder for future .can log file support on SD card
  - Located under GPIO → CAN Bus in the menu system
  - Gated by `M1_APP_CAN_ENABLE` compile flag (enabled by default)
  - STM32 HAL FDCAN driver (`stm32h5xx_hal_fdcan.c/.h`) added from STM32CubeH5

- **Furi compatibility layer** (`lib/furi/`) — minimal shim that implements the
  subset of Momentum/Flipper's Furi runtime used by Sub-GHz protocol decoders.
  Enables near-direct porting of protocol code from Momentum-Firmware with minimal
  edits.  Provides:
  - `FuriString` — dynamic string type (`furi_string_alloc/free/cat_printf/get_cstr`)
  - `FURI_LOG_E/W/I/D/T` — logging macros mapped to M1's `m1_logdb_printf()`
  - `furi_assert` / `furi_check` / `furi_crash` — assertion macros
  - Common defines (`MAX`, `MIN`, `CLAMP`, `COUNT_OF`, `UNUSED`, etc.)
  - Aggregate `<furi.h>` header matching Momentum's include convention

- **Sub-GHz Phase 4 — Specialty protocols**: Treadmill37 (QH-433 OOK PWM, 37 bits),
  POCSAG (pager decode, auto-detects 512/1200/2400 baud, FSK/NRZ),
  TPMS Generic (catch-all TPMS Manchester decoder), PCSG Generic (pager Manchester
  catch-all).  All registered in `subghz_protocol_registry.c`.
- **Sub-GHz Phase 3 — Weather/Sensor protocols** (12 new decoders):
  Auriol AHFL (42-bit, PPM), Auriol HG0601A (37-bit, PPM), GT-WT-02 (37-bit, PPM),
  Kedsum-TH (42-bit temp/humidity, PPM), Solight TE44 (36-bit, PPM),
  ThermoPro TX-4 (37-bit, PPM), Vauno EN8822C (42-bit, PPM),
  Acurite 606TX (32-bit temp, PPM), Acurite 609TXC (40-bit temp/humidity, PPM),
  Emos E601x (24-bit, PWM), LaCrosse TX141THBv2 (40-bit, PWM),
  Wendox W6726 (29-bit, PWM).
- **Sub-GHz Phase 4 — Remote/Gate/Automation protocols** (5 new decoders):
  DITEC GOL4 (54-bit gate remote, PWM, dynamic/rolling),
  Elplast (18-bit remote, inverted PWM, static),
  Honeywell WDB (48-bit wireless doorbell, PWM w/ parity, static),
  KeyFinder (24-bit keyfinder tag, inverted PWM, static),
  X10 (32-bit home automation, PWM w/ preamble, dynamic).
- **Sub-GHz Phase 5 — Advanced weather protocols** (5 new decoders with CRC/checksum
  validation):
  Acurite 592TXR (56-bit, sum checksum + parity validation),
  Acurite 986 (40-bit, CRC-8 poly 0x07, LSB-first bit reversal),
  TX-8300 (72-bit, Fletcher-8 checksum + inverted-copy validation, uses 128-bit decoder),
  Oregon V1 (32-bit Manchester, byte-sum checksum with carry),
  Oregon 3 (32-bit Manchester inverted, preamble detection + nibble checksum).
- **`subghz_decode_generic_ppm()`** — generic PPM decoder utility for weather sensors,
  reducing boilerplate across PPM-based protocol decoders.
- **LF-RFID Phase 5 — Indala 224-bit**: PSK2 long-format Motorola card (28 decoded
  bytes).  Files: `lfrfid_protocol_indala224.{h,c}`.  Ported from Momentum Firmware.
- **LF-RFID Phase 5 — InstaFob**: Hillman Group Manchester-encoded fob (8 decoded
  bytes, Block 1 = 0x00107060).  Files: `lfrfid_protocol_insta_fob.{h,c}`.
  Ported from Momentum Firmware.

## [0.9.0.7] - 2026-03-31

### Changed

- **Sub-GHz protocol registry refactor** — replaced the manually-synchronised
  quartet of enum + `subghz_protocols_list[]` + `protocol_text[]` + switch-case
  dispatch with a single table-driven `SubGhzProtocolDef` registry
  (`Sub_Ghz/subghz_protocol_registry.{h,c}`).  Adding a new Sub-GHz protocol now
  requires only: (1) write the decode function, (2) add one entry to the registry
  array, (3) add the `.c` to CMakeLists.  No more switch-case edits or
  three-way manual sync.

- **Flipper/Momentum compatibility macros** — added `DURATION_DIFF()`,
  `subghz_protocol_blocks_add_bit()`, and `subghz_protocol_blocks_reverse_key()`
  helpers in the registry header so ported Flipper decode logic compiles with
  minimal changes.

- **Protocol metadata** — each protocol now carries Flipper-compatible type
  (`SubGhzProtocolTypeStatic` / `Dynamic` / `Weather` / `TPMS`), capability
  flags (frequency bands, AM/FM, save/send), and category filter alongside its
  timing parameters and name.

- **Documentation** — updated `flipper_import_agent.md` porting instructions to
  reflect the registry-based workflow, added Momentum Firmware as a reference
  source, updated checklist.

### Added

- **Flipper-compatible decoder building blocks** — new header files that mirror
  Flipper's `lib/subghz/blocks/` directory structure:
  - `Sub_Ghz/subghz_block_decoder.h` — `SubGhzBlockDecoder` state machine struct
    with `parser_step`, `te_last`, `decode_data`, `decode_count_bit` fields plus
    `subghz_protocol_blocks_add_bit()`, `add_to_128_bit()`, and `get_hash_data()`
    helpers.  Ported protocol decoders can use the exact same field names as
    Flipper source.
  - `Sub_Ghz/subghz_block_generic.h` — `SubGhzBlockGeneric` decoded output struct
    (`data`, `serial`, `btn`, `cnt`, `data_count_bit`) with a
    `subghz_block_generic_commit_to_m1()` bridge function that writes results
    into M1's global state after a successful decode.
  - `Sub_Ghz/subghz_blocks_math.h` — Flipper-named wrappers (`bit_read`,
    `bit_set`, `bit_clear`, `bit_write`, `DURATION_DIFF`,
    `subghz_protocol_blocks_crc8`, `_crc16`, `_parity8`, `_get_parity`,
    `_lfsr_digest8`, etc.) that delegate to M1's existing `bit_util.h`
    implementations.  Ported Flipper code calling these functions compiles
    without renaming.

- **Manchester codec building blocks** — new inline headers porting Flipper's
  Manchester encoder/decoder from `lib/toolbox/`:
  - `Sub_Ghz/subghz_manchester_decoder.h` — table-driven Manchester decoder
    state machine (`ManchesterEvent`, `ManchesterState`, `manchester_advance()`)
    used by Somfy Telis, Somfy Keytis, Marantec, FAAC SLH, Revers RB2, and
    other Manchester-coded protocols.
  - `Sub_Ghz/subghz_manchester_encoder.h` — Manchester encoder
    (`manchester_encoder_advance/reset/finish`) that converts data bits into
    Manchester-coded half-bit symbols for TX waveform generation.

- **TX encoder building blocks** — new inline headers porting Flipper's
  encoder infrastructure:
  - `Sub_Ghz/subghz_level_duration.h` — `LevelDuration` type (compact
    level + duration pair) used by encoder upload buffers and the TX ISR.
  - `Sub_Ghz/subghz_block_encoder.h` — `SubGhzProtocolBlockEncoder` struct
    for TX state tracking, MSB-first bit-array helpers
    (`set_bit_array`/`get_bit_array`), and `get_upload_from_bit_array()` to
    convert encoded bit arrays into `LevelDuration[]` waveforms.  All inline.

- **Generic decoder conversion** — both `subghz_decode_generic_pwm()` and
  `subghz_decode_generic_manchester()` now use the Flipper-compatible building
  blocks (`SubGhzBlockDecoder`, `SubGhzBlockGeneric`, `DURATION_DIFF`,
  `subghz_protocol_blocks_add_bit`, `manchester_advance`,
  `subghz_block_generic_commit_to_m1`).  Reads timing from the protocol
  registry directly, removing the legacy `subghz_protocols_list[]` dependency.
  All protocols that delegate to these generic decoders benefit automatically.

### Fixed

- **`.sub` file interop — registry-based protocol matching** — replaced the
  fragile `strstr()` chain for KEY-type `.sub` file playback with a
  registry-driven lookup.  `subghz_protocol_find_by_name()` resolves the
  protocol, checks the `SubGhzProtocolType`, and computes encoding timing
  from the registry's `te_short`/`te_long` ratio.  This fixes:
  - **Cham_Code** (Chamberlain) — previously fell through to "unsupported"
  - **Marantec24** — previously caught by rolling-code check via substring
    match on `"Marantec"` (which is Dynamic); Marantec24 is Static
  - **All static protocols** added in March 2026 (Clemsa, BETT, MegaCode,
    Centurion, Elro, Intertechno, Firefly, etc.) — previously missing from
    both strstr branches
  - Legacy strstr matching retained as fallback for protocol names not in
    the registry (e.g. third-party Flipper firmware captures)

- **`subghz_protocol_is_static()` — registry-driven** — replaced hardcoded
  23-entry switch-case with a single registry type check
  (`SubGhzProtocolTypeStatic`).  Any static protocol in the registry is now
  automatically eligible for TX emulation — no manual maintenance needed.

## [0.9.0.6] - 2026-03-30

### Changed

- **Hapax-branded logos** — updated all three display logos (`menu_m1_icon_M1_logo_1` 128×64
  splash, `m1_logo_40x32` boot, `m1_logo_26x14` menu icon) with the Hapax H crossbar design,
  replacing the original V-notch mountain silhouette.

## [0.9.0.7] - 2026-03-31

### Changed

- **Documentation: Flipper import agent — Flipper-weighted pattern adoption policy** — added a
  Pattern Adoption Policy section to `documentation/flipper_import_agent.md` that gives Flipper
  Zero/One and community forks (Momentum, Unleashed) higher weight than Monstatek/M1 patterns
  for protocol decoders, struct layouts, naming, and HAL decisions while the Monstatek stock
  firmware remains at version ≤ 1.0.0.0.  Added a Monstatek Pattern Deviation Log table to
  track all cases where Flipper patterns were adopted over divergent Monstatek patterns, for
  later reconciliation once Monstatek matures.  Updated Step 0 and the import checklist to
  reflect the new policy.
  
## [0.9.0.5] - 2026-03-28

### Added

- **PicoPass/iCLASS NFC support** — full read, write, and emulate support for HID
  iCLASS / PicoPass cards over ISO15693 PHY. Includes DES cipher for key
  diversification, poller for card reading with authentication, and listener for
  card emulation via NFC-V transparent mode. Eight new source files in
  `NFC/NFC_drv/legacy/picopass/`. Merged from bedge117/M1 (C3 fork).

- **RTC/NTP time synchronization** — new `wifi_sync_rtc()` function syncs the
  STM32 RTC via ESP32 SNTP (`pool.ntp.org`). New `m1_time_t` struct and
  `m1_get_datetime()` / `m1_set_datetime()` / `m1_get_localtime()` API in
  `m1_system.c/h`. Merged from C3 fork.

- **AES-256-CBC encryption with custom keys** — new `m1_crypto_encrypt_with_key()`
  and `m1_crypto_decrypt_with_key()` functions allow AES operations with
  externally-provided 32-byte keys (e.g. PicoPass diversified keys). Existing
  `m1_crypto_encrypt()` / `m1_crypto_decrypt()` refactored as thin wrappers.
  Merged from C3 fork.

- **BadUSB keyboard emulation API** — `badusb_send_key()`, `badusb_type_char()`,
  `badusb_type_string()` made public. New `badusb_type_string_forced()` auto-
  switches to HID mode, waits for USB enumeration, types the string, then returns.
  Merged from C3 fork.

- **Choice dialog UI** — new `m1_message_box_choice()` displays a multi-button
  dialog with cursor-based selection (LEFT/RIGHT/UP/DOWN to navigate, OK to
  select, BACK to cancel). Returns 1-based button index or 0 for cancel.
  Merged from C3 fork.

- **USB mode detection** — added `M1_USB_MODE_NORMAL`, `M1_USB_MODE_HID` defines
  and `m1_usb_get_current_mode()` inline helper to `m1_usb_cdc_msc.h`.
  Merged from C3 fork.

- **App API expansions** — 25+ new exported symbols for external ELF apps:
  RTC (datetime get/set, NTP sync), crypto (custom key encrypt/decrypt),
  display (choice dialog), USB HID (mode switch, key send, string type).
  `API_MAX_SYMBOLS` increased from 200 to 256. Merged from C3 fork.

## [0.9.0.4] - 2026-03-28

### Added

- **NFC Cyborg Detector** — new NFC tool that turns on the NFC field continuously
  so body implants with LED indicators light up when held near the M1's back. Press
  BACK to stop. Accessible from the NFC Tools submenu.

- **NFC Read NDEF** — new NFC tool that scans a T2T tag and parses its NDEF content
  (URI and Text records), displaying up to 4 lines of decoded text on screen. Supports
  both short-record (SR) and standard NDEF TLV framing.

- **NFC Write URL** — new NFC tool that prompts for a URL via the virtual keyboard and
  writes it as an NDEF URI record (https:// prefix) to an NTAG-compatible T2T tag,
  starting at user-data page 4. Includes a confirmation screen before writing.

- **IRSND RC6A protocol** — enabled RC6A infrared transmit support
  (`IRSND_SUPPORT_RC6A_PROTOCOL` 0→1 in `Infrared/irsndconfig.h`).

- **IRSND Samsung48 protocol** — enabled Samsung48 infrared transmit support
  (`IRSND_SUPPORT_SAMSUNG48_PROTOCOL` 0→1 in `Infrared/irsndconfig.h`).

### Changed

- **NFC Tools menu expanded 5→8 items** — "Cyborg Detector", "Read NDEF", and
  "Write URL" added after "Wipe Tag". Dispatch updated in both the UIView
  (`nfc_utils_kp_handler` cases 5–7) and the standalone `nfc_tools()` loop.

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

- **RAW waveform visualization** — press LEFT during recording to toggle a real-time pulse
  waveform display (like Flipper's Read RAW). Shows a scrolling mark/space waveform at
  500μs per column across the full 128px display width. The waveform renders marks (high)
  above and spaces (low) below a center reference line. Includes a sample counter and
  recording duration. Press BACK to return to the live/protocol view.

- **Protocol-specific TX emulation** — press RIGHT in the signal detail view to transmit a
  decoded static-code signal directly from the history, without needing a raw recording.
  The encoder generates OOK PWM pulse durations from the decoded key + protocol timing
  parameters and transmits at the signal's original capture frequency with 4 repeats.
  Supported for 23 static-code protocols: Princeton, CAME 12-bit, Nice FLO, Linear 10-bit,
  Holtek HT12E, Gate TX, SMC5326, Power Smart, Ansonic, Marantec, Firefly, Clemsa, Bett,
  Megacode, Intertechno, Elro, Centurion, Marantec 24-bit, HAY21, Magellan, Intertechno V3,
  Linear Delta3, and Roger. Rolling-code protocols (KeeLoq, Security+, etc.) are excluded
  and show no "Send" button. Region/FCC restrictions are enforced before transmitting.

### Changed

- **Continuous signal decoding** — the decoder now runs continuously during recording,
  capturing every decoded signal into the history buffer. Previously, decoding stopped
  after the first successful protocol match.

- **`SubGHz_Dec_Info_t` extended** with `serial_number`, `rolling_code`, and `button_id`
  fields, copied from the decoder control struct after each successful protocol decode.

- **`subghz_reset_data()` now clears extended fields** (`n32_serialnumber`,
  `n32_rollingcode`, `n8_buttonid`) to prevent stale data from persisting across decodes.

## [0.9.0.2] - 2026-03-28

### Changed

- **Homescreen layout updated to match SiN360 style** — now shows "MONSTATEK M1" on the first
  line, "v{major}.{minor}.{build}.{rc}" on the second line, and "Hapax" on the third line.
  Previously showed "M1 BY Hapax" and "v0.9.0.x-Hapax.x" (two lines only).

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
