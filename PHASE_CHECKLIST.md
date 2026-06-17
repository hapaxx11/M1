# Phase Checklist — Dag T-800 / T-1000 Integration

## PR Metadata
- **PR Title**: feat: integrate dag T-800 / T-1000 features (channel survey, AT cap mapping, T-800 discriminator)
- **PR Description**: Ports the 2.4G Channel Survey from dagnazty/M1_T-1000, expands the AT command → capability mapping table to cover all 14 dag T-800 custom AT commands, adds a `M1_ESP32_CAP_PROFILE_DAG_T800` discriminator profile, extracts the pure channel-analysis logic into a host-testable module, and updates documentation.

## Status key
✅ Complete · 🔶 Partial · 🔲 Not started

---

## Phases

### Phase 1 — Expand AT Command → Capability Mapping (T-800)
- **Description**: Add the 14 dag T-800 custom AT commands to `s_at_cmd_cap_map[]` in
  `m1_csrc/m1_esp32_caps.c`, mapping each to the correct `M1_ESP32_CAP_*` bit. Mappings:
  `AT+M1DEAUTH`/`AT+M1DEAUTHALL`/`AT+M1DEAUTHSTOP` → `CAP_DEAUTH`;
  `AT+M1BEACON` → `CAP_BEACON`; `AT+M1KARMA` → `CAP_KARMA`;
  `AT+M1EVILTWIN` → `CAP_PORTAL`; `AT+M1BLESPAM` → `CAP_BLE_ADV`;
  `AT+M1MONITOR` → `CAP_PKTMON`; `AT+M1PROBE` → `CAP_PROBE_FLOOD`;
  `AT+M1PMKID`/`AT+M1HSCAP` → `CAP_PKTMON`;
  `AT+M1WIFISTATS` → `CAP_WIFI_SCAN`;
  `AT+HIDKBINIT`/`AT+HIDKBSEND` → `CAP_BLE_HID`.
  Also update the parallel `k_test_at_cmd_map[]` in `tests/test_esp32_caps.c` and add
  a `test_at_cmd_parse_dag_t800_caps()` test case with a realistic T-800 AT+CMD? response.
- **Status**: ✅ Complete
- **Commit**: `feat: add 14 dag T-800 AT commands to ESP32 cap mapping table`

### Phase 2 — T-800 Binary RPC / AT-Fallback Transport Investigation
- **Description**: Determine whether the dag T-800 ESP32 firmware uses binary SPI framing
  compatible with the existing `M1_CMD_MAGIC` / `m1_cmd_t` packet protocol (Phase 2A), or
  requires a separate AT text transport layer (Phase 2B).
  **Phase 2A (preferred)**: If T-800 uses binary RPC, add a third discriminator branch to
  `m1_esp32_caps_init()` and update `m1_esp32_feature_map.c` entries as needed — no new
  transport needed.
  **Phase 2B (fallback)**: If T-800 uses AT text only, implement a minimal
  `wifi_at_transport.c` module providing `wifi_at_deauth_start/stop()`,
  `wifi_at_beacon_start/stop()`, etc., with a `+CWLAP` response parser that fills
  `ap_list[]` compatible with the binary scan path. The module must be pure-logic
  (FatFS/HAL-free) and fully host-testable.
  Outcome: Document the resolution (2A or 2B) in `documentation/esp32_firmware.md` and
  add unit tests for whichever path is implemented.
  **Note**: This phase requires access to the actual T-800 firmware binary or source to
  confirm the wire protocol. If access is unavailable, defer to a separate PR.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_

### Phase 3 — Audit Remaining T-1000 STM32 Unique Features
- **Description**: Review the dagnazty/M1_T-1000 commit log for any STM32-side features
  not yet imported into Hapax. The 2.4G Channel Survey is already imported. Check for:
  USB fingerprint/field-detect improvements, any additional WiFi scenes, display changes,
  or Sub-GHz additions. Cherry-pick anything relevant; close with "nothing outstanding"
  note in this checklist if no new items are found.
- **Status**: 🔶 Partial (2.4G Channel Survey imported; full audit of T-1000 log pending)
- **Commit**: _(pending)_

### Phase 4 — 2.4G Survey: Extract Pure Logic + Host Tests
- **Description**: Extract the three pure-logic functions from `wifi_survey_24g()` in
  `m1_csrc/m1_wifi.c` into `m1_csrc/wifi_ch_analysis.c/h`:
  - `wifi_ch_count_from_aps(ap_list, count, ch_count[14])` — accumulates per-channel AP counts and tracks strongest RSSI
  - `wifi_ch_find_busiest_best(ch_count[14], &busiest, &best)` — finds busiest/least-congested channels
  - `wifi_ch_bar_height(count, max_count, chart_h)` — proportional bar height with min-1-px clamp
  Update `wifi_survey_24g()` to call these helpers. Add `wifi_ch_analysis.c` to
  `cmake/m1_01/CMakeLists.txt`. Write `tests/test_wifi_ch_analysis.c` with Unity tests
  covering: count accumulation, RSSI tracking, busiest/best edge cases (all-zero, tie,
  single-AP), and bar-height formula with clamp.
- **Status**: ✅ Complete
- **Commit**: `refactor: extract wifi_ch_analysis.c/h from wifi_survey_24g + 14 Unity tests`

### Phase 5 — T-800 Firmware Discriminator Profile
- **Description**: Add `M1_ESP32_CAP_PROFILE_DAG_T800` to `m1_csrc/m1_esp32_caps.h` as
  the OR of all CAP bits confirmed present in a T-800 `AT+CMD?` response (at minimum:
  `CAP_WIFI_JOIN | CAP_DEAUTH | CAP_BEACON | CAP_KARMA | CAP_PORTAL | CAP_BLE_ADV |
  CAP_PKTMON | CAP_PROBE_FLOOD | CAP_BLE_HID`). Update `caps_apply_footprint_estimates()`
  with a T-800 branch (discriminator: presence of `AT+M1DEAUTH` bit, i.e. `CAP_DEAUTH`
  together with `CAP_WIFI_JOIN`). Add a `M1_ESP32_FALLBACK_BSS_T800` / `_HEAP_T800`
  constant pair with estimated values (to be measured from the T-800 firmware; use T-800
  AT firmware defaults of ~280 KB BSS / ~120 KB heap as starting estimate). Add a test in
  `tests/test_esp32_caps.c` asserting the new profile constants and discriminator ordering.
- **Status**: ✅ Complete
- **Commit**: `feat: add M1_ESP32_CAP_PROFILE_DAG_T800 + T-800 fallback constants + 3-way discriminator`

### Phase 6 — T-800 PMKID / HSCAP AT-Path Scene (if Phase 2B lands)
- **Description**: If Phase 2 resolves as Phase 2B (AT text transport), add an AT-path
  branch to the EAPOL sniffer scene for T-800: send `AT+M1PMKID` to start, poll for
  `+M1PMKID:` URCs containing BSSID+PMKID hex, call `pmkid_save_to_sd()`. Gated on
  `M1_ESP32_CAP_PKTMON` being present via AT probe. If Phase 2 resolves as 2A (binary RPC
  compatible), this phase is a no-op — the existing `CMD_PKTMON_START/NEXT/STOP` path
  already handles it.
- **Status**: 🔲 Not started (blocked on Phase 2)
- **Commit**: _(pending)_

### Phase 7 — CI Path Filter Update
- **Description**: After adding `wifi_ch_analysis.c` (Phase 4) and `wifi_at_transport.c`
  (Phase 2B, if applicable), audit `.github/workflows/tests.yml` `paths:` list to ensure
  `m1_csrc/` is already covered (it is). Run the quick audit script from CLAUDE.md to
  confirm no new top-level source directories are referenced by new test targets without a
  CI path filter entry.
- **Status**: ✅ Complete (`m1_csrc/**` already in `paths:` — no new directories needed)
- **Commit**: _(no code change needed; verified during Phase 4)_

### Phase 8 — Documentation and Changelog
- **Description**: Finalize documentation:
  - Update `CLAUDE.md` Public Forks Tracker dag T-1000 row with correct "Last Reviewed"
    timestamp and any new cherry-picks noted.
  - Update `documentation/esp32_firmware.md` T-800 section with the Phase 2 resolution
    (binary RPC or AT text) and Phase 5 discriminator details.
  - Update `README.md` WiFi feature list if new scenes were added.
  - Add/update `.changelog/` fragment for all user-visible changes (Survey already has
    `.changelog/dag-t800-import.added.md`; add fragments for AT mapping, T-800 profile).
  - Remove `PHASE_CHECKLIST.md` and open the PR.
- **Status**: 🔶 Partial (dag doc and CLAUDE.md forks tracker updated in first commit;
  remaining items blocked on Phases 2–6)
- **Commit**: `Remove phase checklist before PR` _(final commit)_
