# Phase Checklist — Proto Pirate / Sub-GHz Config filtering

## PR Metadata
- **PR Title**: Filter Proto Pirate and Sub-GHz Read config choices by registry capability
- **PR Description**: Limit frequency and modulation selections in the shared Sub-GHz Config scene to combinations actually supported by the active protocol scope: Proto Pirate registry only in the Proto Pirate receiver path, the full Sub-GHz registry in regular Read, and no filtering in Read Raw.

## Phases

### Phase 1 — Add pure-logic filtering API to registry and frequency presets
- **Description**: Add helpers to compute allowed modulation bitmask and allowed frequency-preset-index bitmask from a registry subset and a selected modulation. Add frequency-preset lookup helpers if missing.
- **Status**: ✅ Complete
- **Commit**: `subghz: add registry/frequency filtering helpers for config scene`

### Phase 2 — Add config filter mode to SubGhzApp and wire parent scenes
- **Description**: Define `SubGhzConfigFilterMode` enum; add `config_filter_mode` field to `SubGhzApp`. Set it to Proto Pirate/full/none in the appropriate parent scenes before pushing Config.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_

### Phase 3 — Restrict Config scene frequency/modulation cycling
- **Description**: Update `m1_subghz_scene_config.c` to respect the filter mode when cycling modulation and frequency, and to hide/disable hopping when the current filter excludes hopper frequencies.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_

### Phase 4 — Host-side regression tests
- **Description**: Extend registry tests to verify Proto Pirate mask (OOK only, 433.92 MHz only), full-registry mask includes FSK, and frequency mask changes with modulation. Add tests for the new pure-logic helpers.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_

### Phase 5 — Firmware build + RAM check
- **Description**: Build firmware, confirm linker RAM summary is acceptable, and run host tests.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_

### Phase 6 — Changelog fragment
- **Description**: Add `.changelog/` fragment describing the change.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_
