---
name: forks-tracker
description: Public forks tracker (Monstatek, bedge117, sincere360, dagnazty and others) and the upstream merge policy explaining why Hapax does not merge from Monstatek. Load when auditing upstream/forks, comparing implementations, or deciding whether to cherry-pick a change.
---

# Forks Tracker & Upstream Merge Policy

> Extracted from CLAUDE.md. Load when auditing forks or evaluating an upstream cherry-pick.

### Upstream Merge Policy — Why We Don't Merge Monstatek

As of April 2026, **we do not merge from Monstatek/M1 upstream**. Reasons:

1. **Upstream is stale.** Monstatek/M1 has been at `v0.8.0.0` since mid-2024 with no
   public commits. There is nothing new to merge.
2. **Divergence is too large.** Hapax has rewritten the build system (CMake + Ninja
   replacing STM32CubeIDE managed makefiles), added 60+ Sub-GHz protocols, a Flipper
   file compatibility layer (`lib/furi/`), CAN bus support, ESP32-C6 SPI AT integration,
   LF-RFID / NFC / IR Flipper import, and a full CI/CD pipeline. A blind merge would
   produce hundreds of conflicts with no benefit.
3. **Version scheme divergence.** Hapax owns `FW_VERSION_MINOR` (9) and `FW_VERSION_RC`.
   Monstatek's version numbering assumptions no longer apply.
4. **Cherry-pick, don't merge.** If Monstatek ever pushes a meaningful update, the
   correct approach is to **review the diff, cherry-pick relevant changes**, and adapt
   them to the Hapax codebase — not to merge the branch wholesale.

If Monstatek publishes a new release in the future, re-evaluate this policy by:
- Fetching `monstatek/main` and inspecting the diff against our `main`
- Cherry-picking any bug fixes or HAL updates that apply
- Bumping `FW_VERSION_MAJOR` only if upstream introduces a breaking API change

---

## Public Forks Tracker

Track all known public forks of Monstatek/M1 so agents can determine whether
a fresh analysis is warranted.  All timestamps are **UTC**.

### Known Forks

> **Naming note:** "C3"/"C3.12" below refers exclusively to bedge117's **STM32
> firmware** fork (`bedge117/M1`). It is unrelated to the ESP32-C6 coprocessor
> firmwares also authored by bedge117 — those are tracked and named separately
> as **CD3-AT** (`bedge117/esp32-at-monstatek-m1`, AT-command based) and **CD3**
> (`bedge117/m1-esp32-brain`, native binary RPC); see
> [`documentation/esp32_firmware.md`](../../../documentation/esp32_firmware.md#source-repository)
> and the [`esp32-coprocessor`](../esp32-coprocessor/SKILL.md) skill.

| Fork | Owner | Activity | Latest Commit (SHA) | Latest Commit Date (UTC) | Last Reviewed by Hapax | Notes |
|------|-------|----------|---------------------|--------------------------|------------------------|-------|
| [Monstatek/M1](https://github.com/Monstatek/M1) | Monstatek | **Active** (upstream) | `4c77bb86` | 2026-06-05 06:30 | 2026-07-21 00:30 | Original upstream. v0.8.0.2 (2026-05-08): added Universal Remote (inferior to Hapax's) and Flipper IR flat database (inferior to Hapax's ir_database). `battery_log.c` diagnostic CSV logger is worth cherry-picking. `bq27421_golden_image.h` constants are additive. irmp/irsnd updated but Hapax version appears newer. Cherry-pick `battery_log.c`. **2026-07-21 review**: only new commit since is PR #11 (`4c77bb86`, macOS build support — CMake toolchain tweaks + `setup_macos.sh`); low value for the Hapax Linux/Windows workflow, not imported. |
| [bedge117/M1](https://github.com/bedge117/M1) | bedge117 | **Active** | `dda12722` | 2026-07-23 20:53 | 2026-07-24 04:12 | C3 enhanced firmware, now C3.125. **2026-07-24 review** (spans C3.13-C3.125, ~30 commits since C3.12): most headline items already present in Hapax (100+ Sub-GHz protocols incl. all newly-named decoders, PicoPass, cc1101 FEC, Field Detector, standard game set, RTC/NTP, BadUSB forced type, custom encryption, choice dialogs). Bad-USB typing speedup (poll-gated per-key timing, removes redundant fixed delays) cherry-picked 2026-07-24 (`m1_csrc/m1_badusb.c`, `tests/test_badusb_typing_speed.c`). Outstanding candidates not yet imported: (1) Recovery FW build variant (`-DRECOVERY=ON`, C3.1.0 line) — stripped-down USB-CDC+RPC-only recovery image; (2) SWD peer-recovery tool ("Tools > Recover Peer") — bit-bang SWD over header pins to reflash a bricked peer M1, depends on (1); (3) external-app GPIO (12 header pins) + 1-Wire (`m1_ow_*`) API for `.m1app` apps — no equivalent in Hapax `m1_app_api.c`/`m1_gpio.h` today; (4) Amiibo master-key re-signing (nfc3d/amiitool AES/HMAC/DRBG stack) — needs license/provenance review; (5) new games (Flappy, Coin Flip, Rock-Paper-Scissors, Tamagotchi) — low-priority additive content. Rejected/deferred: ESP32 "SPI-slave brain" architecture + ESP-NOW peer link (Hapax owns a divergent, actively-maintained ESP32 integration — see `esp32-coprocessor` skill; a blind port would conflict badly); Sub-GHz "full scene engine" (Hapax already has its own, see `subghz-protocols` skill); portrait/90° orientation mode (large cross-cutting UI rewrite, treat as its own scoped feature if wanted — see `ui-scene-architecture` skill); CDC transmit NULL class-data guard (already present in Hapax `usbd_cdc.c`). |
| [sincere360/M1_SiN360](https://github.com/sincere360/M1_SiN360) | sincere360 | **Active** | `786b7c21` | 2026-05-09 03:29 | 2026-05-21 02:14 | v0.9 lineage — LCD settings, IR remote, screen orientation. Hapax version scheme derived from SiN360. v0.9.0.5 NFC Amiibo/Switch HALT fix cherry-picked. v0.9.0.6/0.7 binary SPI WiFi/BLE subsystem (`m1_esp32_cmd`, new `m1_wifi.c`, new `m1_bt.c`, SiN360 ESP32 firmware) integrated 2026-05-01. v0.9.0.8 Google Fast Pair BLE spam cherry-picked 2026-05-21. v0.9.1.0 added BLE HID via CMD_BLE_HID_START/STOP/STATUS/REPORT (0x2E/0x2F/0x61/0x60) — m1_badbt.c updated to detect and use binary SPI path 2026-05-21. v0.9.1.0 m1_apps.c (ELF loader) not integrated — Hapax has superior m1_app_manager.c + m1_elf_loader.c. ble_gatt_discovery() imported as a Bluetooth scene (`BtSceneGattDiscovery`) with `M1_ESP32_CAP_BLE_GATT` capability gate 2026-05-21. lfrfid_protocol_extra.c not needed — all protocols exist as individual files. |
| [dagnazty/M1_T-1000](https://github.com/dagnazty/M1_T-1000) | dagnazty (dag) | **Active** | `1bf83f54` | 2026-07-01 23:21 | 2026-07-21 00:30 | STM32 fork (T-1000) built on top of bedge117/C3.12. Uses dag ESP32 AT firmware (dagnazty/esp32-at-monstatek-m1) with custom AT+M1* commands (AT+M1DEAUTH, AT+M1BEACON, AT+M1KARMA, AT+M1PMKID, AT+M1HSCAP, AT+M1EVILTWIN, AT+M1BLESPAM, AT+M1MONITOR, AT+M1PROBE, AT+M1WIFISTATS, AT+M1DEAUTHALL, AT+M1DEAUTHSTOP, AT+HIDKBINIT, AT+HIDKBSEND, AT+ZIGSNIFF). Protocol: SPI AT text commands (hapaxx11/M1-T-800 uses SPI AT transport matching Hapax's spi_AT_send_recv(); binary RPC layer in main/rpc/ is labeled "phase 1 dual-mode" and not yet the primary path). New STM32 features not in Hapax at time of review: 2.4G Channel Survey. All other features: deauth, beacon spam, karma, PMKID/EAPOL capture, probe sniff, evil portal, AP clone, station scan already in Hapax. Integrated: `wifi_survey_24g()` (2026-06-16), all 14 AT+M1* CAP mappings (Phase 1), T-800 discriminator profile (Phase 5), `wifi_pmkid_at()` PMKID Grab scene (Phase 6, 2026-06-17). See documentation/esp32_firmware.md for the full dag AT command table. **2026-07-21 review** (v0.2.0/v0.2.1): mfkey32/mfc_crypto1/m1_diag already in Hapax; USB-UART bridge (`m1_gpio_uart.c`) and 2048 game (`game_2048.c`) already in Hapax; ESP32 SPI-AT re-init leak fix already present (`esp32_main_deinit` join barrier). **ESP32-C6 idle auto power-off was MISSING** (Hapax `m1_esp32_deinit` leaves EN high) → implemented as `m1_csrc/esp32_idle.c/h` + `m1_esp32_idle_poll()` (2026-07-21). GPIO Pin Map viewer + LF RFID ICFilter/write-from-saved tuning deferred (need hardware validation). |
| [da-pingwing/M1_T-1000_RFID](https://github.com/da-pingwing/M1_T-1000_RFID) | da-pingwing | **Active** | `2d2aa2ca` | 2026-06-24 17:04 | 2026-07-21 00:30 | "Monstatek M1 RFID Patch" (GPL-3.0), patched on top of dag's T-1000. Source of dag's LF RFID diagnosis/fixes (m1_diag reset-cause/write-phase, T5577 write timing, TIM3/TIM5 clock-enable-before-deinit — the latter already in Hapax `lfrfid_read_hw_deinit`). Outstanding candidates (deferred, need hardware validation): TIM5 HW input-capture noise filter to stop read-hang on noisy cards (Hapax has `ICFilter = 0`); write-from-saved HardFault fix (drop forced IRQ flag) + release read lock on read-back timeout; soft-DFU bootloader entry over USB CDC (niche). |
| [rgomez31UAQ/Monstatek-M1_STM32H573VIT6_Firmware](https://github.com/rgomez31UAQ/Monstatek-M1_STM32H573VIT6_Firmware) | rgomez31UAQ | Inactive | `024b4c16` | 2026-02-27 19:25 | 2026-04-02 03:21 | Fork of stock + build doc PR. No custom firmware work. |
| [steveAG/monstatek-m1](https://github.com/steveAG/monstatek-m1) | steveAG | Inactive | `2df97efc` | 2026-02-20 21:22 | 2026-04-02 03:21 | Mirror of stock at time of fork. No custom commits. |
| [fengjuan0/Monstatek-M1](https://github.com/fengjuan0/Monstatek-M1) | fengjuan0 | Inactive | `2df97efc` | 2026-02-24 03:45 | 2026-04-02 03:21 | Mirror of stock at time of fork. No custom commits. |
| [RogueMaster/M1](https://github.com/RogueMaster/M1) | RogueMaster | Inactive | `682e6a06` | 2026-06-15 22:09 | 2026-08-03 02:29 | Branding/promo fork (Discord/Patreon plugs, "RM WUZ HERE" README edits, `.github/FUNDING.yml`) layered on top of bedge117's C3.12 line (`81434fa8` = wholesale copy of bedge117 at C3.12, already fully reviewed under the bedge117 row). Also merged in Monstatek's PR #11 macOS build support (`4c77bb86`, already reviewed under the Monstatek row as low-value) and Monstatek v0.8.0.2 content (IR remote database, `battery_log.c`, `bq27421_golden_image.h` — already tracked as cherry-pick candidates in the Monstatek row). No original code contributions found; nothing new to cherry-pick. |

### How to Update This Table

1. **When to check**: Before starting any task that involves porting features,
   comparing implementations, or auditing upstream changes.  Also check
   periodically (roughly monthly) or when the user asks.
2. **Discover new forks**: Search GitHub for `M1 in:name fork:true monstatek`
   and for forks of related repos (`bedge117/m1-sdk`,
   `bedge117/esp32-at-monstatek-m1`).  Add any new public fork to the table.
3. **Update existing rows**: For each **Active** fork, fetch the latest commit
   SHA and date.  Compare with the "Latest Commit" column.  If the SHA has
   changed, the fork has new work — update the SHA, date, and bump "Last
   Reviewed by Hapax" to the current UTC timestamp after reviewing.
4. **Classify activity**:
   - **Active** — fork has custom commits beyond stock Monstatek and has been
     updated within the last 60 days.
   - **Inactive** — no custom commits, or last update > 60 days ago.
5. **Record cherry-picks**: When a commit or feature is cherry-picked from a
   fork into Hapax, note it in the "Notes" column (e.g. "C3.4 crash fix
   cherry-picked").
6. **Never remove rows** — mark forks as Inactive instead, so agents know
   the fork was already evaluated.
