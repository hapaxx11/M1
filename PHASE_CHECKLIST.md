# Phase Checklist — Import dag T-1000/T-800 features

## PR Metadata
- **PR Title**: feat: import dag 2.4G Channel Survey and dag firmware documentation
- **PR Description**: Ports the 2.4G Channel Survey tool from dagnazty/M1_T-1000 to Hapax using the SiN360 binary SPI scan infrastructure. Adds a per-channel bar chart display showing busiest/recommended channels and strongest RSSI. Also extracts the pure channel-analysis logic into a host-testable module and documents the full dag ESP32 AT firmware command table.

## Phases

### Phase 1 — Core feature import + documentation
- **Description**: Port `wifi_survey_24g()` from dag T-1000 into `m1_wifi.c`; add it to the WiFi scene menu between Station Scan and MAC Track; update CLAUDE.md Public Forks Tracker with dag T-1000 entry; add dag AT firmware command table to `documentation/esp32_firmware.md`; add changelog fragment.
- **Status**: ✅ Complete
- **Commit**: `feat: add 2.4G Channel Survey and dag T-800/T-1000 documentation`

### Phase 2 — Extract pure channel-analysis logic + host tests
- **Description**: Extract the channel-counting, busiest/best-channel finder, and bar-height calculation into `m1_csrc/wifi_ch_analysis.c/h`; update `wifi_survey_24g()` to call the extracted helpers; add the new source file to `cmake/m1_01/CMakeLists.txt`; write `tests/test_wifi_ch_analysis.c` with Unity tests covering count accumulation, busiest/best selection edge cases, and bar-height clamping.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_

### Phase 3 — Finalize and open PR
- **Description**: Remove `PHASE_CHECKLIST.md` from the branch; open the pull request using the PR metadata above.
- **Status**: 🔲 Not started
- **Commit**: `Remove phase checklist before PR`
