<!-- See COPYING.txt for license details. -->

# Changelog

All notable changes to the M1 project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Changed

- **Build output filenames simplified** — `CMAKE_PROJECT_NAME` changed from
  `M1_Hapax_v{MAJOR}.{MINOR}.{BUILD}.{RC}` to plain `M1_Hapax`.  Build
  outputs are now `M1_Hapax.elf`, `M1_Hapax.bin`, `M1_Hapax.hex`, and
  `M1_Hapax_wCRC.bin`.  Version numbers remain embedded in the firmware
  binary via `m1_fw_update_bl.h` defines — only the filenames changed.

### Added

- **Text Size setting (LCD & Notifications)** — New "Text Size" option with
  Small (default) and Large modes.  Large mode uses a clearer `spleen5x8`
  monospaced font and increases menu row height from 8px to 10px, improving
  readability for users with vision difficulties.  Affects all scene-based
  menus, Sub-GHz menu/config, and LCD settings.  Setting is persisted to SD
  card (`menu_style` key in `settings.cfg`).  Neither Flipper Zero nor
  Momentum firmware offers a comparable in-menu font/spacing toggle — this
  is a Hapax-original accessibility feature.

- **Battery indicator on splash screen** — The boot/welcome screen now shows a
  battery icon in the top-right corner with charge percentage and fill level.
  When charging (pre-charge or fast charge), a lightning bolt overlay appears
  inside the battery icon. Similar to Momentum firmware's splash screen.

- **WiFi Deauther** — ported WiFi deauthentication tool from neddy299/M1 fork.
  Accessible from WiFi → Deauther menu.  Flow: preflight AT command check →
  AP scan → AP select → station scan → station select → attack toggle.
  Requires custom ESP32-C6 AT firmware with `AT+DEAUTH` and `AT+STASCAN`
  command support.  Includes hidden advanced mode (UP UP DOWN DOWN) to cycle
  deauth attack techniques.  Adapted for Hapax conventions: scene-based menu
  integration as blocking delegate, `m1_esp32_deinit()` on all exit paths,
  no "Back" button bar labels.

- **ESP32 firmware OTA download** — new "Download" option in Settings → ESP32
  update menu.  Downloads ESP32-C6 AT firmware (`.bin` + `.md5`) from GitHub
  Releases to SD card (`0:/ESP32_FW/`), then user flashes via the existing
  Image File → Firmware Update flow.  Reuses the same `fw_sources.txt`
  configuration and GitHub Releases API infrastructure as M1 firmware download.
  Default sources: C3 ESP32 AT (`bedge117/esp32-at-monstatek-m1`) and
  Deauth ESP32 AT (`neddy299/esp32-at-monstatek-m1`).

- **Documentation**: ESP32 coprocessor firmware reference
  (`documentation/esp32_firmware.md`) — source repos, custom AT commands,
  SPI pin mapping, flash methods, build instructions.

- **Virtual keyboard — text-mode layout with SPACE key** — Added
  `m1_vkb_get_text()` function that provides a context-specific keyboard
  layout for general text input (passwords, device names).  The text-mode
  keyboard replaces the underscore key with a visible SPACE indicator (⎵)
  on lowercase and uppercase pages; underscore is moved to the symbols page
  (replacing pipe `|`).  Follows Flipper Zero's pattern of multiple keyboard
  layouts depending on context.  WiFi password entry and Bad-BT device name
  entry now use the text-mode keyboard.  Filename entry (`m1_vkb_get_filename`)
  retains the original layout without space.

- **Static analysis (on-demand)** — Added `static-analysis.yml` workflow running cppcheck
  on `m1_csrc/` and `Sub_Ghz/protocols/` with `--enable=warning,performance,portability`.
  Also runs cppcheck MISRA-C addon in advisory mode.  Triggered via `workflow_dispatch`
  (Actions tab) — not a required PR check.

- **Unit testing framework** — Added Unity test framework (v2.6.1) with host-side
  CMake build in `tests/`.  Initial test suite covers `bit_util.c` (33 tests):
  CRC-4/7/8/16, parity, reverse/reflect, XOR/add, CCITT/IBM whitening, LFSR digest,
  and UART extraction functions.

- **Address Sanitizer builds** — Host-side test build enables `-fsanitize=address,undefined`
  by default, catching memory bugs (buffer overflows, use-after-free, undefined behavior)
  at test time without requiring hardware.

- **Doxygen documentation** — Added `Doxyfile` and `docs.yml` workflow to auto-generate
  API documentation from source comments and publish to GitHub Pages.

- **Unit test CI** — Added `tests.yml` workflow running host-side tests with ASan+UBSan
  on every PR and push that touches source files.

- **cppcheck suppressions** — Added `.cppcheck-suppressions.txt` for vendor HAL and
  expected embedded firmware patterns.

- **IR Saved File Actions menu** — Pressing LEFT on the IR commands list now opens
  a file-level action menu with Send All (transmit every command sequentially),
  Info (file name, command count, parsed/raw breakdown, protocol), Rename, and
  Delete.  This brings IR in line with the Flipper `infrared_scene_saved_menu.c`
  pattern already used by Sub-GHz, NFC, and RFID.

- **Sub-GHz Saved Signal Info screen** — The Sub-GHz saved file action menu now
  includes an Info item that loads the `.sub` file and displays protocol name,
  key value, bit count, timing element, frequency, and modulation preset.
  Supports both parsed and raw signal types.

### Changed

- **Build: suppress `-Woverlength-strings` for vendored u8g2 font data** —
  Added per-file `COMPILE_OPTIONS` in `cmake/m1_01/CMakeLists.txt` to silence
  the harmless ISO C99 portability warning on `u8g2_fonts.c`.

- **CI: automatic changelog version stamping** — The `build-release.yml`
  workflow now automatically replaces `## [Unreleased]` in `CHANGELOG.md`
  with the release version and date (e.g. `## [0.9.0.56] - 2026-04-10`)
  after each successful release, and inserts a fresh empty `## [Unreleased]`
  heading.  This prevents changelog entries from accumulating indefinitely
  under the `[Unreleased]` heading.

- **Firmware download source config** — `fw_sources.txt` now supports a
  `Category:` field (`firmware` or `esp32`) to distinguish M1 firmware
  sources from ESP32 AT firmware sources.  Existing configs without a
  `Category:` field default to `firmware`.

- **Sub-GHz Decode action for saved RAW files** — The Saved scene action menu
  now shows a "Decode" option (first item) for RAW `.sub` files.  Selecting it
  feeds the raw pulse timing data through all registered protocol decoders
  offline (no radio needed) and displays any matched protocols with key, bit
  count, TE, and frequency.  Multiple decoded packets are shown in a scrollable
  list with detail view.  Inspired by Momentum firmware's decode feature.

- Documentation: added mandatory bug-fix regression test policy to CLAUDE.md,
  DEVELOPMENT.md, and .github/GUIDELINES.md — every bug fix must include unit tests
  that fail before the fix and pass after it

- Documentation: added explicit "no duplicate subsection headings" rule to CLAUDE.md
  and GUIDELINES.md changelog instructions — agents must scan the entire `[Unreleased]`
  block for an existing heading before creating a new one

- **Documentation: font inventory in CLAUDE.md** — Added a comprehensive font
  inventory table listing the 22 u8g2 fonts linked/used by application code,
  their display-role macros, BadUSB API availability, ascent values, and
  lowercase support.  Includes u8g2 suffix reference and font maintenance
  rules so future agents keep the table current when fonts are added,
  removed, or reassigned.

- **CI workflow no longer runs on push to main** — Removed the `push`
  trigger from `ci.yml` so the CI Build Check only runs on pull requests.
  Previously, every push to `main` triggered both CI Build Check and
  Build and Release in parallel, producing two identical firmware builds.
  Build and Release already validates compilation, so the CI run was
  fully redundant.  This halves the compute usage on push-to-main events.

- **Sub-GHz Read Raw waveform — oscilloscope-style rendering** — Rewrote
  `subghz_raw_waveform_draw()` from filled vertical bars to a proper
  square-wave oscilloscope trace.  High/low signal levels are now drawn
  as 2px-thick horizontal rails with crisp vertical transition edges.
  Added subtle dashed grid reference lines at the high, center, and low
  positions for visual context.  Adjusted the waveform area geometry
  (Y=15, H=36) so it no longer overlaps the RSSI bar.

- **Sub-GHz Saved / Playlist scenes skip straight to file browser** — Removed
  the intermediate "Press OK to browse" prompt screen.  Entering either scene
  now opens the SD card file browser immediately.  BACK from the action menu
  (Saved) or playback view (Playlist) re-opens the browser; BACK from the
  browser returns to the Sub-GHz menu.  After Rename or Delete, the file
  browser reopens automatically per the Saved Item Actions Pattern.

- **File browser lists directories before files** — The SD card file browser
  now sorts entries with directories first, then files, each group sorted
  alphabetically (case-insensitive).  This matches Flipper Zero / Momentum
  firmware behaviour and prevents subdirectories (e.g. `playlist/`) from
  appearing in the middle of signal files.

- **Hapax logo — H inside diamond** — Updated all three M1 logo bitmap arrays
  (128×64 splash, 40×32 boot, 26×14 status bar) in `m1_display_data.c` with
  Hapax H-mark variants.  The Hapax "H" letterform is carved into the center
  diamond of the M mountain logo by clearing specific pixels to form two vertical
  legs and a horizontal crossbar.  The diamond outline and mountain slopes are
  unchanged from stock.  PNG previews extracted from the C byte arrays are in
  `documentation/logo_previews/`.

- **Settings — LCD & Notifications menu** — Removed the bottom instruction bar
  ("U/D=Sel L/R=Change") that wasted 12px of screen space.  Now uses the same
  layout as the Sub-GHz Config scene: title with separator line, 8px rows showing
  all 5 items at once (no scrolling needed), and `< value >` arrows on the selected
  item to indicate L/R cycling.  Reclaims the bottom 12px and eliminates the
  3-item scrolling window.

- **Settings — About screen** — Removed the bottom Prev/Next inverted bar.
  L/R navigation is intuitive; replaced with a subtle centered `< 1/3 >` page
  indicator.  Added title header with separator line for visual consistency.
  Content repositioned to use the full display area.

- **Standardized Saved Item Actions pattern** — All four modules (Sub-GHz, IR,
  NFC, RFID) now implement the core saved-item verbs: Emulate/Send, Info,
  Rename, Delete.  Documented the canonical pattern in CLAUDE.md under
  "Saved Item Actions Pattern" so future modules follow the same structure.

- **Documentation: Saved Item Actions as canonical UX standard** — The Saved Item
  Actions Pattern is now the highest-priority UX standard across all docs.
  Updated CLAUDE.md (precedence note), ARCHITECTURE.md (new section),
  DEVELOPMENT.md (mandatory for new modules), flipper_import_agent.md (Pattern
  Adoption Policy table: scene UX follows Flipper), and .github/GUIDELINES.md
  (new § 11 UX Pattern Standards).  Previously defined UX preferences (button bar
  rules, display layout) still apply when not superseded by this pattern.

- **IR commands bottom bar** — The bottom bar now shows "< More" (for file
  actions) and "Send >" instead of the generic "Open" label.

- **NFC menu: merged "Tools" into "Extra Actions"** — The top-level NFC menu
  had both "Extra Actions" and "Tools" submenus, which is inconsistent with
  Flipper Zero and Momentum firmware (both only have "Extra Actions").  All
  eight former Tools items (Tag Info, Clone Emulate, NFC Fuzzer, Write UID,
  Wipe Tag, Cyborg Detector, Read NDEF, Write URL) are now part of the
  Extra Actions submenu.  The NFC main menu is now 6 items: Read, Detect Reader,
  Saved, Extra Actions, Add Manually, Field Detect.

- **NFC post-read submenu: renamed "Utils" to "Card Actions"** — The contextual
  submenu shown after reading or loading an NFC card (More Options → Utils) has been
  renamed to "Card Actions" for clarity, since these tools operate on the
  currently-loaded card data.

- **Removed ESP32 boot-time auto-init** — The `m1_esp32_auto_init` setting
  (Settings → System → "ESP32 at boot") was a Hapax addition that stock firmware
  does not have.  ESP32 is now always initialized on-demand when a WiFi, BT, or
  802.15.4 function is first selected, matching stock Monstatek behaviour.  The
  "System Settings" screen and its SD card persistence key (`esp32_auto_init`)
  have been removed.  Existing settings files with the key are harmlessly ignored.

- **Sub-GHz Config scene: add TX Power and ISM Region settings** — The legacy
  `sub_ghz_radio_settings()` screen (stock Monstatek) exposed TX Power, Modulation,
  and ISM Region.  The scene-based Config only had Frequency, Hopping, Modulation,
  and Sound — TX Power and ISM Region were inaccessible.  Now all 6 settings are
  available in the scene Config screen with scrollable navigation.  ISM Region
  changes are persisted to SD card on exit via `settings_save_to_sd()`.  TX Power
  changes are applied via new `_ext` accessor functions that bridge the static
  `subghz_tx_power_idx` to scene code.

- **Documentation overhaul** — comprehensive audit and update of all markdown files
  for consistency with codebase state and completed PRs. README.md rewritten with
  comparison table vs stock firmware, accurate protocol counts, missing features
  (playlist, add manually, AES crypto, NTP sync), included databases section, and
  expanded SD card layout. Updated DEVELOPMENT.md with full build environment and
  architecture overview. Expanded ARCHITECTURE.md with missing directories and
  module scene table. Fixed placeholder URLs in CONTRIBUTING.md, CODE_OF_CONDUCT.md,
  SECURITY.md. Fixed outdated version label in GUIDELINES.md. Consolidated duplicate
  CHANGELOG [0.9.0.7] block. Removed completed subghz_improvement_plan.md.

### Fixed

- **Splash screen: "M1" no longer looks like "MI"** — Changed the splash
  screen font (`M1_POWERUP_LOGO_FONT`) from `u8g2_font_tenthinnerguys_tr`
  to `u8g2_font_helvB08_tr`.  The old font's '1' glyph was a featureless
  2-pixel vertical bar indistinguishable from a capital I at a glance.  The
  new font's '1' has a clear top hook, making "M1 Hapax" immediately
  readable.  Both fonts are bold sans-serif with similar size (ascent 8 vs 9)
  and near-identical string width (51 vs 52 px), so the overall look is
  preserved.

- **Sub-GHz Decode: deduplicate repeated transmissions** — RAW `.sub` files
  typically contain multiple copies of the same transmission (remotes repeat
  3–5 times).  The offline decoder now skips duplicate protocol+key pairs so
  the results list shows each unique signal once, making decoded output much
  clearer.

- **Sub-GHz Decode: sync `npulsecount` before calling protocol decoders** —
  The offline decoder now sets `subghz_decenc_ctl.npulsecount` before invoking
  protocol decoders, matching the live pulse handler path and preventing
  potential mismatches if a decoder reads the global counter directly.

- **Sub-GHz Read scene: unnecessary full radio reset on child scene return** —
  Returning from Config, ReceiverInfo, or SaveName back to the Read scene
  previously called `menu_sub_ghz_init()` (full SI4463 power-cycle + patch load)
  and `subghz_pulse_handler_reset()` (wipes accumulated pulse state), even though
  the radio was only in SLEEP mode.  Now uses a lightweight `resume_rx()` path
  that skips the expensive reset and preserves decode state.

- **Sub-GHz Read scene: hopper frequency resets to index 0 after Config** —
  Opening and closing Config while frequency hopping was active would restart
  the hopper from frequency index 0.  Now preserves the hopper position
  (`hopper_idx` and `hopper_freq`) across child scene navigation.

- **Sub-GHz Read scene: L/R frequency change power-cycles the radio** —
  Pressing Left/Right to change frequency during active RX previously did a full
  `stop_rx()` + `start_rx()` cycle (TIM1 deinit/init, `menu_sub_ghz_init()`,
  pulse handler reset).  Now uses `subghz_retune_freq_hz_ext()` for a lightweight
  retune — the same mechanism the frequency hopper uses internally — keeping the
  capture pipeline intact.

- **OTA "Fetching releases" always returns "No releases found"** — `http_get`
  used `AT+HTTPCLIENT` which has a 2 KB receive-buffer limit in the ESP32-C6
  SPI AT firmware (`CONFIG_AT_HTTP_RX_BUFFER_SIZE=2048`).  A GitHub releases
  API response is 2–20 KB depending on the number of releases and the length
  of release notes, so the AT command either errored or returned truncated
  JSON that could not be parsed.  A secondary bug in the `AT+HTTPCLIENT`
  format string (`"2,0,\"%s\",,,,%d"` — one extra comma) placed the HTTPS
  transport-type argument in the wrong parameter slot, causing the connection
  to default to plain HTTP, which is rejected by `api.github.com`.  Fixed by
  rewriting `http_get` to use the same raw TCP/SSL path (`AT+CIPSTART` →
  `AT+CIPSEND` → `AT+CIPRECVDATA`) that `http_download_to_file` already uses,
  which has no AT-layer body-size limit.  The GitHub API response buffer
  (`s_api_buf` in `m1_fw_source.c`) was also increased from 4 KB to 12 KB to
  hold 3–5 releases without truncation.  `http_get` now also sends the
  mandatory `User-Agent: M1-Hapax/1.0` and `Accept: application/json` headers
  that the GitHub API requires.

- **Splash screen showing "M1 H" instead of "M1 Hapax"** — Changed
  `M1_POWERUP_LOGO_FONT` from `u8g2_font_tenthinnerguys_tu` (uppercase only,
  glyphs 32-95) to `u8g2_font_tenthinnerguys_tr` (restricted, glyphs 32-127)
  so lowercase letters in "Hapax" are rendered correctly.

- **802.15.4 (Zigbee/Thread) ESP32 resource leak** — Added `m1_esp32_deinit()`
  to all four exit paths in `ieee802154_scan()`.  Previously, every Zigbee/Thread
  scan left the ESP32 SPI transport initialized on exit, wasting power and
  potentially interfering with subsequent WiFi/BT operations.

- **BLE Spam ESP32 resource leak** — Added `m1_esp32_deinit()` to both the
  early return (ESP32 not ready) and normal exit paths in `ble_spam_run()`.

- **Bad-BT ESP32 resource leak** — Added `m1_esp32_deinit()` to all five exit
  paths in `badbt_run()` (ESP32 not ready, BLE HID init failure, connection
  timeout, SD card error, and normal exit).  Previously only `ble_hid_deinit()`
  was called, leaving the underlying SPI transport active.

- **WiFi disconnect early return without ESP32 deinit** — Added
  `m1_esp32_deinit()` to the early return path in `wifi_disconnect()` when
  `wifi_ensure_esp32_ready()` fails.  The ESP32 may have been partially
  initialized before the failure.

- **NFC worker ignoring Q_EVENT_NFC_STOP** — Added handler for
  `Q_EVENT_NFC_STOP` in the NFC worker state machine's `NFC_STATE_PROCESS`
  case.  Previously this event was silently discarded, leaving the worker
  stuck in PROCESS state — `nfc_deinit_func()` never ran and `EN_EXT_5V`
  was never deasserted.  Two call sites in `m1_nfc.c` (unlock read and
  unlock with reader) send this event on user cancellation.

- **IR Send All skipping deinit on timeout** — Changed the Send All loop in
  `m1_ir_universal.c` to always call `infrared_encode_sys_deinit()` after
  each command, regardless of whether `Q_EVENT_IRRED_TX` was received.
  Previously, a 3-second timeout or unexpected event left the IR hardware
  initialized, and the next `transmit_command()` called
  `infrared_encode_sys_init()` on already-initialized hardware.

- **Sub-GHz Read Raw not capturing signals** — Fixed critical initialization ordering
  bug in Read Raw scene where `sub_ghz_tx_raw_deinit_ext()` was called AFTER starting
  the radio in RX mode.  The deinit function internally calls
  `sub_ghz_set_opmode(ISOLATED)` which puts the SI4463 to SLEEP, completely undoing
  the RX start.  TIM1 captured zero edges because GPIO0 outputs nothing while the
  radio is asleep.  Moved the TX cleanup to before the RX initialization sequence.

- **Sub-GHz Read and Read Raw radio recovery after blocking delegates** — Added
  `menu_sub_ghz_init()` (full radio PowerUp + config reset) at the start of both
  Read and Read Raw RX initialization, matching the Spectrum Analyzer's pattern.
  Previously, after a blocking delegate (Spectrum Analyzer, Freq Scanner, etc.)
  exited and powered off the radio via `menu_sub_ghz_exit()`, Read/Read Raw relied
  on `radio_init_rx_tx()`'s retry loop to recover the powered-off SI4463.  The
  explicit reset ensures the radio is always in a clean, known state before
  starting RX.

- **Sub-GHz Playlist inter-item latency** — Added `menu_sub_ghz_init()` after each
  `sub_ghz_replay_flipper_file()` call in the playlist transmitter.  Without this,
  the radio was left powered off between playlist items (replay calls
  `menu_sub_ghz_exit()`), forcing the next file's TX setup to recover from a dead
  radio via `radio_init_rx_tx()`'s retry loop — adding ~100ms latency per item.

- **Sub-GHz 2FSK replay at all frequencies** — Emulate now correctly uses FSK
  modulation when replaying Flipper .sub files with `Preset: 2FSKDev238Async` (or
  any FSK preset) at any frequency.  Previously, standard band configs (300–433.92
  MHz) always forced OOK modulation and the CUSTOM band handler only checked FSK
  for frequencies ≥ 850 MHz.  The fix forces CUSTOM band mode when FSK is detected,
  loads the 915 FSK radio config, and retunes to the target frequency.

- **Sub-GHz CUSTOM band antenna switch** — Frontend (antenna path) for CUSTOM
  band frequencies is now selected based on the actual target frequency, not the
  base config band.  Fixes incorrect antenna path when the radio config base band
  differs from the target frequency (e.g. 915 FSK config loaded for 433 MHz FSK).

- **Sub-GHz preset parser consistency** — `flipper_subghz_preset_to_modulation()`
  now uses case-insensitive matching and recognises ASK presets, matching the replay
  function's `stristr()` approach.

- **Sub-GHz Read Raw — RSSI feedback during recording** — RSSI bar now updates
  on every 200ms display refresh (not only when ring-buffer flush events arrive),
  providing continuous signal-strength feedback while recording.

- **Sub-GHz Read Raw — sample count display** — Shows exact sample count when
  below 1000 instead of always displaying "0k".

- **Sub-GHz Read Raw — sample count accuracy** — Fixed overcounting in the flush
  function which added the total available ring-buffer depth instead of the 512
  samples actually consumed per flush.

- **Sub-GHz Emulate — native .sgh file support** — Emulate now works with M1's
  own .sgh recording files (Read Raw output), not only Flipper .sub files.  Added
  parsing for `Modulation:` header and `Data:` lines in M1 native format.

- **Sub-GHz Emulate — error feedback** — Emulate now displays an error message
  box when replay fails (file not found, no data, unsupported protocol, etc.)
  instead of silently returning to the saved menu.

- **Sub-GHz Emulate — radio state after replay** — Radio hardware is re-initialized
  after emulate returns, preventing subsequent Read/Read Raw from failing because
  the SI4463 was left powered off.

- **IR database — Windows reserved filename** — Renamed `ir_database/AC/AUX.ir`
  to `AUX_.ir`.  `AUX` is a reserved device name on Windows, causing
  `error: invalid path` on checkout and reset operations.

- **NFC Amiibo emulation — Nintendo Switch compatibility** — Fixed T2T card
  emulation failing on Nintendo Switch (and other readers with anti-clone
  detection).  The Switch sends HALT followed by WUPA ~1ms later; the M1
  was not entering SLEEP_A during T2T emulation because the SLP_REQ handler
  was guarded with `Emu_GetPersona() != EMU_PERSONA_T2T` and was unreachable
  behind the T2T handler that forwarded ALL commands (including HALT) to
  nfc_listener.c.  Reordered the check chain in `rfal_nfc.c` so SLP_REQ is
  detected before the T2T persona dispatch, and added fast HALT detection in
  `rfal_rfst25r3916.c` at the RFAL driver level to immediately transition to
  SLEEP_A within the ISR context, meeting the Switch's tight ~1ms WUPA timing.
  Cherry-picked from sincere360/M1_SiN360 v0.9.0.5.

- **Sub-GHz raw recording (Read Raw)** — Fixed raw signal recording which was
  completely non-functional.  The Read Raw scene set up the radio and ISR for
  capture but never opened an SD card file, never read captured pulses from the
  ring buffer, never saved data, and never updated the waveform display or
  sample count.  Matching C3's working implementation: recording now streams
  data to an `.sgh` file on SD card in real-time, the oscilloscope waveform
  updates live during capture, and the sample counter tracks progress.  After
  stopping, DOWN/Save shows the auto-generated filename.  Previously, pressing
  Save after recording pushed to the protocol-decode save scene which
  immediately exited because no decoded protocol data existed.

- **ESP32 screen responsiveness** — Reduced AT readiness probe timeout from
  2s→1s per probe and inter-probe delay from 500ms→200ms, cutting worst-case
  ESP32 initialization blocking from ~25s to ~12s.  Added separate 5s timeout
  for AT mode-set commands (AT+CWMODE, AT+BLEINIT) instead of reusing the full
  30s/10s scan timeout, so a non-responsive ESP32 fails fast on initial setup.
  Added stale button event draining after all blocking ESP32 init and scan
  phases across WiFi, Bluetooth, and 802.15.4 modules so the interactive
  result loops start with clean queues.  BACK button pressed during ESP32
  init is now detected and exits immediately instead of being silently lost.

- **WiFi/BT scan regression** — Removed the AT readiness probe added in v0.9.0.30
  (`spi_AT_send_recv("AT\r\n")` loop at the end of `esp32_main_init()`).  The probe
  sent AT commands to the ESP32 during its boot sequence, before the AT command
  processor was fully initialized; this could corrupt the SPI command/response state
  and leave the transport unusable for subsequent mode-set commands.  Also reverted
  the 5-second `MODE_SET_RESP_TIMEOUT` that replaced the full scan timeout for
  `AT+CWMODE` / `AT+BLEINIT` — the short timeout gave insufficient recovery time
  when the ESP32 was slow to respond.  Mode-set commands now use the caller's full
  scan timeout (30 s WiFi, 10 s BLE) as in v0.9.0.29, while the actual scan command
  still gets a fresh full timeout via the `saved_scan_timeout` + `tick_t0` reset
  pattern.  Button event draining and queue resets (added in v0.9.0.40) are retained.

- **ESP32 screen responsiveness** — Added stale button event draining after all
  blocking ESP32 init and scan phases across WiFi, Bluetooth, and 802.15.4 modules
  so the interactive result loops start with clean queues.  BACK button pressed during
  ESP32 init is now detected and exits immediately instead of being silently lost.

- **Sub-GHz scene display cleanup** — Fixed overlapping text and tight spacing
  across multiple Sub-GHz scenes:
  - **Need Saving dialog**: Moved dialog text and choice buttons up to prevent
    the "before exiting?" text from overlapping the Save/Discard button boxes.
  - **Saved action menu**: Reduced item height from 13px to 12px so the last
    menu item (Delete) no longer extends past the 64px screen boundary.
  - **Receiver Info**: Shortened frequency label and right-aligned RSSI text
    to prevent overlap when long frequency values are displayed.
  - **Playlist**: Moved progress bar from y=48 to y=46 for proper spacing
    above the bottom button bar.
  - **Read detail view**: Tightened inter-line spacing by 1px so the optional
    "Received x#" line no longer sits at the exact button bar boundary.

- **Sub-GHz Config screen layout overlap** — The 5th config row (TX Power)
  overlapped the bottom button bar because 5 rows × 9px starting at y=12
  pushed the last row into the bar area (y=52).  Removed the unnecessary
  button bar (the `<` `>` arrows on selected items already indicate L/R
  changes values), added a separator line below the title, and switched to
  8px rows matching the menu scene layout.  All 6 config items now fit
  cleanly without scrolling.

- **WiFi/BT scan failure after ESP32 cold init** — After `esp32_main_init()`
  the SPI transport layer was ready but the ESP32-C6 AT command processor had
  not finished booting its WiFi/BLE stacks.  The first AT command
  (`AT+CWMODE=1` for WiFi, `AT+BLEINIT` for BT) was sent before the slave
  could process it, causing a timeout and "Failed. Retrying..." on every first
  scan attempt.  Added an AT readiness probe loop in `esp32_main_init()` that
  sends `AT\r\n` and waits for `OK` (up to 10 × 500 ms) before marking init
  complete — all subsequent callers (WiFi scan, BLE scan, 802.15.4, etc.) now
  start with a confirmed-ready ESP32.

- **Misleading "Failed. Retrying..." WiFi message** — The scan failure screen
  said "Failed. Retrying..." but no retry logic existed; the device just waited
  for the user to press Back with no on-screen hint.  Changed to "Scan failed!"
  which accurately describes the state.

- **Sub-GHz Read never recognises signals** — The scene-based Read mode queued
  every individual radio edge as a separate FreeRTOS event, but the event loop
  processed only one pulse per iteration (with LCD redraws in between).  At
  433 MHz with AM650 modulation the noise floor alone generates thousands of
  edges/sec; the 256-deep queue overflowed and real signal pulses were silently
  dropped, so the protocol decoders never saw a complete packet.  Three changes
  fix this:
  1. **Batch-drain pulse events** — the event loop now processes all pending
     `Q_EVENT_SUBGHZ_RX` items in a tight inner loop before yielding, keeping
     the queue drained and feeding complete packets to decoders.
  2. **Use hardware-captured CCR1 instead of free-running CNT** — the TIM1
     input-capture ISR now reads the CCR1 register (latched at the exact edge)
     and computes the pulse duration via unsigned subtraction from the previous
     capture, eliminating ISR-latency jitter that corrupted timing.
  3. **Increase RX timer period to 65535 µs** — the old 20 ms period caused
     modular-arithmetic aliasing for inter-packet gaps near 20/40/60 ms; a
     full 16-bit period (65.5 ms) covers virtually all OOK protocol gaps.
  Additionally, the TIM1 Update interrupt is now disabled during RX to prevent
  the TX-specific UP handler from executing while no transmission is active.

- **Sub-GHz scene bottom-bar overlap cleanup** — Several Sub-GHz scene screens
  had menu content drawn too close to (or overlapping) the 12px bottom button bar
  at y=52.  Specific fixes:
  - **Config scene**: Reduced config item row height from 10px to 9px so the last
    item ("Sound") ends at y=48 with a 4px gap before the bar, and shortened the
    center bar label from "LR:Change" to "LR:Chng" to prevent horizontal text overlap
    with the right column.
  - **Read scene**: Reduced `HISTORY_VISIBLE` from 4 to 3 to prevent the 4th
    history row (y=47-54) from overlapping the bar (y=52-63).
  - **Playlist scene**: Moved the progress bar frame up 1px (y=49→48) so its
    bottom edge no longer collides with the bar's top edge.
  - **NeedSaving dialog**: Moved the Save/Discard choice buttons up 2px
    (y=42→40) to create a visible gap before the bar.
  - **Add Manually list** (legacy): Reduced `ADDMAN_VISIBLE_ITEMS` from 5 to 4
    so the 5th item (y=52-62) no longer draws directly on top of the bar.
  - **Radio Settings** (legacy): Shifted the ISM Region row up 1px (y=40→39)
    so its highlight box ends at y=50 with a 2px gap before the bar.

- **Sub-GHz DMA buffer 32-byte alignment** — The front and back sample buffers
  used by the Sub-GHz TX DMA (`subghz_front_buffer`, `subghz_back_buffer`) were
  allocated with plain `malloc()` which only guarantees 8-byte alignment.  On the
  STM32H573 (Cortex-M33 with D-Cache), DMA buffers must be aligned to the 32-byte
  cache line size to prevent data corruption from cache-line sharing.  Both buffers
  now over-allocate by 31 bytes and align the usable pointer to a 32-byte boundary,
  with the original base pointer stored separately for correct `free()`.

- **Main menu off-by-one: highlight and selection could diverge** — When
  returning from a scene-based module whose blocking delegates used legacy
  X_MENU operations (NFC Read, RFID Read, etc.), the `x_menu_update_init`
  flag was left set and `disp_window_active_row` / `disp_window_top_row`
  could be overwritten with stale values on the next `MENU_UPDATE_REFRESH`.
  The visual highlight then pointed to a different item than the one
  actually selected, making it appear that pressing OK opened the item
  below the highlighted one.  The fix clears stale X_MENU state without
  restoring from the backup, and verifies that the display position matches
  `sel_item` — recalculating if they diverge.

- **Stale button events after module exit** — `button_events_q_hdl` was
  never reset when a scene-based module exited, so a leftover button press
  from inside the module could be consumed by the main menu handler on the
  very next event loop iteration, causing a spurious menu movement.  All
  scene exit paths (generic `m1_scene_run`, Sub-GHz scene, and the main
  menu `subfunc_handler`) now drain the button queue before returning
  control to the main menu.

- **Sub-GHz: `uint8_t`/`bool` type mismatch compile error** — `manchester_advance()`
  writes to a `bool *` output, but the Oregon V1 and Oregon3 decoders declared the
  receiving variable as `uint8_t`, causing a pointer-type mismatch on strict compilers.
  Changed `bit_val` to `bool` in both decoders and updated `subghz_protocol_blocks_add_bit()`
  parameter from `uint8_t` to `bool` to match (the value is always 0 or 1).

- **Sub-GHz Read: frequency=0 saved for non-hopper signals** — When the frequency
  hopper was not active, decoded signals were stored with frequency 0 Hz.  This broke
  saved `.sub` files (wrote `Frequency: 0`) and caused TX replay to use the wrong
  radio band.  Now correctly stores the active preset frequency via a new
  `subghz_get_freq_hz_ext()` helper and the `current_freq_hz` app field.

- **Sub-GHz Read: history auto-select picked oldest signal** — After a new decode,
  the history list selected index `count-1` (oldest, due to reversed ring-buffer
  indexing) instead of index 0 (newest).  User had to manually scroll up after every
  decode.  Now auto-selects the most recent signal and scrolls to the top.

- **Sub-GHz Read: hopper never actually retuned the radio** — The hopper tick handler
  updated `app->hopper_freq` in software but never called any radio hardware function
  to change frequency.  The Si446x stayed on the initial frequency for the entire
  session.  Now calls `subghz_retune_freq_hz_ext()` which mirrors the legacy
  pause→set-freq→set-opmode→start sequence from `subghz_hopper_retune_next()`.

- **Sub-GHz Read: `current_freq_hz` never populated** — The `SubGhzApp.current_freq_hz`
  field (declared in header) was never assigned.  It is now set on RX start, frequency
  preset cycling (L/R), and hopper ticks, keeping it consistent with the actual radio
  frequency at all times.

### Removed

- **Sub-GHz legacy dead code (~2,070 lines)** — Surgically removed all public
  functions and their unique helpers that were fully superseded by the scene-based
  architecture: `sub_ghz_record()`, `sub_ghz_replay()`, `sub_ghz_read()`,
  `sub_ghz_saved()`, `sub_ghz_regional_information()`, `sub_ghz_radio_settings()`,
  `sub_ghz_config_screen()`, `sub_ghz_config_draw()`, `sub_ghz_saved_action_menu()`,
  `sub_ghz_saved_draw_actions()`, all Record GUI callbacks, all Replay Browse GUI
  callbacks, dead Replay Play GUI callbacks, legacy menu entries in `m1_menu.c`,
  and corresponding declarations in `m1_sub_ghz.h`. Retained
  `subghz_replay_play_gui_update()` (still called by live
  `sub_ghz_replay_flipper_file()`), all waveform helpers, static TX helpers, and
  all blocking delegate functions used by scene wrappers.

- **Bluetooth legacy dead code (~300 lines)** — Removed the dead simple-mode
  scene code in `m1_bt_scene.c` (entire `#ifndef M1_APP_BT_MANAGE_ENABLE` block),
  the legacy `bluetooth_scan()` original implementation in the `#else` fallback
  path of `m1_bt.c`, the empty `bluetooth_config()` function (only called from
  removed simple-mode scene), original `ble_scan_list_validation()` and
  `ble_scan_list_print()` helpers, and the `bluetooth_config()` declaration from
  `m1_bt.h`. All live blocking delegates (`bluetooth_scan()`, `bluetooth_saved_devices()`,
  `bluetooth_advertise()`, `bluetooth_info()`, `bluetooth_set_badbt_name()`) retained.

- **WiFi legacy dead code (~50 lines)** — Removed the dead `wifi_config()` stub in
  the `#else` fallback of `m1_wifi.c` (never compiled since `M1_APP_WIFI_CONNECT_ENABLE`
  is always defined), the `wifi_config()` redirect function (scene delegate now calls
  `wifi_saved_networks()` directly), and the unused `menu_wifi_exit()` empty function.
  Cleaned up corresponding declarations from `m1_wifi.h`.

- **NFC dead code (~28 lines)** — Removed `#if 0` empty view tables
  (`view_nfc_tools_table`, `view_nfc_saved_table`) and the never-compiled
  `SEE_DUMP_MEMORY` debug block (commented-out `#define` + `#ifdef` guard) from
  `m1_nfc.c`. RFID audited — no dead code found.

- **Infrared dead code (~10 lines)** — Removed the empty `menu_infrared_exit()`
  function and its declarations from `m1_infrared.c` and `m1_infrared.h`. The
  scene entry passes `NULL` as deinit; the function was never called.

## [0.9.0.28] - 2026-04-06

### Added

- **Generic scene framework** (`m1_scene.h/c`) — Shared, reusable scene manager with
  stack-based navigation, button event translation, generic event loop, and Flipper-style
  scrollable menu draw helper.  All modules except Sub-GHz (radio-specific) share this
  framework, eliminating 90% of boilerplate code.
- **Scene-based menus for all modules** — Migrated RFID, NFC, Infrared, GPIO, WiFi,
  Bluetooth, Games, and Settings from the legacy `S_M1_Menu_t` menu system to the
  scene-based architecture.  Each module now uses its own scene manager with blocking
  delegates wrapping the existing legacy functions (no rewrites needed).
  - **RFID**: 4–5 items (Read, Saved, Add, Import, Utilities)
  - **NFC**: 7 items (Read, Detect Reader, Saved, Extra Actions, Add Manually, Tools, Field Detect)
  - **Infrared**: 3 items (Universal Remotes, Learn, Replay)
  - **GPIO**: 5–6 items (GPIO Control, 3.3V, 5V, USB-UART, Signal Gen, CAN Bus sub-menu)
  - **WiFi**: 4–6 items (Scan+Connect, Zigbee, Thread, Saved, Status, Disconnect)
  - **Bluetooth**: 3–7 items (Scan, Saved, Advertise, Bad-BT, BT Name, BLE Spam, BT Info)
  - **Games**: 6 items (Snake, Tetris, T-Rex Runner, Pong, Dice Roll, Music Player)
  - **Settings**: 7 items + 4 nested sub-menus (Storage/5, Power/3, FW Update/4, ESP32/3)
- **Sub-GHz Spectrum Analyzer** — Bar-graph spectrum display with zoom, pan, peak detection,
  and 5 preset sweep bands.  Accessible from the scene-based Sub-GHz menu.
- **Sub-GHz RSSI Meter** — Continuous signal strength meter with bar graph, peak tracking,
  and band switching.
- **Sub-GHz Freq Scanner** — Frequency scanner that sweeps a range, captures signal hits
  above threshold, and displays a deduplicated hit list with RSSI and counts.
- **Sub-GHz Weather Station** — Weather protocol decoder for 433.92 MHz stations
  (Oregon V2, LaCrosse TX, Acurite, etc.) with temperature, humidity, and battery display.
- **Sub-GHz Brute Force** — Brute-force RF code transmitter supporting Princeton, CAME,
  Nice FLO, Linear, and Holtek protocols with progress display.

### Changed

- **m1_menu.c simplified** — Removed ~400 lines of individual submenu item definitions and
  conditional compilation variants.  Each module entry now uses a single-line scene entry
  point (e.g. `rfid_scene_entry`) with `num_submenu_items = 0`, matching the Sub-GHz pattern.
  BadUSB and Apps retain direct function calls (single-function modules, no submenus).
- **Scene-based architecture is now complete** — All 9 modules with submenus (Sub-GHz, RFID,
  NFC, Infrared, GPIO, WiFi, Bluetooth, Games, Settings) use the scene manager.  Only BadUSB
  and Apps (single-function leaf items) remain as direct function calls.
- **Sub-GHz menu expanded to 11 items** — Added Spectrum Analyzer, RSSI Meter, Freq Scanner,
  Weather Station, Brute Force, and wired up Add Manually (was a no-op).  Now matches C3
  feature parity.  Menu scrolls with 6 visible items and scrollbar position indicator.
- **CI: skip builds for non-compilation changes** — Added `paths-ignore` filters to
  `ci.yml` so that PRs and pushes touching only documentation, database files, IDE
  configs, or CI workflow files no longer trigger a firmware build.  Updated `CLAUDE.md`
  Workflow Rules and `.github/GUIDELINES.md` to instruct agents and contributors that
  builds are not required for such changes.
- **Sub-GHz menu: Flipper-style selection list** — Removed the full-width "OK" button bar
  from the Sub-GHz main menu scene and replaced it with a right-edge scrollbar position
  indicator.  All 6 menu items now display without scrolling, using the full vertical space.
- **Sub-GHz Saved action menu** — Removed the "OK" button bar from the Emulate / Rename /
  Delete action menu and redistributed item spacing to fill the available screen area evenly
  (17px per item instead of 13px).
- **Documentation**: Added scene-based application architecture guidance and updated button
  bar rules in `CLAUDE.md` to prohibit "OK"-only button bars on selection lists.
- **Documentation**: Scene-based architecture is now **mandatory** for all modules.
  `CLAUDE.md` includes a migration status table (Sub-GHz done, 9 modules pending),
  step-by-step migration instructions for agents, and the blocking-delegate pattern
  for wrapping legacy functions without rewriting them.

### Fixed

- **Firmware update menu overflow**: The 4th menu item ("Download") no longer
  overflows into the info box below. Switched from the tall info box (Y=32) to
  the short info box (Y=42) and condensed the version display from three rows
  to two (file name + compact "old > new" version line).

### Added

- **WiFi Firmware Download**: New "Download" option in the Firmware Update menu
  that allows browsing and downloading firmware images from Internet sources
  (GitHub Releases) directly to the M1's SD card for flashing. Requires active
  WiFi connection.
  - **Configurable sources**: User-editable `fw_sources.txt` on SD card defines
    download sources. Ships with three defaults: Monstatek Official, Hapax Fork,
    and C3 (bedge117).
  - **Source type architecture**: Pluggable source-type system (currently supports
    `github_release`, extensible to direct URLs and other forges).
  - **Asset filtering**: Each source specifies suffix filters (e.g., `_wCRC.bin` for
    Hapax, `.bin` for Monstatek) and exclusion patterns (`.elf`, `.hex`) to
    automatically select the correct firmware binary from release assets.
  - **Download progress UI**: Real-time progress bar with percentage, KB counter,
    and cancel support (BACK button during download).
  - **Reusable HTTP client** (`m1_http_client.c/h`): General-purpose module for
    both small API requests (`AT+HTTPCLIENT`) and large file downloads
    (`AT+CIPSTART` streaming to SD). Supports HTTPS, HTTP redirect following,
    and progress callbacks.
  - **Minimal JSON parser** (`m1_json_mini.c/h`): Lightweight parser for GitHub
    API responses — extracts strings, integers, booleans, and navigates arrays
    and nested objects without heap allocation.
  - **ESP32 AT command extensions**: Added TCP/SSL and HTTP AT command definitions
    (`AT+HTTPCLIENT`, `AT+CIPSTART`, `AT+CIPSEND`, `AT+CIPRECVDATA`, etc.) to
    the host command list. No ESP32 firmware rebuild needed — these commands were
    already compiled into the AT firmware.
  - **WiFi state accessor**: Added `wifi_is_connected()` and
    `wifi_get_connected_ssid()` public functions for external module use.

### Changed

- **Renamed release binary suffix `_SD.bin` → `_wCRC.bin`** — aligns Hapax release
  file naming with Monstatek Official (`MonstaTek_M1_v0800_wCRC.bin`) and
  C3/bedge117 (`M1_v0800_C3.12_wCRC.bin`). Affected: CMake output filename,
  CI workflow artifact paths, firmware download source defaults, and documentation.
- **Removed redundant "Back" labels from all app button bars and menus** — the
  hardware back button is self-explanatory. Button bars now only show hints for
  non-obvious directional actions (Config, Save, OK:Listen, etc.), freeing
  screen space on the 128px display. Affected apps: Sub-GHz (all scenes),
  Bluetooth, 802.15.4, BadUSB, BadBT, Infrared, IR Universal, CAN, Settings.
- **Removed "Back" from Sub-GHz saved file action menus** — the action menu
  (Emulate / Rename / Delete) no longer includes a redundant "Back" item.
- **Documentation**: Added UI / Button Bar Rules to CLAUDE.md prohibiting
  "Back" labels in button bars and action menus.
- **Redistributed Sub-GHz saved action menu layout** — after removing the
  "Back" item (3 items instead of 4), increased row height to fill the
  available vertical space evenly (13px rows in scene version, 16px in legacy).
- **Documentation**: Added layout redistribution rule to CLAUDE.md — when
  item count changes, recompute row height to fill the display zone evenly.

### Fixed

- **Sub-GHz signal detection not working** — fixed three issues preventing the
  Read scene from decoding captured signals:
  1. **ISR state machine not synchronized**: The pulse handler's return value
     (PULSE_DET_EOP / PULSE_DET_IDLE) was discarded by the scene manager event
     loop, leaving the ISR in NORMAL state permanently. This caused every RF
     noise edge to flood the 256-item FreeRTOS queue, drowning actual signal
     edges and preventing protocol decoders from accumulating enough pulses.
  2. **No pulse handler state reset**: The `subghz_pulse_handler` static
     `interpacket_gap` variable persisted across sessions. Starting a new RX
     session could begin with stale decoder state.
  3. **subghz_record_mode_flag not cleared**: Entering the Read scene after
     a Read Raw session could leave `subghz_record_mode_flag=1`, causing the
     event loop to skip the pulse handler path entirely.

### Changed

- **Sub-GHz Read scene — Flipper-consistent behavior**:
  - RX auto-starts on scene entry (no "Press OK to listen" idle screen)
  - L/R buttons retune frequency while actively receiving (live retune)
  - History list auto-opens when first signal is decoded
  - Newest signal auto-selected in history list
  - History preserved across child scene navigation (ReceiverInfo, Config)
  - Config accessible via DOWN during active RX (not just idle)
  - BACK exits scene directly instead of requiring two presses (stop → exit)
  - Bottom bar updated to show available actions contextually

### Fixed

- **Sub-GHz Read crash** — fixed three bugs that caused a consistent crash
  shortly after launching the Read operation in the new scene-based Sub-GHz UI:
  1. **Missing decoder init**: `subghz_decenc_init()` was never called, leaving
     all function pointers in `subghz_decenc_ctl` NULL. Calling
     `subghz_decenc_read()` dereferenced a NULL function pointer → HardFault.
  2. **Uninitialized ring buffer**: `subghz_record_mode_flag` was set to 1
     without calling `sub_ghz_ring_buffers_init()`, causing the ISR to write
     raw pulse data into an uninitialized ring buffer → memory corruption.
  3. **Pulse data never fed to decoder**: `Q_EVENT_SUBGHZ_RX` events were
     mapped to a generic scene event without extracting the pulse duration.
     `subghz_pulse_handler()` was never called, so protocol decoders never ran.
  - Fix adds `subghz_decenc_init()` at scene app startup, removes the
    incorrect `subghz_record_mode_flag` usage from the Read scene, and
    processes pulses directly in the event loop via the `subghz_pulse_handler`
    function pointer.
  - RSSI now updates via periodic 200ms timeout during active RX instead of
    on every pulse event, avoiding excessive display redraws.

### Added

- **Sub-GHz Playlist Player** — new scene that reads `.txt` playlist files from
  `/SUBGHZ/playlist/` and transmits each referenced `.sub` file sequentially.
  - Playlist format: one `sub: /SUBGHZ/path/to/file.sub` entry per line;
    comments (`#`) and blank lines are ignored
  - Flipper Zero path compatibility: `/ext/subghz/...` paths are automatically
    remapped to `/SUBGHZ/...` so UberGuidoZ playlists work without editing
  - Adjustable repeat count (1–9 or infinite) via L/R buttons
  - Progress display with file list, current-file highlight, and progress bar
  - Maximum 16 files per playlist (firmware memory constraint)
  - Curated playlist database included (`subghz_playlist/`) from
    UberGuidoZ/Flipper: Tesla charge port, doorbells, fans on/off
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

- **SiN360 fork sync** — merged sincere360/M1_SiN360 v0.9.0.4
  (`sincere360/main`, `997c9ce`) using `ours` strategy to record ancestry.
  SiN360 includes LCD settings with SD persistence, screen orientation,
  IR remote mode, expanded IR protocols, SPI handshake fix, NFC tools
  (Cyborg Detector, Fuzzer, Write URL, Utils), and IR file loader.  Many
  features already present in Hapax or planned for cherry-pick.  No files
  changed; merge recorded for branch comparison and future cherry-picks.

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

- **IR database expansion** — imported 1,296 curated IR remote files from
  [Flipper-IRDB](https://github.com/Lucaslhm/Flipper-IRDB) (CC0 license,
  commit `ed18b8dc`) into `ir_database/`.  Covers TVs (395), ACs (158),
  audio receivers (102), soundbars (105), speakers (69), fans (154),
  projectors (125), LED lighting (167), and streaming devices (25).
  Two new categories added: `LED/` and `Streaming/`.  Existing 116 M1
  original files preserved at top level; IRDB files live in brand
  subdirectories.  Total IR library now 1,412 files.

- **Sub-GHz signal database** — created `subghz_database/` with 313
  curated `.sub` files from
  [UberGuidoZ/Flipper](https://github.com/UberGuidoZ/Flipper) (GPLv3,
  commit `c074cebb`).  Categories: outlet switches (179), doorbells (81),
  weather stations (39), smart home remotes (10), fans (4).  Includes
  GPLv3 LICENSE and SOURCES.md attribution.

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

- **Documentation: Flipper-weighted pattern adoption policy** — added a Pattern
  Adoption Policy section to `documentation/flipper_import_agent.md` that gives
  Flipper and community forks (Momentum, Unleashed) higher weight than
  Monstatek/M1 patterns for protocol decoders, struct layouts, naming, and HAL
  decisions while the Monstatek stock firmware remains at version ≤ 1.0.0.0.
  Added a Monstatek Pattern Deviation Log table.

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
