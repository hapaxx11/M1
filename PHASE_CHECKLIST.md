# Phase Checklist — SubGHz Flipper/Momentum Parity Improvements

## PR Metadata
- **PR Title**: feat: SubGHz parity — Acurite 5n1, playlist delays, larger RAW buffer, configurable RSSI, custom freq, SubGHz Remote
- **PR Description**: Six SubGHz improvements to close Flipper/Momentum compatibility gaps: Acurite 5n1 weather decoder, playlist `# delay:` support, 8192-sample RAW decode buffer, configurable hopper RSSI threshold, custom frequency entry via VKB, and a new SubGHz Remote scene for multi-button RF remote control.

## Phases

### Phase 1 — Acurite 5n1 decoder
- **Description**: Port Acurite 5n1 weather station decoder (64-bit OOK PWM, 433.92 MHz). Add enum, decoder .c, registry entry, CMakeLists, and test.
- **Status**: ✅ Complete
- **Commit**: `feat: add Acurite 5n1 weather station decoder`

### Phase 2 — Playlist delay support
- **Description**: Parse `# delay: <ms>` lines in playlist .txt files. Apply vTaskDelay() between signals. Update SubGhzApp struct, playlist parser, and scene. Add tests for delay parsing.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_

### Phase 3 — Raw offline-decode buffer increase
- **Description**: Bump FLIPPER_SUBGHZ_RAW_MAX_SAMPLES from 2048 to 8192 in flipper_subghz.h. Update test that checks the limit.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_

### Phase 4 — Configurable RSSI threshold
- **Description**: Add rssi_threshold to subghz_cfg struct. Add getter/setter accessors. Add config row to Config scene. Persist in settings.cfg.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_

### Phase 5 — Custom frequency entry
- **Description**: Add 63rd "Custom" preset. When selected+OK, invoke VKB for MHz input. Store and persist user custom frequency. Handle in subghz_apply_config().
- **Status**: 🔲 Not started
- **Commit**: _(pending)_

### Phase 6 — SubGHz Remote scene
- **Description**: New SubGHz Remote scene. Manifest .rem format (5 button assignments). Add to Sub-GHz menu. Implement on_enter/on_event/draw/on_exit.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_

### Phase 7 — Changelog fragment
- **Description**: Add .changelog fragments for all 6 features.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_
