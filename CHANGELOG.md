<!-- See COPYING.txt for license details. -->

# Changelog

All notable changes to the M1 project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.9.0.146] - 2026-04-22

### Fixed

- **Sub-GHz: M1 native .sgh NOISE files now replay directly without conversion** — C3.12
  and SiN360 `.sgh` recordings previously failed with "Memory error" (error 5) when
  Emulate was chosen in Saved, because they were routed through the Flipper `.sub`
  converter which wrote a temp file and parsed it via `sub_ghz_raw_samples_init()`.
  M1 native NOISE files are now detected via `is_m1_native` (set during file load)
  and replayed directly with `sub_ghz_replay_datafile()`, bypassing the conversion
  entirely. The same direct-replay dispatch is applied in the Playlist scene via the
  new `flipper_subghz_probe()` header probe. The `sub_ghz_raw_samples_init()` CRLF
  assumption (`strlen(token) + 2`) is also fixed to scan past actual line terminators,
  so LF-only files from Linux-origin firmware builds stream correctly.
- **Sub-GHz: guard `sub_ghz_ring_buffers_init()` against double-allocation** — calling
  the function while buffers are already live no longer leaks the prior allocation;
  a preceding `sub_ghz_ring_buffers_deinit()` call ensures a clean slate and prevents
  heap fragmentation from triggering spurious "Memory error" (error 4) failures.
## [0.9.0.145] - 2026-04-22

### Fixed

- **Sub-GHz: fixed reboot when saving a captured signal as .sgh or .sub** — `flipper_subghz_signal_t`
  contains a 16 KB `raw_data[8192]` array that was being allocated on the FreeRTOS task stack in
  four save-path functions (`subghz_save_history_entry`, `scene_on_enter` in the Save Name scene,
  `sub_ghz_add_manually_transmit`, and the Bind Wizard save helper).  The 16 KB stack frame
  overflowed the task stack and caused an immediate reboot before any file could be written.
  Fixed by adding lightweight `flipper_subghz_save_key()` and `flipper_subghz_save_m1native_key()`
  helpers that write a parsed/key signal directly from its decoded fields (frequency, protocol,
  bit_count, key, te) without requiring a `flipper_subghz_signal_t` at the call site at all.
  This approach (writing directly from decoded values rather than through an intermediate struct)
  matches how Flipper/Momentum firmware serializes decoded signals, and costs zero extra BSS —
  the large `raw_data[]` buffer is only needed when loading RAW files in the Saved scene, where
  it is already correctly managed as a file-scope static (`saved_signal`).
## [0.9.0.144] - 2026-04-21

### Fixed

- **Sub-GHz Frequency Analyzer: two-stage Momentum-style scan** — Adopted the same
  two-stage algorithm used by Momentum/Flipper firmware. Stage 1 (coarse) scans the
  preset frequency table (~25 entries per band, ~50 ms) instead of a linear MHz sweep,
  guaranteeing exact known frequencies like 433.920 MHz and 330.000 MHz are always
  checked. Stage 2 (fine) sweeps ±300 kHz at 20 kHz step around the coarse peak
  (~30 steps, ~60 ms) when a signal exceeds the threshold, providing sub-20-kHz
  accuracy for any signal — including non-preset frequencies. Total scan cycle
  ≈ 110 ms vs the previous 3.9 s linear sweep. Fixes: 330 MHz not detected in the
  300 MHz band, and 433.92 MHz incorrectly shown as 434.00 MHz.
## [0.9.0.143] - 2026-04-21

### Fixed

- **Sub-GHz RSSI Meter: anchored "dBm" labels** — The "dBm" unit label now stays at a
  fixed pixel position for both the current and peak readings. Previously the label
  shifted horizontally as the numeric value changed digit count (e.g. -9 → -100).
  All five display elements (current value, "dBm", "Pk:", peak value, "dBm") are now
  drawn at independent, fixed X coordinates.
## [0.9.0.142] - 2026-04-21

### Changed

- **Sub-GHz Read: frequency display cycles unconditionally when hopping** — The hopper now advances to the next frequency on every 200 ms tick regardless of RSSI level, so the status bar always visually cycles through the hopper frequencies. Previously the hop was RSSI-gated and the display could get stuck on one frequency if the ambient noise floor was above the threshold.
- **Sub-GHz Read Raw: Hopping option hidden from Config** — The Hopping setting is no longer shown in the Config screen when accessed from Read Raw, since frequency hopping is not applicable during raw capture. Hopping continues to work normally in the Read (protocol decode) scene.

### Fixed

- **IR and Sub-GHz databases now included in GitHub releases** — added `SD_Assets.zip`
  to every release artifact.  Extracting this archive to the root of the SD card
  places `IR/` (1,412 IR remote files) and `SubGHz/` (313 signal files + playlists)
  in the correct locations so **Infrared → Saved**, **Infrared → Universal Remote**,
  **Sub-GHz → Saved**, and **Sub-GHz → Playlist** work immediately after flashing
  without any manual file copying.
## [0.9.0.141] - 2026-04-21

### Added

- **Sub-GHz: add 319.50 MHz frequency preset** — Magellan/GE/Interlogix security sensors (common in North American installations) operate at 319.5 MHz. The preset was missing from the frequency list, making it impossible to receive these signals or verify Flipper-emulated Magellan .sub files. 319.50 MHz is now selectable in the Sub-GHz Read/Read Raw/Config scenes. The Magellan protocol flags have also been corrected from 433 MHz-only to include the 315 MHz band, so the protocol appears in Add Manually at both 315 and 433 MHz.
- **Sub-GHz: host-side unit tests for frequency preset table** — extracted the Sub-GHz frequency preset table from `m1_sub_ghz.c` into a hardware-independent module (`m1_csrc/subghz_freq_presets.c/.h`) and added `tests/test_subghz_freq_presets.c` with 13 tests enforcing: array length matches `SUBGHZ_FREQ_PRESET_COUNT`, custom sentinel equals count, default index points to 433.92 MHz, all presets within SI4463 operating range, sorted ascending, no duplicates, non-null labels, and regression guards for 319.5 MHz / 315.0 MHz / 433.92 MHz / 868.35 MHz. Also added a protocol-band-vs-preset coverage test to `test_subghz_registry.c` that verifies known protocol operating frequencies are present in the preset table — future protocol ports that omit a required frequency will fail CI.

### Changed

- **Sub-GHz Read: frequency hopper is now region-aware** — The hopper used by the
  Read scene now selects its 6 dwell frequencies based on the user's ISM Region
  setting (Sub-GHz → Config → Region) rather than using a single global list.
  North America hops 315/345/390/433.92/434.42/915 MHz; Europe hops the SRD 433
  cluster and 868 MHz; Asia/APAC adds 330/345 MHz gate-remote frequencies; Region
  Off uses a wide cross-region fallback covering every major global band.
  No settings UI change is needed — the existing Region selector already controls
  the behaviour.
- **Documentation: Sub-GHz New Protocol Checklist** — added mandatory agent checklist to CLAUDE.md that prevents "missing frequency preset" bugs when porting new protocols from Flipper/Momentum. Covers frequency preset audit, band flag audit, Flipper vs M1 clock/frequency distinction, and required test commands. Motivated by the Magellan/GE 319.5 MHz bug.
## [0.9.0.140] - 2026-04-21

### Fixed

- **Sub-GHz Read Raw: replace Start-state sine wave with static empty waveform frame** — Start state now shows a completely static waveform frame (matching Flipper/Momentum Read Raw). No animation of any kind is shown until REC is pressed; the RSSI spectrogram and cursor only appear once recording has begun and actual RF edges are captured.
## [0.9.0.139] - 2026-04-21

## [0.9.0.138] - 2026-04-21

### Changed

- **Documentation: README and ARCHITECTURE cleanup** — Clarified that qMonstatek is a
  community-maintained companion app (not a Hapax project); removed redundant standalone
  flashing method in favour of the Web Updater and DFU sections; removed stale bedge117/M1
  cross-reference from the highlights table; synced Sub-GHz protocol count in ARCHITECTURE.md.

### Removed

- **Sub-GHz: removed non-functional 150/200/250 MHz bands** — The Si4463 radio
  config and antenna matching network on the M1 are designed for 300–928 MHz.
  The 150/200/250 MHz band options were initialising the radio with the 300 MHz
  config and retuning the VCO, which does not produce correct output at those
  frequencies. The bands have been removed from the firmware UI, the spectrum
  analyser sweep presets, and the README.
## [0.9.0.137] - 2026-04-20

### Added

- **Sub-GHz: Bind New Remote wizard** — new guided wizard for creating and binding
  brand-new rolling-code remotes to receivers directly from the M1.  Accessible via
  Sub-GHz → Bind Remote (13th menu item).  Supported protocols (all have
  `SubGhzProtocolFlag_PwmKeyReplay` — bind once, replay forever from Saved):
  CAME Atomo 433, Nice FloR-S 433, Alutech AT-4N 433, DITEC GOL4 433,
  KingGates Stylo4k 433.  The wizard generates an entropy-seeded random
  key, saves the `.sub` file to `0:/SUBGHZ/`, then walks the user
  through the receiver-side binding steps with optional countdown timers and inline
  one-press TX at each transmit step.  The generated file is immediately replayable
  from Sub-GHz → Saved with full counter-increment logic.
## [0.9.0.136] - 2026-04-20

### Added

- **Sub-GHz: Acurite 5n1 weather station decoder** — Added decoder for Acurite 5-in-1 weather transmitters (64-bit OOK PWM, 433.92 MHz). Flipper `.sub` files with `Protocol: Acurite_5n1` are now recognised and displayed. Validates the 7-bit checksum; decodes both message types (0x31: wind/rain, 0x38: wind/temperature/humidity).
- **Sub-GHz Config: configurable hopper RSSI threshold** — A new "RSSI Thresh" row in the Sub-GHz Config screen lets the user choose the hopping threshold between −50 and −100 dBm (5 dBm steps; default −70 dBm, matching Flipper). The value is persisted in `settings.cfg` as `subghz_rssi_threshold=`. Previously the threshold was hardcoded at −70 dBm.
- **Sub-GHz Config: custom frequency entry** — A "Custom" option (index 62) has been added after the 62 standard frequency presets. Selecting it and pressing OK opens the virtual keyboard to type a frequency in MHz (e.g. `433.92`). The value is validated to the SI4463 operating range (300–930 MHz) and persisted in `settings.cfg` as `subghz_custom_freq=`. This closes the last meaningful UX gap vs Flipper's frequency config screen.
- **Sub-GHz Playlist: inter-signal delay support** — Playlist `.txt` files now support `# delay: <ms>` comment directives (e.g. `# delay: 500`). When present, the player waits the specified number of milliseconds between signals. Compatible with UberGuidoZ and RocketGod playlist collections that rely on inter-signal gaps to avoid double-triggering devices. Delays are per-entry and are applied after the preceding signal transmits. Valid range: 0–60000 ms.
- Documentation: **Sub-GHz Emulation Status reference** — new
  `documentation/subghz_emulation_status.md` tracking every Dynamic protocol's
  emulation capability, blocking reason, and research pointers for future work.
- **Sub-GHz Remote** — New "Remote" scene in the Sub-GHz menu. Load a `.rem` manifest file that maps the five hardware buttons (UP / DOWN / LEFT / RIGHT / OK) to individual `.sub` signal files. Pressing a mapped button fires that signal immediately, making the M1 act as a multi-button RF remote control. Manifest format is plain text with `up:`, `down:`, `left:`, `right:`, and `ok:` directives pointing to `.sub` paths; Flipper-style `/ext/subghz/` paths are remapped automatically. This is a Momentum-parity feature not present in upstream Monstatek firmware.

### Changed

- **Sub-GHz: larger RAW offline-decode buffer** — `FLIPPER_SUBGHZ_RAW_MAX_SAMPLES` increased from 2048 to 8192 samples. The offline decode path (Saved scene) can now handle real-world Flipper captures of garage doors, TPMS bursts, and security sensors without truncation.

### Fixed

- **Battery: voltage-based SoC correction for miscalibrated fuel gauges** — When
  the BQ27421 fuel gauge reports 0% (or ≤5%) state of charge but the measured
  battery voltage indicates meaningful charge remains (above the configured
  terminate threshold), a piecewise-linear voltage-to-SoC estimate is applied
  as a floor.  This prevents the "Asian M1" hardware variant — where the device
  stays powered and fully functional at a reported 0% — from incorrectly blocking
  firmware updates that require ≥25% battery.
- **Tests: add missing CI path filters for NFC and Esp_spi_at** — `test_nfc_poller`
  (sources in `NFC/`) and `test_esp_queue` (sources in `Esp_spi_at/`) were not
  triggering on CI when their source directories changed. Added `NFC/**` and
  `Esp_spi_at/**` to the `tests.yml` path filter. Also documented the CI path
  filter maintenance rule in `DEVELOPMENT.md` and `CLAUDE.md` so future agents
  keep the filter in sync when adding new tests.
- Documentation: Fixed `python3` snippet in `CLAUDE.md` Quick audit section — switched from `python3 -c "..."` (indented code caused `IndentationError`) to `python3 - <<'PY' ... PY` heredoc style and corrected the regex backslash escaping accordingly.
- **Tests: fix lfrfid_manchester test infrastructure** — correct `lfrfid_evt_t` stub
  to use `uint16_t edge` matching the production struct; add `lfrfid/**` to the
  `tests.yml` CI path filter so changes to LFRFID source files automatically
  trigger the unit test suite.
## [0.9.0.134] - 2026-04-20

### Changed

- **Sub-GHz: Frequency Analyzer overhaul** — Rewrote `sub_ghz_frequency_reader()` to match
  Momentum's frequency-analyzer UX. Fixes the incorrect "Frequency Reader" title. Now
  continuously sweeps a user-selected ISM band (300-316 / 387-464 / 779-928 MHz) each pass
  and reports the peak-signal frequency with a live RSSI bar graph. New controls: L/R cycles
  the scan band, UP/DOWN adjusts the detection threshold (−50…−100 dBm in 5 dBm steps), OK
  toggles hold/resume (freezes the displayed frequency), and BACK exits. Threshold marker is
  drawn over the RSSI bar so the user can see at a glance whether the current signal crosses
  it. A brief buzzer pulse fires when a signal above threshold is detected. Cleanup now uses
  `radio_set_antenna_mode(ISOLATED)` + SI4463 sleep + `menu_sub_ghz_exit()`, matching the
  patterns used by the Spectrum Analyzer and Freq Scanner.
## [0.9.0.133] - 2026-04-20

### Added

- **Sub-GHz: RocketGod SubGHz Toolkit integration** — The `keeloq_mfcodes` manufacturer
  key store parser now accepts both the compact Flipper-compatible format
  (`HEX:TYPE:Name`) and the multi-line format exported by
  [RocketGod's SubGHz Toolkit](https://github.com/RocketGod-git/RocketGods-SubGHz-Toolkit)
  for Flipper Zero.  Users can copy the toolkit's `keeloq_keys.txt` directly to
  `SUBGHZ/keeloq_mfcodes` on the M1's SD card without a conversion step.  A new
  `scripts/convert_keeloq_keys.py` utility is also included for users who prefer the
  compact format.  The new `keeloq_mfkeys_load_text()` API enables host-side unit
  testing of the parser; 22 new C tests and 28 Python tests cover both format variants.
## [0.9.0.132] - 2026-04-20

### Added

- **Games/Splash: main splash screen shows current local time** — the back-button idle
  screen now displays the current time ("HH:MM") centered horizontally at the top of
  the screen.  When the user's UTC offset is non-zero (configured in Settings → LCD &
  Notifications → Local TZ), the offset label (e.g. "UTC+5") is shown underneath the
  time in a smaller font.  The M1 logo and version text block have been shifted down
  and centered horizontally to accommodate the time display without obscuring any
  existing content.

### Fixed

- **Games: World Clock — local timezone setting** — the world clock now uses the
  user's configured UTC offset (Settings → LCD & Notifications → Local TZ) to
  correctly derive world-zone times from local RTC time.  The Local page now
  displays the configured timezone label (e.g. "Local UTC+5").  The offset is
  persisted in `settings.cfg` as `clock_tz_offset` and survives reboots.
## [0.9.0.131] - 2026-04-20

### Fixed

- **Infrared: fix "Learned" and Universal Remote showing "file not found" with no browse option**
  — `Infrared > Replay` now goes directly to the Learned files browser instead of the
  full Universal Remote dashboard, so saved .ir files are immediately accessible.
  — Quick-remote categories (TV, AC, Audio, etc.) now fall back to browsing the full
  `0:/IR/` tree when the category subdirectory is empty, instead of showing a dead-end
  "No files found" message with no way to navigate.
  — Empty-directory messages are now context-sensitive: the Learned folder tells the user
  to use the Learn screen; other empty folders tell the user where to copy .ir files.
## [0.9.0.130] - 2026-04-20

### Added

- **Sub-GHz: KeeLoq counter-mode rolling-code replay** — KeeLoq, Jarolift (Flipper
  format), and Star Line signals can now be replayed from Flipper `.sub` files when
  the manufacturer master key is present in `0:/SUBGHZ/keeloq_mfcodes` on the SD
  card.  The M1 derives the per-device key via Normal or Simple Learning, increments
  the 16-bit counter in the encrypted hop word using the full 528-round KeeLoq NLFSR,
  re-encrypts, and retransmits.  Export the keystore from a Flipper Zero using
  RocketGod's SubGHz Toolkit app.  The `Manufacture:` field in `.sub` files is now
  parsed so the correct master key is selected automatically.
## [0.9.0.129] - 2026-04-20

### Changed

- **Display: legacy submenu menus now honour user text-size preference** — `m1_gui_submenu_update()` (used by NFC and RFID action menus) now uses the font-aware helpers `m1_menu_max_visible()`, `m1_menu_item_h()`, and `m1_menu_font()` instead of a hardcoded 4-row / 16 px layout, so all menus in the system respond correctly to Settings → LCD & Notifications → Text Size.
## [0.9.0.128] - 2026-04-20

### Added

- **Sub-GHz: persist radio config across sessions** — frequency, modulation, hopping, sound, and TX power settings are now saved to `settings.cfg` when the Config screen is exited. On next launch the Sub-GHz app restores the last-used values instead of resetting to defaults.
## [0.9.0.127] - 2026-04-20

### Fixed

- **Sub-GHz RSSI Meter: fixed dBm/Pk label shift** — "Pk:" label is now drawn at a
  fixed pixel position so it no longer shifts left/right as the current RSSI value
  changes width. Refresh rate reduced from 20 Hz to ~6 Hz so the values are stable
  and easy to read.
## [0.9.0.126] - 2026-04-20

### Changed

- **Sub-GHz: ⚠ Migration note — Sub-GHz files saved before v0.9.0.124 must be recaptured** —
  Any `.sub` or `.sgh` signal file that was **saved by the Hapax firmware itself** in a
  build earlier than v0.9.0.124 contains a zeroed key value (`Key: 00 00 00 00 00 00 00 00`)
  and a missing frequency field. These files will load and display without error, but
  **emulation will fail silently** because the key that drives the OOK PWM waveform is zero.
-   **Root causes (both fixed in v0.9.0.124):**
-   1. The legacy save path (`subghz_save_history_entry`) did not copy the decoded key into
     the signal struct before writing it to disk — the key was always written as 0x0.
  2. The frequency field was empty because `snprintf("%.2f MHz", ...)` is a no-op under
     `--specs=nano.specs` (newlib-nano) without the `-u _printf_float` linker flag.
-   **Who is affected:**
  - Files captured and saved using **any Hapax build prior to v0.9.0.124**.
-   **Who is NOT affected:**
  - `.sub` / `.sgh` files saved by **C3.12, SiN360, or stock Monstatek v0.8.0.x** firmware —
    those files carry the correct key value and load/emulate correctly on Hapax v0.9.0.124+.
  - Files from the bundled **`subghz_database/`** signal library — these are pre-validated
    Flipper `.sub` format files and are unaffected.
-   **Action required:** Delete all `.sub`/`.sgh` files saved by Hapax before v0.9.0.124
  and recapture the signals using v0.9.0.124 or later. The new save path writes the correct
  key, bit count, TE, and frequency every time.

### Fixed

- **Sub-GHz: frequency display now shows correctly on device** — The frequency value (e.g.
  "433.92 MHz") was blank in the Signal Info screen, Decode Results detail, hopper status
  bar, and Receiver Info screen because `%.2f` format is not supported by newlib-nano
  (`--specs=nano.specs`) without `-u _printf_float`. All four sites now use integer
  arithmetic formatting (`%lu.%02lu MHz`) so frequency always renders correctly.
## [0.9.0.125] - 2026-04-20

### Fixed

- **ESP32 OTA / M1 FW Download: auto-upgrade stale `fw_sources.txt`** — Users who upgraded from an older Hapax build that predates the ESP32 OTA download feature (or the category system) would see "No ESP32 sources / Check fw_sources.txt" immediately. The downloader now detects this by regenerating `fw_sources.txt` with current defaults on the first "no sources" result, then retrying — making the ESP32 and M1 firmware download screens work out-of-the-box after an upgrade without any manual SD card edits.
- **Web Updater: fix CORS policy block on firmware download** — The web updater was blocked by the browser's CORS policy when trying to download firmware binaries directly from GitHub's CDN (`objects.githubusercontent.com`), which does not send `Access-Control-Allow-Origin` headers. Release asset downloads are now routed through the corsproxy.io CORS proxy, resolving the `ERR_FAILED` / "CORS policy" error in the browser console. The GitHub API calls (release listing) are unaffected — they already respond with correct CORS headers and are not proxied. Closes #251.
- **Web Updater: SHA-256 verification + proxy notice** — After every firmware download (from GitHub releases or a local file), the web updater now computes and displays the SHA-256 hash of the binary in the log so users can optionally verify it against the GitHub release page. A calm informational note is also shown in the "Select Firmware" panel explaining that downloads are routed through the corsproxy.io CORS proxy (a widely-used open-source proxy) and linking to the release page for verification.
## [0.9.0.124] - 2026-04-20

### Fixed

- **Sub-GHz Read Raw: instant recording start and Flipper-compatible output** — Radio now
  starts in passive listen mode when entering the Read Raw scene, so pressing REC gives
  immediate response with no ~100ms delay (Flipper/Momentum pattern). Recorded files are
  saved as Flipper-compatible `.sub` files (`RAW_Data:` keyword with signed timing values,
  `Preset: FuriHalSubGhzPresetOok650Async` header) and can be opened directly in Magellan
  and other Flipper-ecosystem tools. The animated sine wave in Start state now runs
  continuously at ~5 fps to clearly signal the scene is ready to record.
## [0.9.0.123] - 2026-04-17

### Fixed

- **Sub-GHz: Save from read-history now respects save format setting** — `subghz_save_history_entry()` previously hardcoded `.sub` regardless of the user's configured save format. It now honours the Sub-GHz config setting and writes `.sgh` (M1 native) or `.sub` (Flipper) accordingly.
## [0.9.0.122] - 2026-04-17

### Fixed

- **Sub-GHz: 330 MHz emulation fixed for Asia region** — Added 322–348 MHz to the
  Asia ISM allowed band, enabling emulation of 330 MHz and 345 MHz gate/garage
  remotes common in APAC markets. Also fixed a display bug where the Sub-GHz
  saved-file replay screen incorrectly showed "emulating" when the ISM region
  check blocked the transmission; the function now exits cleanly after showing
  the "TX Blocked" message instead of entering the TX event loop.
## [0.9.0.121] - 2026-04-17

### Added

- **Sub-GHz Brute Force: Security+ 1.0 (Chamberlain) rolling-code mode** — Brute Force
  now supports Security+ v1 / Chamberlain as a counter mode: the ternary OOK two-sub-packet
  encoding is computed on-device from the counter value and transmitted via the existing raw
  TX path.  A new pure-C encoder module (`subghz_secplus_v1_encoder`) implements the
  argilo/secplus algorithm (MIT) adapted from Flipper Zero firmware (GPLv3) with 11 host-side
  unit tests.

### Changed

- **Sub-GHz: Replace string-based rolling-code replay exclusion with `SubGhzProtocolFlag_PwmKeyReplay`** — Dynamic protocols that use plain OOK PWM encoding (CAME Atomo, CAME TWEE, Nice FloR-S, Alutech AT-4N, KingGates Stylo4k, Scher-Khan Magicar/Logicar, Toyota, DITEC_GOL4) are now explicitly marked replayable via a registry flag instead of fragile name-string comparisons. Manchester-encoded protocols (FAAC SLH, Somfy Telis, Somfy Keytis, Revers_RB2, Star Line) that were previously silently accepted by the key encoder and would have produced invalid OOK output are now correctly rejected.

### Fixed

- **Frequency Analyzer: fix missing 330 MHz coverage** — The 315 MHz band was
  disabled (0 channel steps), leaving a gap between 320 MHz and 345 MHz. Enabled
  the band with 80 channel steps (250 kHz each), extending coverage from 315 MHz
  to 335 MHz and restoring detection of 330 MHz remote control signals.
## [0.9.0.119] - 2026-04-17

### Added

- **Sub-GHz: CC1101 FEC encode/decode utility** — imported and adapted the CC1101
  Forward Error Correction (FEC) algorithm from
  [SpaceTeddy/urh-cc1101_FEC_encode_decode_plugin](https://github.com/SpaceTeddy/urh-cc1101_FEC_encode_decode_plugin).
  Adds `Sub_Ghz/cc1101_fec.c` / `cc1101_fec.h`: a hardware-independent C module
  implementing the rate-1/2 Viterbi convolutional codec, 4-byte block interleaver,
  and CRC-16/IBM used by TI CC1101-based IoT sensors when their FEC feature is enabled.
  Enables the M1 to decode raw GFSK packets from CC1101-FEC devices in software.
  Covered by 26 host-side unit tests (encode/decode roundtrip, known vector, edge cases).
## [0.9.0.118] - 2026-04-17

### Added

- **Sub-GHz: FireCracker (CM17A) home-automation RF decoder** — decodes 40-bit
  CM17A packets (0xD5AA header + 16-bit data + 0xAD footer) on 310/433 MHz.
  Displays house code (A-P), unit number (1-16), and command (ON/OFF/DIM/BRIGHT).
  Ref: https://github.com/evilpete/flipper_toolbox/raw/refs/heads/main/subghz/firecracker_spec.txt
## [0.9.0.117] - 2026-04-17

### Added

- **Sub-GHz: Configurable save format** — Config screen now includes a "Save Format" option (Flipper `.sub` / M1 native `.sgh`). Decoded signals are saved in the chosen format; M1 native `.sgh` PACKET files are also fully supported for emulation via the Saved file browser.
## [0.9.0.116] - 2026-04-17

### Added

- **Infrared: ESL Tag Tinker** — IR-based tool for Pricer Electronic Shelf Label tags,
  inspired by github.com/i12bp8/TagTinker. Accessible via Infrared → ESL Tags.
  Features: broadcast page flip (pages 0–7), broadcast diagnostic screen, and
  targeted ping of a specific tag by 17-digit barcode. Uses the tag's native
  ~1.25 MHz PP4 carrier (TIM1_CH4N, PC5) with DWT cycle-accurate symbol timing
  scaled for the STM32H573's 250 MHz core clock.
## [0.9.0.115] - 2026-04-17

### Added

- **Sub-GHz: expanded protocol compatibility tests** — Added 13 new decoder
  roundtrip tests covering Princeton (24-bit, auto te-detect from pulse[2]/[3]),
  Holtek_HT12X (12-bit, 1:3 ratio), and Ansonic (12-bit, 1:2 ratio).  Added 18
  new registry validation tests verifying that all static AM protocols carry the
  Send and Save flags, weather/TPMS protocols carry no Send flag, dynamic
  (rolling-code) protocols carry no Send flag, all decodable protocols have
  min_count_bit_for_found > 0, and all AM protocols declare at least one
  frequency band.  Added presence tests for 12 Momentum-ported protocols
  (Magellan, Marantec24, Clemsa, Centurion, BETT, Legrand, LinearDelta3,
  CAME TWEE, Nice FloR-S, Elplast, KeyFinder, Acurite_606TX).
## [0.9.0.114] - 2026-04-17

### Fixed

- **Sub-GHz: fix three 300 MHz implementation bugs** — (1) `SUBGHZ_FCC_ISM_BAND_310_000`
  was incorrectly defined as `300.00001` (copy-paste error); corrected to `310.00001` and
  updated all three ISM-band tables (NA/EU/Asia) to use `SUBGHZ_FCC_ISM_BAND_300_000` as
  the lower bound so 300 MHz frequencies remain within the allowed TX range. (2) Brute
  Force always initialised the radio at 433.92 MHz regardless of protocol; the "Linear"
  entry now correctly uses `SUB_GHZ_BAND_300` so 300 MHz is actually transmitted. (3)
  `SubGhzProtocolFlag_300` was missing from the protocol-flag enum; added it and tagged
  `Linear` and `LinearDelta3` with `F_STATIC_300`, and `Princeton` / `KeeLoq` with the
  new `F_STATIC_300_MULTI` / `F_ROLLING_300_MULTI` shorthands for RogueMaster feature
  parity.
## [0.9.0.113] - 2026-04-17

### Fixed

- **Sub-GHz Saved: M1 native .sgh files now show the action menu** — Files recorded with C3.12 (fw 0.8.x) or SiN360 (fw 0.9.x) and saved in M1 native `.sgh` format could not be opened from the Saved scene: selecting a file went straight back to the Sub-GHz menu instead of showing the Emulate/Info/Rename/Delete action menu. Root cause: `flipper_subghz_load()` only accepted Flipper `.sub` format versions "1" and "2"; M1 native files use the firmware version string (e.g. "0.8", "0.9") which the version check rejected. Fixed by saving the `Filetype:` value before reading the `Version:` line, then routing M1 native files (identified by "M1 SubGHz" in the filetype) through dedicated `m1sgh_parse_raw()` / `m1sgh_parse_packet()` body parsers. RAW `.sgh` pulse data is now also parsed for offline protocol decode (unsigned M1 durations are converted to alternating-signed Flipper-style samples).
## [0.9.0.112] - 2026-04-17

### Fixed

- **Sub-GHz Magellan decoder: fix inverted bit polarity and header skip** — The Magellan decoder
  incorrectly delegated to the generic OOK PWM decoder which reads standard polarity
  (bit 1 = LONG HIGH + SHORT LOW). Magellan uses inverted polarity (bit 1 = SHORT HIGH + LONG LOW),
  so every decoded bit was wrong. The decoder also failed to skip Magellan's unique frame header
  (800µs burst + 12 toggles + 1200µs start bit) before reading data bits. Replaced the
  broken generic-PWM delegation with a dedicated Magellan decoder that correctly identifies the
  start bit and decodes bits with the proper inverted polarity. Verified with 16 roundtrip unit
  tests covering bare-data buffers, full-frame buffers, and representative real-world keys.
  Protocol analysis also confirmed that CAME 12-bit, GateTX 24-bit, and Nice FLO 12-bit
  encoder/decoder pairs use consistent standard OOK PWM polarity (no bugs found in those).
## [0.9.0.111] - 2026-04-16

## [0.9.0.110] - 2026-04-16

### Added

- **Settings: System Dashboard** — 3-page at-a-glance status display ported from
  dagnazty/M1_T-1000, accessible from Settings menu. Shows overview (date/time,
  battery, voltage, uptime), I/O (SD card, USB, ESP32 status), and system info
  (firmware version, active bank, orientation, buzzer/LED state).
## [0.9.0.109] - 2026-04-16

### Fixed

- **Sub-GHz: Fix Magellan protocol emulation and improve Flipper .sub compatibility** — Magellan KEY files (e.g. Gulf Star Marina alarm signals from issue #188) now transmit the correct waveform with proper preamble, start/stop bits, and inverted bit polarity matching Flipper Zero's encoder. Protocol name lookup is now case-insensitive for better .sub file compatibility. Unknown protocols with a TE field in the .sub file now use a generic 1:3 OOK PWM fallback instead of failing.
## [0.9.0.108] - 2026-04-16

### Added

- **Games: Hex Viewer** — SD card hex/ASCII file viewer ported from
  dagnazty/M1_T-1000. Browse any file and view hex dump + ASCII preview
  with scroll navigation (UP/DOWN by row, LEFT/RIGHT by page, OK to
  browse another file). Accessible from Games menu.
## [0.9.0.107] - 2026-04-16

### Added

- **NFC: Poll profile support** — added `nfc_poll_profile_t` enum with
  `NFC_POLL_PROFILE_NORMAL` (all technologies) and `NFC_POLL_PROFILE_FAST_A`
  (NFC-A only with shorter 220ms discovery window) modes, plus
  `nfc_poller_set_profile()` / `nfc_poller_get_profile()` accessors.
  Ported from dagnazty/M1_T-1000.
## [0.9.0.106] - 2026-04-16

### Added

- **Games: World Clock utility** — standalone clock display with local time
  + 4 world time zones (UTC, UTC+1, UTC+5, UTC+9). Large digit display,
  leap year handling, weekday display. Pages through zones with LEFT/RIGHT.
  Ported from dagnazty/M1_T-1000 with pure logic extracted to
  m1_clock_util.c/h and host-side unit tests.
## [0.9.0.105] - 2026-04-16

### Added

- **CI: Auto-merge main into agent branches** — New `update-branches.yml`
  GitHub Actions workflow that triggers on every push to `main` and
  automatically merges main into all active `copilot/*` branches, keeping
  long-running agent branches up-to-date and reducing merge conflicts.

### Changed

- **Changelog fragment system** — Replaced direct CHANGELOG.md editing with
  fragment files in `.changelog/` to eliminate merge conflicts when multiple
  branches are in flight.  CI assembles fragments at release time.  Documented
  the workflow in DEVELOPMENT.md and CONTRIBUTING.md for human contributors.
## [0.9.0.104] - 2026-04-16

### Added

- **GPIO: USB-UART Bridge** — Momentum-style USB-to-serial adapter using
  USART1 (PA9 TX / PA10 RX) on J7 header and USB CDC virtual COM port.
  Features configurable baud rate (9600–921600, LEFT/RIGHT to cycle),
  on-screen scrolling terminal with ASCII and HEX display modes (OK to
  toggle), and bidirectional USB↔UART data forwarding via the existing VCP
  task infrastructure.  Debug logging is automatically suppressed during
  bridge operation to prevent UART data corruption.

## [0.9.0.103] - 2026-04-16

### Added

- **IR Universal Remote: Custom Remote Builder** — New "Create Remote" entry in
  the Universal Remote dashboard.  Users choose a template (TV / AC / Audio /
  Custom), then assign each button slot a signal via:
  - **Learn** — capture a live IR signal from a physical remote (point-and-press);
  - **Browse IRDB** — browse `.ir` files in the IRDB tree and pick from the
    commands currently listed by the browser UI;
  - **Skip** — leave the slot unmapped.
  Assigned slots display a 7-character truncated label.  After editing all slots,
  pressing RIGHT opens the virtual keyboard to name the file; the remote is saved
  to `0:/IR/Custom/<name>.ir` using the existing `flipper_ir_write_header()` /
  `flipper_ir_write_signal()` primitives.  Custom remotes are accessible via
  "Browse IRDB" → Custom/.
- **IR infrared_capture_one_signal()** — New exported function in `m1_infrared.c`
  that initialises the IR decoder, captures exactly one signal from a physical
  remote, displays the result briefly, and returns the `IRMP_DATA` to the caller.
  Used by the custom remote builder for in-builder signal learning.

## [0.9.0.102] - 2026-04-15

### Added

- **IR Universal Remote: IRDB Search** — New "Search" option on the Universal
  Remote dashboard (between Browse IRDB and Learned). Type a brand, model, or
  filename fragment using the virtual keyboard; the firmware walks all 3 levels
  of the IRDB tree (category → brand → device) and displays up to 16 matching
  `.ir` files in a scrollable list. Selecting a result opens its command list
  and records it in Recent history. Search results list uses font-aware helpers
  (`m1_menu_font()`, `m1_menu_item_h()`) to respect user text size settings.

### Changed

- **Documentation: Text size compliance for imports** — Added agent instructions
  to CLAUDE.md and `documentation/flipper_import_agent.md` requiring all imported
  features with scrollable lists to use the Hapax font-aware helpers
  (`m1_menu_font()`, `m1_menu_item_h()`, `m1_menu_max_visible()`, `M1_MENU_VIS()`)
  as a blocking merge requirement. Neither Flipper nor upstream Monstatek have
  user-configurable text size — every imported UI must be adapted.

## [0.9.0.101] - 2026-04-15

### Added

- **Infrared: Universal Remote quick-access remotes** — Momentum-style category
  quick-remotes (TV, AC, Audio, Projector, Fan, LED) accessible directly from
  the Universal Remote dashboard.  Each category opens a visual button grid
  with labeled buttons arranged like a real remote (e.g., Power, Vol+/−, Ch+/−,
  Mute, OK, Menu for TV).  Navigation with D-pad, OK sends the highlighted
  command.  First launch prompts user to select a brand/device from IRDB;
  subsequent launches load the last-used device instantly.  RIGHT button
  allows switching to a different device file.
- **Infrared: Brute-force power scan** — Iterate through all known power codes
  from the Universal_Power.ir file for a category (TV, Audio, Projector, Fan).
  Shows progress with brand name and progress bar.  User presses OK when
  device responds.  Accessible via LEFT button from any category quick-remote.
- **Infrared: Per-function universal IR files for TV** — Added
  Universal_Vol_up.ir, Universal_Vol_dn.ir, Universal_Mute.ir,
  Universal_Ch_next.ir, and Universal_Ch_prev.ir covering 19 major TV brands
  each (Samsung, LG, Sony, Philips, Panasonic, Vizio, TCL, Hisense, Toshiba,
  Sharp, JVC, Insignia, Roku, Fire TV, plus NEC/RC5/RC6 generics).

## [0.9.0.100] - 2026-04-15

## [0.9.0.99] - 2026-04-15

### Fixed

- **Firmware Download: Hapax source showing only 1 release** — Increased the
  GitHub API response buffer from 12 KB to 32 KB to accommodate Hapax releases
  which have 4 assets each (~5.5 KB of JSON per release vs ~3 KB for single-asset
  sources like C3).  Also fixed `http_get()` to detect silent buffer truncation
  and made the release parser handle truncated responses gracefully by showing
  whatever complete releases fit rather than an error.
- IR Universal Remote: exiting the dashboard with "Remote Mode" active no longer
  leaves the entire device stuck in portrait (90°) orientation.  The original
  screen orientation is now saved on entry and restored on exit.

## [0.9.0.98] - 2026-04-15

### Added

- **Sub-GHz: Momentum-compatible frequency list** — Expanded the frequency
  preset list from 17 to 62 entries, matching the Momentum firmware's default
  Sub-GHz frequency list. Adds coverage for 330 MHz (common Asian gate remotes),
  300–350 MHz band (27 presets), 387–468 MHz band (25 presets including full
  LPD433 range), and 779–928 MHz band (10 presets including 868 EU ISM
  variants). Hopper frequencies also updated to match Momentum defaults.

## [0.9.0.97] - 2026-04-15

### Fixed

- Config menus (Sub-GHz Config, LCD & Notifications settings): colon at end of
  label no longer abuts the value text in medium and large font modes.  Value
  x-position is now computed dynamically from the widest label in the current
  font instead of using a hardcoded pixel offset.

## [0.9.0.96] - 2026-04-15

### Changed

- **Documentation: ST-Link as primary debugger reference** — Release notes and
  hardware docs now reference ST-Link instead of J-Link as the primary SWD
  debugger, matching the standard STM32 development workflow. J-Link remains
  listed as an alternative where appropriate (VSCode debug configs, README).
- **VSCode: dynamic ELF path in debug configs** — All three debug
  configurations in `launch.json` (ST-Link IDE, ST-Link VSCode, J-Link) now
  use `${command:cmake.launchTargetPath}` instead of a hardcoded
  `MonstaTek_M1_v0800.elf` path, matching the pattern already used by the
  CubeProg flash tasks. Removed the stale `projectName` field.
- **J-Link flash script updated** — `scripts/program.jlink` now references the
  default source-tree Hapax build output (`M1_Hapax_v0.9.0.1.bin`) with
  comments documenting the version coupling to `m1_fw_update_bl.h`.

## [0.9.0.95] - 2026-04-15

### Changed

- Documentation: added CI stamper safety rule to CLAUDE.md and GUIDELINES.md —
  the [Unreleased] heading must only appear once (as the actual heading);
  writing that exact heading string in body text risks the CI stamper matching
  body text instead of the real heading (caused corruption at v0.9.0.78–83)
- Documentation: updated GUIDELINES.md changelog instructions — corrected
  misleading guidance that said `[Unreleased]` was only for non-compilation
  changes; all new entries go under `[Unreleased]` and CI handles version
  promotion automatically
- CI: changelog stamp PRs now labeled `changelog-stamp` and excluded from
  auto-generated release notes via `.github/release.yml`; added instructions
  in CLAUDE.md and GUIDELINES.md that stamp PRs must not appear in changelogs
  or release notes

## [0.9.0.93] - 2026-04-15

### Changed

- **NFC: Range extender support aligned with Flipper Zero** — Updated the RFAL
  analog config table (`rfal_analogConfigTbl.h`) to match Flipper Zero's
  `furi_hal_nfc.c` init sequence (stock and Momentum are identical here).
  Settings that now match Flipper: enable external load modulation (`lm_ext`),
  enable internal load modulation (`lm_dri`), disable overshoot/undershoot
  protection across all Poll NFC-A TX bitrates (106/212/424/848).  Intentional
  deviations from Flipper for maximum extender range: field activation thresholds
  lowered to 75mV (Flipper uses 105mV), deactivation rfe threshold lowered to
  25mV (Flipper uses 75mV), regulator forced to manual max voltage (Flipper
  auto-calibrates).  Also boost regulator voltage to max in `ReadIni()` and
  reduce polling interval from 1000ms to 500ms.  All range-extender-sensitive
  settings are now in the RFAL analog config table so they persist through
  `rfalSetMode()` / bitrate changes.
- **Documentation**: Added human-facing documentation for preferred patterns
  (UI/Button Bar Rules, Font-Aware Menu Implementation, Hardware State
  Management) to `DEVELOPMENT.md`, `.github/GUIDELINES.md`, and
  `.github/CONTRIBUTING.md`.  These patterns were previously documented only
  in `CLAUDE.md` (agent instructions).
- **Documentation**: Added button model and button-to-column mapping rule
  to `DEVELOPMENT.md`, `CLAUDE.md`, `.github/GUIDELINES.md`, and
  `m1_subghz_scene.h`.  Button bar columns must correspond to physical
  buttons (LEFT=LEFT, CENTER=OK, RIGHT=RIGHT).  Identified two existing
  violations in the migration backlog (`m1_subghz_scene_read.c`,
  `m1_subghz_scene_receiver_info.c`).

### Fixed

- **Sub-GHz Read scene button bar**: Fixed button-to-column mapping — moved
  `↓Config`/`↓Save` (DOWN actions) from CENTER to LEFT column, moved
  `Listen`/`Info`/`View` (OK actions) from RIGHT to CENTER column.  Also
  removed misleading `↓Config` label in history view (DOWN scrolls there).
- **Sub-GHz Receiver Info button bar**: Fixed button-to-column mapping — moved
  `↓Save` (DOWN action) from CENTER to LEFT column, moved `Send` (OK action)
  from RIGHT to CENTER column.
- **BLE Spam mode menu**: Converted hardcoded `MENU_ROW_H 9` to font-aware
  helpers (`m1_menu_item_h()`, `m1_menu_font()`, layout constants from
  `m1_scene.h`).  Menu now respects the user's text size setting.
- **IR Universal Remote dashboard**: Converted hardcoded `DASHBOARD_ITEM_HEIGHT`
  and `DASHBOARD_START_Y` to font-aware helpers.  Added scrollbar.  Dashboard
  now respects the user's text size setting.

## [0.9.0.91] - 2026-04-15

### Changed

- **NFC: Boost power and polling rate for range extender support** — Configure the
  ST25R3916 NFC frontend after RFAL init for improved range-extender compatibility,
  modelled after Flipper Zero's `furi_hal_nfc.c` init sequence. Changes reflected in
  this PR include boosting regulated voltage to maximum (`rege=0x0F`, `reg_s=1`),
  enabling external load modulation (`lm_ext`), lowering field detection thresholds
  for extended coupling distances, and reducing polling interval from 1000ms to
  500ms. During polling, the ST25R3916 poller bitrate path reapplies
  `CHIP_POLL_COMMON`, but does not rerun `CHIP_INIT`, so post-init `lm_ext` and
  field-threshold adjustments generally persist rather than being broadly reset by
  RFAL polling. These changes improve NFC range-extender support and overall NFC read
  responsiveness.
- **Read Raw scene rebuilt to match Momentum firmware workflow** — Complete rewrite of
  the Sub-GHz Read Raw scene with three distinct states (Start → Recording → Idle) and
  a Momentum-aligned button bar that correctly maps physical buttons to display columns
  (LEFT=Config/Erase, OK=REC/Stop/Send, RIGHT=Save).  Key improvements:
  - **Button bar fixed**: Eliminated the mismatched `↓Config` in center / `OK:Rec` on
    right layout.  All three button bar columns now correspond to their physical buttons
    (LEFT column ← LEFT button, CENTER column ← OK button, RIGHT column ← RIGHT button).
  - **Post-recording actions**: After stopping a recording, users can now Send (replay
    the captured raw signal via OK), Save (rename with virtual keyboard via RIGHT), or
    Erase (delete and start over via LEFT) — matching Flipper/Momentum's Read Raw UX.
  - **BACK during recording stops (not exits)**: Pressing BACK while recording now stops
    the capture and transitions to the Idle state, preserving the recording.  Previously
    it would exit the scene entirely.
  - **Status bar cleaned up**: Eliminated the separate sample count and state indicator
    that competed for space.  Now shows a single combined string on the right side of the
    status bar (e.g. "REC 42k" during recording, "42k spl" after recording).
  - **Filename display**: After recording, the auto-generated filename is shown centered
    in the waveform area (matching Momentum's loaded-key display pattern).
  - **No more clipped text**: Long labels like "OK:Stop" and "OK:New" that extended past
    the 128px display edge are replaced with concise single-word labels in the correct
    column positions.
- Documentation updated across `README.md`, `ARCHITECTURE.md`, `DEVELOPMENT.md`,
  `CONTRIBUTING.md`, `GUIDELINES.md`, and the Web Updater documentation to
  highlight Hapax's unique GitHub-first development model — automated CI/CD
  builds, GitHub Releases, GitHub Pages Web Updater, automated testing, and
  transparent development all on GitHub.

## [0.9.0.90] - 2026-04-15

### Fixed

- **OTA firmware download "Bad response" error** — HTTP response headers arriving
  in fragments over SSL (common when the ESP32 delivers data in separate SSL records)
  caused `parse_http_headers()` to fail because the initial `tcp_recv()` did not
  contain the complete header block (`\r\n\r\n` terminator missing).  Added a header
  accumulation loop that reads additional data until headers are complete or timeout.
  Also separated the SPI working buffer used by `tcp_recv()` / `tcp_recv_available()`
  from the main `s_at_buf` so accumulated header data is not clobbered between reads.

## [0.9.0.88] - 2026-04-14

### Fixed

- **Home screen battery indicator now updates while idle** — the battery percentage
  and charging icon on the home screen (splash/welcome screen) previously showed a
  stale snapshot taken at the time the screen was last drawn.  The home screen now
  redraws its battery indicator every 1 second while the device is idle (reduced from
  2 s), and only when the battery level or charging state actually changed — matching
  Momentum firmware's conditional-refresh pattern.  This eliminates unnecessary redraws
  and reduces latency for charge-state changes appearing on screen.

## [0.9.0.87] - 2026-04-14

### Fixed

- **Web updater: Android "No compatible devices found"** — Added Android-specific
  troubleshooting panel with connection tips (USB OTG requirement, system permission
  dialog) and a "Grant USB Access" button that uses the WebUSB API to trigger
  Android's USB device permission dialog before the Web Serial port picker is shown.
  Improved error message when port selection fails on mobile to guide users through
  the permission flow.

- **OTA download: misleading error code when ESP32 deinit** — `http_get()` and
  `http_download_to_file()` previously returned `HTTP_ERR_NO_WIFI` for any
  `http_is_ready()` failure, even when the real cause was a deinitialized ESP32
  HAL (e.g. after `wifi_scan_ap()` exits).  Both functions now check WiFi and
  ESP32 readiness separately: `HTTP_ERR_NO_WIFI` when WiFi is not connected,
  `HTTP_ERR_ESP_NOT_READY` when the ESP32 HAL or SPI task is not initialized.
  Five additional regression tests added to `test_http_client_parse` covering
  the `http_readiness_status()` split-check logic.

- **OTA download: unnecessary ESP32 init/deinit when WiFi not connected** —
  `fw_download_start()` and `esp32_fw_download_start()` unconditionally called
  `m1_esp32_init()` / `esp32_main_init()` at entry, even when WiFi was not
  connected or `M1_APP_WIFI_CONNECT_ENABLE` was absent.  The block is now
  guarded with `#ifdef M1_APP_WIFI_CONNECT_ENABLE` and gated on
  `wifi_is_connected()`, so no hardware init/deinit side effects occur when the
  download will immediately fail the WiFi readiness check.

## [0.9.0.86] - 2026-04-14

### Fixed

- **OTA download: DNS error after WiFi connect** — `fw_download_start()` and
  `esp32_fw_download_start()` did not initialize the ESP32 SPI interface before
  making HTTP requests.  After `wifi_scan_ap()` exits (user presses BACK after
  connecting), `m1_esp32_deinit()` disables the SPI hardware.  The old
  `http_is_ready()` check only tested the task-level flag (never cleared by
  deinit) and the WiFi connected flag, so both download functions proceeded with
  a disabled SPI, causing all `AT+CIPDOMAIN` commands to time out and return
  `HTTP_ERR_DNS_FAIL`.  Fix: both functions now call `m1_esp32_init()` and
  `esp32_main_init()` (no-op if already done) before using the network, and
  call `m1_esp32_deinit()` on all exit paths per the architecture rules.
  `http_is_ready()` now also checks `m1_esp32_get_init_status()` (HAL-level
  flag) as a defense-in-depth guard.  Two regression tests added to
  `test_http_client_parse`.

- **Web Updater — Flash section hidden when device info query times out.** The
  "Flash Firmware" panel was only revealed after a successful device info RPC
  response.  If the device info query timed out (as seen on some connections),
  the flash section stayed hidden even though the device was connected and
  responsive.  The flash section is now shown as soon as the serial connection
  is established, regardless of device info query outcome.

## [0.9.0.85] - 2026-04-14

### Changed

- **Home-screen status icons now reflect actual state** — The SD/BT/WiFi status
  icons on the home screen (reachable at boot and by pressing BACK from the main
  menu) are now gated on actual peripheral state rather than shown unconditionally.
  SD card icon already required physical detection; BT icon now only appears when
  a Bluetooth device is actively connected (`bt_get_connection_state()->connected`),
  and WiFi icon only appears when associated to an AP (`wifi_is_connected()`).
  At boot, BT and WiFi are absent until a connection is established; the SD icon
  continues to appear whenever a card is physically detected.

## [0.9.0.84] - 2026-04-14

### Fixed

- **OTA download menus respect font size setting** — the source and release
  selection lists in the firmware and ESP32 download flows now use
  `m1_scene_draw_menu()` instead of a custom draw function.  This brings them
  in line with the rest of the UI: they honour the user's Text Size preference
  (Small/Medium/Large), display the standard proportional scrollbar, use the
  correct `m1_menu_max_visible()` item count for the active font size, and show
  the standard centred title with separator line.

## [0.9.0.78] - 2026-04-14

> **Note:** Versions 0.9.0.79 through 0.9.0.83 were released during a CI
> changelog stamper bug where the stamper matched body text instead of the
> heading (see [0.9.0.95]).  Their entries are included in this section.
> Detailed entries for features and fixes in the 0.9.0.28–0.9.0.77 range
> that were previously duplicated here have been removed — see each
> individual version section below for those entries.

### Added

- **SubGhz unit tests: signal history ring buffer** (`tests/test_subghz_history.c`) —
  19 tests covering `subghz_history_add/get/reset`: duplicate detection, circular
  overflow, RSSI update, extended fields, saturation at 255, ordering (index 0 =
  most recent), and boundary conditions.

- **LFRFID Manchester decoder tests** (`tests/test_lfrfid_manchester.c`) — 18 unit
  tests for the generic Manchester decoder: `manch_init()` parameter setup,
  `manch_push_bit()` MSB shift register with byte patterns and overflow,
  `manch_get_bit()` position extraction, `manch_is_full()` completion check,
  `manch_reset()` state clearing, and `manch_feed_events()` half/full period
  classification with edge normalization and overflow protection.

- **HTTP client URL parser tests** (`tests/test_http_client_parse.c`) — 9 unit tests
  for `parse_url()`: validates HTTPS detection (is_https flag triggers SSL path),
  host/port/path extraction, custom ports, default path, invalid scheme rejection,
  buffer overflow protection, and real GitHub API URL parsing for OTA scenarios.

### Changed

- **Extracted `asset_matches_filter()`** — The OTA asset name filtering function
  in `m1_fw_source.c` is now non-static and declared in `m1_fw_source.h` as
  `fw_source_asset_matches_filter()`, enabling host-side unit testing of the
  include/exclude filter logic without firmware HAL dependencies.

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

- **Documentation: Preferred Modularization Pattern** — Documented the
  extract-pure-logic-to-standalone-module approach (established in PR #106)
  as the canonical development pattern.  Added to `CLAUDE.md` (full
  specification with examples, callback decoupling technique, what NOT to
  extract), `DEVELOPMENT.md`, `.github/GUIDELINES.md`, and `ARCHITECTURE.md`
  (with list of successfully extracted modules).  This pattern applies to
  all new development, refactors, and bug fixes where monolithic files mix
  pure logic with hardware-coupled code.

- **Firmware download source config** — `fw_sources.txt` now supports a
  `Category:` field (`firmware` or `esp32`) to distinguish M1 firmware
  sources from ESP32 AT firmware sources.  Existing configs without a
  `Category:` field default to `firmware`.

- **Documentation: font inventory in CLAUDE.md** — Added a comprehensive font
  inventory table listing the 22 u8g2 fonts linked/used by application code,
  their display-role macros, BadUSB API availability, ascent values, and
  lowercase support.  Includes u8g2 suffix reference and font maintenance
  rules so future agents keep the table current when fonts are added,
  removed, or reassigned.

- **Settings — About screen** — Removed the bottom Prev/Next inverted bar.
  L/R navigation is intuitive; replaced with a subtle centered `< 1/3 >` page
  indicator.  Added title header with separator line for visual consistency.
  Content repositioned to use the full display area.

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

- **Sub-GHz Config scene: add TX Power and ISM Region settings** — The legacy
  `sub_ghz_radio_settings()` screen (stock Monstatek) exposed TX Power, Modulation,
  and ISM Region.  The scene-based Config only had Frequency, Hopping, Modulation,
  and Sound — TX Power and ISM Region were inaccessible.  Now all 6 settings are
  available in the scene Config screen with scrollable navigation.  ISM Region
  changes are persisted to SD card on exit via `settings_save_to_sd()`.  TX Power
  changes are applied via new `_ext` accessor functions that bridge the static
  `subghz_tx_power_idx` to scene code.

### Fixed

- **Sub-GHz CUSTOM band antenna switch** — Frontend (antenna path) for CUSTOM
  band frequencies is now selected based on the actual target frequency, not the
  base config band.  Fixes incorrect antenna path when the radio config base band
  differs from the target frequency (e.g. 915 FSK config loaded for 433 MHz FSK).

- **Sub-GHz preset parser consistency** — `flipper_subghz_preset_to_modulation()`
  now uses case-insensitive matching and recognises ASK presets, matching the replay
  function's `stristr()` approach.

- **Misleading "Failed. Retrying..." WiFi message** — The scan failure screen
  said "Failed. Retrying..." but no retry logic existed; the device just waited
  for the user to press Back with no on-screen hint.  Changed to "Scan failed!"
  which accurately describes the state.

## [0.9.0.77] - 2026-04-14

### Fixed

- OTA "Connection failed" — add SNTP time sync, DNS reachability pre-check, stale TCP connection cleanup, and SSL session retry logic

### Changed

- CI: create changelog-stamp PR instead of pushing directly to main

## [0.9.0.76] - 2026-04-13

### Fixed

- Dark mode light border — clear full ST7567 display RAM (132×65) on mode change to eliminate residual lit pixels at LCD edges

## [0.9.0.75] - 2026-04-13

### Changed

- Documentation: highlight Web Updater as recommended flashing method in README
- Documentation: CI trigger docs clarification

### Fixed

- Agent-confusing CRC comments and `va_list` undefined behavior in firmware source

## [0.9.0.74] - 2026-04-13

### Fixed

- Web updater showing "No compatible devices found" on Android — skip WebSerial VID filter on mobile browsers

### Added

- Host-side unit tests, Python RPC test harness, and testing strategy documentation

## [0.9.0.73] - 2026-04-13

### Fixed

- OTA "Connection failed" — configure SSL (`AT+CIPSSLCCONF`) before HTTPS connections

## [0.9.0.71] - 2026-04-13

### Added

- Browser-based firmware updater using Web Serial API — flash firmware directly from a web page without installing desktop software

### Fixed

- BLE Spam crashes — thread-safe `esp_queue` linked-list operations with task suspension around critical sections

## [0.9.0.70] - 2026-04-12

### Added

- Sub-GHz Read Raw: Flipper-style RSSI spectrogram visualization — real-time frequency-domain signal strength display during raw capture

## [0.9.0.69] - 2026-04-12

### Added

- 5 non-Sub-GHz test suites (Flipper file parser, IR file parser, RFID file parser, NFC file parser, OTA asset filter) and preferred unit testing pattern documentation

## [0.9.0.68] - 2026-04-12

### Added

- Large text size option — Settings → LCD & Notifications → Text Size now has Small / Medium / Large with per-size row height, font, and visible item count

## [0.9.0.67] - 2026-04-11

### Fixed

- OTA firmware update — never actually worked due to chunked HTTP transfer encoding, response buffer aliasing, and silent error swallowing; rewrote download pipeline with streaming TCP/SSL reader

## [0.9.0.66] - 2026-04-11

### Fixed

- Dark mode — hardware LCD inversion via ST7567 command + replaced all stock `u8g2_FirstPage()`/`u8g2_NextPage()` calls with wrapper functions that apply inversion consistently

## [0.9.0.65] - 2026-04-11

### Changed

- BLE Spam rewrite with dynamic packet generation — ported Momentum-style per-cycle randomization of model IDs, battery levels, colors, and encrypted payloads; MAC rotation; ~100ms cycle time (was 1500ms); per-brand mode selection menu (Apple, Samsung, Google, Microsoft, Random)

## [0.9.0.64] - 2026-04-11

### Added

- 67 new unit tests for block decoder, block encoder, Manchester codec, and datatype utilities
- Extracted offline RAW decoder and playlist path remapper into standalone modules with 30 new tests

### Changed

- Refactored `sub_ghz_replay_flipper_file()` to use extracted modules (key encoder, raw line parser, RAW decoder, playlist remapper)

## [0.9.0.63] - 2026-04-11

### Changed

- Documentation: replaced hardcoded `M1_Hapax_v0.9.0.1` references in CLAUDE.md, README.md, and DEVELOPMENT.md with `M1_Hapax_v<VERSION>` placeholders

## [0.9.0.62] - 2026-04-11

### Added

- Dark mode (display inversion) toggle in Settings → LCD & Notifications — initial software-only implementation

## [0.9.0.61] - 2026-04-11

### Fixed

- Splash screen "M1" reading as "MI" — switched to `helvB08_tr` font with distinct `1` glyph; documented font inventory and maintenance rules in CLAUDE.md

## [0.9.0.60] - 2026-04-11

### Added

- Text Size setting (Small / Medium) in Settings → LCD & Notifications — adjustable menu row height and font for readability

## [0.9.0.59] - 2026-04-11

### Added

- Battery indicator on splash screen — shows battery level during boot

## [0.9.0.58] - 2026-04-11

### Fixed

- PR #94 review findings: deduplicate decode results in Sub-GHz Read history, sync `npulsecount` between ISR and event loop, fix stale comment

### Changed

- Documentation: add no-duplicate-subsection heading rule to changelog instructions

## [0.9.0.57] - 2026-04-11

### Added

- WiFi deauthentication and station scan — `AT+DEAUTH` and `AT+STASCAN` commands for ESP32 custom AT firmware; new WiFi scene menu items (requires neddy299/esp32-at-monstatek-m1 firmware)

## [0.9.0.56] - 2026-04-11

### Changed

- Build: suppress `-Woverlength-strings` for vendored `u8g2_fonts.c` via per-file CMake compile options
- CI: automatic changelog `[Unreleased]` version stamping on each release build

## [0.9.0.55] - 2026-04-10

### Fixed

- Sub-GHz Read scene: lightweight resume after child scene pop, live retune during RX, hopper state preservation across Config visits

### Added

- Decode action for saved RAW `.sub` files — offline protocol decode from the Saved action menu

## [0.9.0.54] - 2026-04-10

### Added

- Regression unit tests for JSON parser, Flipper Sub-GHz parser, Flipper file parser, and protocol registry
- Bug-fix regression test requirement added to project docs
- GitHub Actions workflow for Jekyll / GitHub Pages deployment

## [0.9.0.53] - 2026-04-10

### Added

- Text-mode virtual keyboard with SPACE key — enables WiFi password entry with spaces and special characters

## [0.9.0.52] - 2026-04-10

### Fixed

- OTA release fetch — replaced `AT+HTTPCLIENT` (4 KB response limit) with raw TCP/SSL streaming in `http_get()` to handle GitHub API's large JSON responses

## [0.9.0.51] - 2026-04-10

### Added

- Code quality infrastructure: on-demand cppcheck static analysis, Unity test framework, AddressSanitizer support, Doxygen API docs, MISRA compliance guidance

### Changed

- CI: removed push-to-main trigger from `ci.yml` to avoid redundant builds (release workflow handles main branch)

## [0.9.0.50] - 2026-04-10

### Changed

- Read Raw waveform rendering polished — oscilloscope-style square wave with mark/space visualization, clamped amplitude, and smoothed transitions

## [0.9.0.49] - 2026-04-10

### Fixed

- Splash screen: use `_tr` font variant to include lowercase characters (was `_tu` — uppercase only)

### Changed

- File browser: removed "Press OK to browse" prompt; directories sort before files

## [0.9.0.48] - 2026-04-10

### Fixed

- IR Send All: wait specifically for `Q_EVENT_IRRED_TX` in transmission loop (was consuming unrelated queue events)
- Hardware state management across ESP32, NFC, and IR modules — documented `init`/`deinit` patterns and ensured all exit paths call deinit

## [0.9.0.47] - 2026-04-10

### Fixed

- Sub-GHz playlist radio reinit — call `menu_sub_ghz_init()` after each file replay to restore radio power; documented SI4463 radio state management pattern
- Sub-GHz Read Raw radio sleep bug — ensure `menu_sub_ghz_init()` is called before starting RX in both Read and Read Raw scenes

## [0.9.0.46] - 2026-04-10

### Changed

- Splash screen layout updated

## [0.9.0.45] - 2026-04-10

### Fixed

- 2FSK replay — extended FSK modulation support to all Sub-GHz frequencies (was limited to 433 MHz band)

## [0.9.0.44] - 2026-04-10

### Added

- Hapax H-mark logo bitmaps and PNG previews for all three display sizes

### Fixed

- Sub-GHz Read Raw feedback and Emulate for native `.sgh` files
- Windows reserved filename error: renamed `ir_database/AC/AUX.ir` to `AUX_.ir`

## [0.9.0.43] - 2026-04-08

### Fixed

- NFC Amiibo emulation for Nintendo Switch compatibility — fixed HALT response timing in T2T listener

## [0.9.0.42] - 2026-04-08

### Changed

- Standardized Settings menus to match Sub-GHz Config layout — consistent `< value >` arrows, UP/DOWN selection, LEFT/RIGHT value change

## [0.9.0.41] - 2026-04-08

### Fixed

- WiFi/BT scan regression — removed AT readiness probe and shortened mode-set timeout that caused false failures
- Sub-GHz raw recording — implemented SD card streaming, waveform display, and save flow; fixed dangling pointer for datfile infix

## [0.9.0.40] - 2026-04-07

### Fixed

- Delayed ESP32 screen response — optimized AT probe loop, added mode-set timeouts (`MODE_SET_RESP_TIMEOUT`), drained stale button events on module entry

## [0.9.0.39] - 2026-04-07

### Fixed

- Overlapping text and tight spacing in Sub-GHz scenes — adjusted Y coordinates and row heights for clean rendering

## [0.9.0.38] - 2026-04-07

### Fixed

- Sub-GHz config scene — removed redundant button bar, fixed row overlap, aligned layout with menu scene pattern

## [0.9.0.37] - 2026-04-07

### Added

- Sub-GHz: Info action in saved file menu — displays protocol, key, frequency, and modulation for the selected `.sub` file
- IR: Saved File Actions menu — Send All, Info, Rename, Delete for `.ir` files

### Changed

- Documentation: elevated Saved Item Actions as canonical UX standard across all modules

## [0.9.0.36] - 2026-04-07

### Fixed

- WiFi/BT scan failure — added AT readiness probe with retry after ESP32 init to ensure SPI transport is ready before sending commands

### Changed

- Removed ESP32 boot-time auto-init — ESP32 is now initialized on-demand only, matching stock firmware behavior

## [0.9.0.35] - 2026-04-07

### Removed

- Legacy dead code from Infrared (~10 lines), GPIO, NFC (~28 lines), WiFi (~50 lines), and Bluetooth (~300 lines) modules — empty functions, unused view tables, dead `#if 0` blocks, and never-compiled fallback paths

## [0.9.0.34] - 2026-04-07

### Fixed

- Sub-GHz Read never recognising signals — batch-drain pulse events from FreeRTOS queue, use TIM1 CCR1 hardware capture instead of software delta, increase timer period to avoid rollover on long pulses

## [0.9.0.33] - 2026-04-07

### Fixed

- Bottom-bar overlap in Sub-GHz Config, Read, Playlist, NeedSaving, Add Manually list, and Radio Settings screens — adjusted Y coordinates so button bar text does not collide with menu items

## [0.9.0.32] - 2026-04-06

### Fixed

- Sub-GHz DMA buffers aligned to 32-byte boundary for D-Cache safety — `malloc` + manual alignment replaced with `aligned_alloc`-style wrapper

## [0.9.0.31] - 2026-04-06

### Fixed

- Main menu off-by-one — drained stale button events on module exit and validated `disp_window_active_row` matches `sel_item` on `MENU_UPDATE_REFRESH`

## [0.9.0.30] - 2026-04-06

### Fixed

- `uint8_t`/`bool` type mismatch in Oregon V1 and Oregon3 decoders — changed `bit_val` to `bool` and updated `subghz_protocol_blocks_add_bit()` parameter type

## [0.9.0.29] - 2026-04-06

### Fixed

- Sub-GHz Read scene: frequency=0 saved for non-hopper signals, history auto-select picked oldest signal, hopper never actually retuned the radio, `current_freq_hz` never populated

### Changed

- Documentation: comprehensive audit and consistency update across CLAUDE.md, DEVELOPMENT.md, README.md, and ARCHITECTURE.md

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

- **Sub-GHz Read scene — Flipper-consistent behavior**:
  - RX auto-starts on scene entry (no "Press OK to listen" idle screen)
  - L/R buttons retune frequency while actively receiving (live retune)
  - History list auto-opens when first signal is decoded
  - Newest signal auto-selected in history list
  - History preserved across child scene navigation (ReceiverInfo, Config)
  - Config accessible via DOWN during active RX (not just idle)
  - BACK exits scene directly instead of requiring two presses (stop → exit)
  - Bottom bar updated to show available actions contextually

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

- Renamed `hardware/` directory to `cad/` to avoid case-insensitive name
  collision with `HARDWARE.md` (which upstream Monstatek/M1 places at
  repo root).

- **Documentation: Public Forks Tracker** — added a "Public Forks Tracker"
  section to `CLAUDE.md` with a table of all known Monstatek/M1 public forks,
  their latest commit hashes/dates (UTC), last-reviewed timestamps, and
  instructions for agents to discover new forks and keep the table current.

### Fixed

- **Firmware update menu overflow**: The 4th menu item ("Download") no longer
  overflows into the info box below. Switched from the tall info box (Y=32) to
  the short info box (Y=42) and condensed the version display from three rows
  to two (file name + compact "old > new" version line).

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

- **Sub-GHz recording stability** — removed protocol decoder polling
  (`subghz_decenc_read()`) from the `Q_EVENT_SUBGHZ_RX` handler in
  `subghz_record_gui_message()`.  Running the decoder plus a full display
  redraw on every RX batch competed with time-critical SD-card writes and
  could cause ring-buffer overflows and device reboots under sustained RF
  activity.  Decoding still works normally in Read and Weather Station modes.
  (Ref: bedge117/M1 C3.4 crash fix.)

## [0.9.0.27] - 2026-04-06

### Added

- 5 Sub-GHz tools added to scene menu: Spectrum Analyzer, RSSI Meter, Freq Scanner, Weather Station, Brute Force — now 11 menu items matching C3 parity

### Changed

- CI: skip builds for non-compilation changes (documentation, databases, IDE configs, CI workflow files)
- Documentation: mandated scene-based architecture for all modules in CLAUDE.md, added migration guide

## [0.9.0.26] - 2026-04-06

### Changed

- Sub-GHz menu and saved action scenes — removed "OK" button bar, added right-edge scrollbar position indicator; items fill available vertical space

## [0.9.0.25] - 2026-04-06

### Fixed

- Firmware update menu overflow — 4th item ("Download") no longer overflows into the info box; switched to short info box layout

## [0.9.0.24] - 2026-04-06

### Changed

- Renamed release binary suffix `_SD.bin` → `_wCRC.bin` — aligns with Monstatek Official and C3/bedge117 naming convention

## [0.9.0.23] - 2026-04-06

### Added

- C3 (bedge117) as third default firmware download source in `fw_sources.txt`

## [0.9.0.22] - 2026-04-06

### Added

- WiFi Firmware Download — browse and download firmware images from GitHub Releases directly to SD card; configurable sources via `fw_sources.txt`; real-time download progress bar
- Reusable HTTP client (`m1_http_client.c/h`) for API requests and large file downloads with HTTPS, redirect following, and progress callbacks
- Minimal JSON parser (`m1_json_mini.c/h`) for GitHub API responses — no heap allocation
- ESP32 AT TCP/SSL and HTTP command definitions (`AT+HTTPCLIENT`, `AT+CIPSTART`, etc.)
- WiFi state accessors: `wifi_is_connected()` and `wifi_get_connected_ssid()`

## [0.9.0.21] - 2026-04-04

### Changed

- Removed redundant "Back" labels from all app button bars and action menus — hardware back button is self-explanatory
- Redistributed Sub-GHz saved action menu layout for 3-item spacing after removing Back item

## [0.9.0.20] - 2026-04-04

### Fixed

- Sub-GHz signal detection not working — synchronized ISR state machine (`PULSE_DET_EOP`/`PULSE_DET_IDLE` return values were discarded), added pulse handler state reset, cleared stale `subghz_record_mode_flag`

### Changed

- Sub-GHz Read scene — Flipper-consistent behavior: RX auto-starts on entry, L/R live retune, history auto-opens on first decode, BACK exits directly

## [0.9.0.19] - 2026-04-04

### Fixed

- Sub-GHz Read crash — added missing `subghz_decenc_init()` (NULL function pointer → HardFault), fixed pulse data never fed to decoder, removed incorrect `subghz_record_mode_flag` usage from Read scene

## [0.9.0.18] - 2026-04-04

### Added

- Sub-GHz Playlist Player — reads `.txt` playlist files from `/SUBGHZ/playlist/` and transmits each referenced `.sub` file sequentially; adjustable repeat count; Flipper path remapping; curated UberGuidoZ playlist files included

## [0.9.0.17] - 2026-04-04

### Added

- IR database expansion — imported 1,296 curated IR remote files from Flipper-IRDB (CC0 license) into `ir_database/`; total IR library now 1,412 files
- Sub-GHz signal database — 313 curated `.sub` files from UberGuidoZ/Flipper (GPLv3) in `subghz_database/`

## [0.9.0.16] - 2026-04-04

### Added

- Sub-GHz Scene Manager framework — Flipper-inspired stack-based scene architecture with `on_enter`/`on_event`/`on_exit`/`draw` callbacks, button bar, status bar, and RSSI bar renderers
- Sub-GHz scenes: Menu (5 items), Read (protocol decode + history), Read Raw (oscilloscope waveform), Receiver Info (signal detail), Config, Save flow (name → success → unsaved dialog), Saved (file browser + action menu), Freq Analyzer wrapper
- Bridge functions (`*_ext()` wrappers) exposing static radio control, RSSI read, protocol TX, and waveform functions to scene files

### Changed

- SiN360 fork sync — merged sincere360/M1_SiN360 v0.9.0.4 using `ours` strategy

## [0.9.0.15] - 2026-04-02

### Changed

- Renamed `hardware/` directory to `cad/` to avoid case-insensitive name collision with `HARDWARE.md`

## [0.9.0.14] - 2026-04-02

### Added

- Enclosure STEP files — 6 M1 enclosure 3D CAD models (STEP AP214) under `cad/step/`

## [0.9.0.13] - 2026-04-02

### Added

- Public Forks Tracker section in CLAUDE.md — table of all known Monstatek/M1 public forks with commit hashes, dates, and review status

### Fixed

- Sub-GHz recording stability — removed protocol decoder polling (`subghz_decenc_read()`) from the `Q_EVENT_SUBGHZ_RX` handler to prevent ring-buffer overflows under sustained RF activity

## [0.9.0.12] - 2026-04-02

### Changed

- Added `[Unreleased]` changelog convention and upstream merge policy to CLAUDE.md
- CI: skip release builds for docs-only merges to main

## [0.9.0.11] - 2026-04-02

### Removed

- Legacy `Release/` folder — removed STM32CubeIDE v0.8.0.0 build artifacts; added `Release/` to `.gitignore`

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

### Fixed

- **SD card firmware update now works from stock Monstatek firmware** — the `_SD.bin` binary
  now includes a self-referential CRC at offset `0xFFC14` (the stock `FW_CRC_ADDRESS`) that
  satisfies the stock firmware's post-flash `bl_crc_check()` verification. Previously, the
  Hapax CRC extension magic sentinel (`0x43524332`) occupied that offset, causing the stock
  firmware to always fail the CRC comparison with "Update failed!".

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

- **Documentation: `documentation/hardware_schematics.md`** — expanded from partial 2-sheet
  extract to full 7-sheet hardware inventory. Added IC designators for NFC board (U2–U7,
  Q4), crystal refs (Y1/Y2), SWD/USB-UART header pinouts, LCD1 spec (128×64, 1/65 duty),
  J4/J5/J6/J7 connector details, IR subsystem (U6, LD1/LD2, Q3), LED indicators (D11, IC1),
  complete power-rail summary, passive component groupings (R/C/L), test points, and
  mounting holes. Capability summary updated with IR TX/RX, RGB LED, FDCAN, and corrected
  component references throughout.

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

- Documentation governance: added "Documentation Update Rules" to `CLAUDE.md` and expanded
  `.github/GUIDELINES.md` with project-specific CHANGELOG format guidance; all future agent
  sessions are required to update `CHANGELOG.md`, `README.md`, and
  `documentation/flipper_import_agent.md` as part of every change

- Documentation: expanded `HARDWARE.md` with MCU specs, RAM, speed, and peripheral table;
  added secondary-reference note in `CLAUDE.md` pointing to new hardware schematics doc

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

- Crash on RFID read caused by rogue `pulse_handler` call in `TIM1_CC` ISR

### Added

- **CI: Manual build-and-release workflow** (`.github/workflows/build-release.yml`):
  `workflow_dispatch`-only GitHub Actions workflow that installs the ARM GCC toolchain,
  builds the firmware with CMake + Ninja, injects CRC32 + Hapax metadata via
  `append_crc32.py`, and publishes a GitHub Release with `.elf`, `.hex`, raw `.bin`, and
  `_wCRC.bin` artifacts. Tag and release title are auto-derived from source versions but
  can be overridden as inputs.

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

## [0.8.0.0] - 2026-02-05

### Added

- Initial open source release
- STM32H573VIT6 support (2MB Flash, 100LQFP)
- Hardware revision 2.x support
