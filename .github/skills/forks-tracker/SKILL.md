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
| [Monstatek/M1](https://github.com/Monstatek/M1) | Monstatek | **Active** (upstream) | `4c77bb86` | 2026-06-05 06:30 | 2026-07-21 00:30 | Original upstream, stale (v0.8.0.0/0.8.0.2). Cherry-pick candidate outstanding: `battery_log.c` diagnostic CSV logger. Reviewed 2026-07-21: only new commit is macOS build support ([#11](https://github.com/Monstatek/M1/pull/11)), low value, not imported. |
| [bedge117/M1](https://github.com/bedge117/M1) | bedge117 | **Active** | `41228460` | 2026-08-04 06:45 | 2026-08-09 | C3 enhanced firmware (now C3.164), the most actively-diverged fork and Hapax's primary cherry-pick source. Extensively reviewed across many 2026-07-24→2026-08-09 sessions; most headline C3 features (100+ Sub-GHz protocols, PicoPass, RPC file ops, offensive WiFi/BLE suite, external-app GPIO) are already present or superseded in Hapax. Cherry-picked fixes/features (see git history for per-commit detail, not reproduced here): bad-USB typing speedup; logdb/SDMMC/mutex/I2C stability fixes; self-flash EXTI-masking brick-risk fix; HW TRNG driver (`m1_rng.c`); FatFs reentrancy; LF-RFID UID-copy overflow fix; Amiibo master-key re-signing (MIT/public-domain sources, license documented in `README_License.md` §7); new games (Flappy, Coin Flip, RPS); Sub-GHz RAW replay full-waveform fix; USB-MSC/CDC/watchdog/field-detector stability hardening (bounded spins, TX abort, USB session-epoch NACK); `rpc_task`/SD-detection watchdog + self-heal. Deferred/rejected (do not re-propose without new information): Recovery FW build variant (`-DRECOVERY=ON`) — explicit owner decision, not a technical blocker; WiFi Hotspot NAT passthrough and BLE Direct (NUS) transport — architecturally complex, ESP32-firmware-dependent; ESP32 "SPI-slave brain" + ESP-NOW peer link and `game_peer_ttt` — Hapax owns a divergent ESP32 integration (see `esp32-coprocessor` skill), blind port would conflict; `game_tamagotchi` — RAM footprint exceeds M1 budget; Sub-GHz "full scene engine" — Hapax already has its own (see `subghz-protocols` skill); portrait/90° orientation mode — large cross-cutting UI rewrite, treat as its own scoped feature if wanted; 1-Wire API — no source exists in either fork to port; SWD peer-recovery tool — concept only, no code in either fork. |
| [sincere360/M1_SiN360](https://github.com/sincere360/M1_SiN360) | sincere360 | **Active** | `786b7c21` | 2026-05-09 03:29 | 2026-05-21 02:14 | v0.9 lineage — LCD settings, IR remote, screen orientation; Hapax's version scheme is derived from SiN360. Cherry-picked: NFC Amiibo/Switch HALT fix; binary SPI WiFi/BLE subsystem (`m1_wifi.c`, `m1_bt.c`); Google Fast Pair BLE spam; BLE HID (`m1_badbt.c`); `ble_gatt_discovery()` as `BtSceneGattDiscovery`. Not integrated: `m1_apps.c` ELF loader (Hapax's `m1_app_manager.c`/`m1_elf_loader.c` is superior); `lfrfid_protocol_extra.c` (not needed, Hapax has individual protocol files). |
| [dagnazty/M1_T-1000](https://github.com/dagnazty/M1_T-1000) | dagnazty (dag) | **Active** | `268d8ca1` | 2026-08-18 09:49 | 2026-09-01 03:14 | STM32 fork (T-1000) on bedge117/C3.12, using dag's ESP32 AT firmware (custom AT+M1* offensive-WiFi/BLE/Zigbee commands, see `documentation/esp32_firmware.md` for the AT table). Most dag features already in Hapax; cherry-picked: `wifi_survey_24g()`, AT+M1* CAP mappings, PMKID Grab scene, ESP32-C6 idle auto power-off (`esp32_idle.c/h`). Outstanding cherry-pick candidates from contributor romulofer (reviewed 2026-09-01, not yet ported): ~~**Sub-GHz Send Once/Repeat** toggle on the replay screen (PR#2, `a1f489c7`)~~ — reviewed 2026-09-03: already implemented (and superseded) by Hapax's Read Raw `raw_tx_repeat_mode` toggle + hold-to-repeat (`m1_subghz_scene_read_raw.c`, `m1_subghz_read_raw_state.h`); do not re-propose. **Live RSSI bar** on the Sub-GHz record screen (4 tasks, ~400 lines with host tests) — Phase 2 candidate, bench-gate the periodic-refresh/pre-scan tasks on hardware; **IR Custom Remotes** — implemented in Hapax (`m1_ir_custom.c/h` + `ir_cust_name.c/h`): on-device IR learning with RAW fallback, remote/button rename/delete, and filename sanitization. The 6 `ir_database/` files were already present in Hapax. Deferred pending audit: **Sub-GHz record once-per-boot leak** fix (dag v0.3.0) — needs confirmation Hapax's record-scene exit paths don't already free the ring buffers before porting. Rejected: **M1↔M1 peer link** (`m1_link.c/h`, ~2600 lines, 915 MHz FSK peer messaging) — entangled with dag's divergent AT+M1LINK protocol; would require designing Hapax's own peer-link layer, treat as an independent feature if wanted. |
| [da-pingwing/M1_T-1000_RFID](https://github.com/da-pingwing/M1_T-1000_RFID) | da-pingwing | **Active** | `2d2aa2ca` | 2026-06-24 17:04 | 2026-07-21 00:30 | "Monstatek M1 RFID Patch" (GPL-3.0), patched on top of dag's T-1000; source of dag's LF RFID diagnosis/fixes (some already ported, e.g. `lfrfid_read_hw_deinit` clock-enable-before-deinit). Outstanding candidates (deferred, need hardware validation): TIM5 HW input-capture noise filter for read-hang on noisy cards; write-from-saved HardFault fix + read-back-timeout lock release; soft-DFU bootloader entry over USB CDC (niche). |
| [Nipahc/NipTek-M1](https://github.com/Nipahc/NipTek-M1) | Nipahc | **Active** | `cb6f6c42` | 2026-08-28 17:54 | 2026-09-02 01:36 | "NipTek M1" flavor built directly on stock Monstatek v0.8.0.2 (not on the bedge117/Hapax line). Cherry-picked: main-menu battery indicator (`draw_main_menu_battery()`). Not applicable: branding changes (fork-specific). Superseded: "Specter" passive NFC field detector — Hapax's `m1_field_detect.c` already covers this (and LF RFID ADC detection) as a superset. Not actionable: Amiibo-Flipper-format load-rejection investigation — Hapax's own amiibo re-signing loader already accepts Flipper-format NFC dumps. |
| [rgomez31UAQ/Monstatek-M1_STM32H573VIT6_Firmware](https://github.com/rgomez31UAQ/Monstatek-M1_STM32H573VIT6_Firmware) | rgomez31UAQ | Inactive | `024b4c16` | 2026-02-27 19:25 | 2026-04-02 03:21 | Fork of stock + build doc PR. No custom firmware work. |
| [steveAG/monstatek-m1](https://github.com/steveAG/monstatek-m1) | steveAG | Inactive | `2df97efc` | 2026-02-20 21:22 | 2026-04-02 03:21 | Mirror of stock at time of fork. No custom commits. |
| [fengjuan0/Monstatek-M1](https://github.com/fengjuan0/Monstatek-M1) | fengjuan0 | Inactive | `2df97efc` | 2026-02-24 03:45 | 2026-04-02 03:21 | Mirror of stock at time of fork. No custom commits. |
| [RogueMaster/M1](https://github.com/RogueMaster/M1) | RogueMaster | Inactive | `682e6a06` | 2026-06-15 22:09 | 2026-08-03 02:29 | Branding/promo fork layered on bedge117's C3.12 line (`81434fa8`, already reviewed under the bedge117 row) plus Monstatek's macOS build support and v0.8.0.2 content (already tracked under the Monstatek row). No original code contributions found; nothing new to cherry-pick. |

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
7. **Keep Notes concise — this is a tracker, not a changelog.** Once a
   cherry-pick has landed (or an item has been permanently rejected/deferred),
   collapse its description to a short clause naming the feature/fix and, if
   useful, the source file — do not keep the session-by-session narrative
   (commit SHAs, line-by-line rationale, per-file diffs). That level of detail
   already lives in git history and PR descriptions; re-deriving it here just
   to summarize it again is wasted effort. A Notes cell should distinguish
   only two things an agent actually needs: (a) a one-line summary of what's
   already been cherry-picked/rejected, and (b) any *currently outstanding*
   candidates still worth a closer look, described briefly enough to decide
   whether to investigate further. If a Notes cell exceeds a short paragraph,
   condense it as part of that update instead of appending another session's
   worth of detail on top.
