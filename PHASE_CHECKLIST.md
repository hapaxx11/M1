# Phase Checklist — Weather Station and TPMS Apps

## PR Metadata
- **PR Title**: Add Momentum-style Weather Station and TPMS apps
- **PR Description**: Convert the Weather Station to the event-driven scene architecture and add a dedicated TPMS receiver with bounded history, configuration navigation, and host-side regression coverage.

## Phases

### Phase 1 — Scene-based Weather Station
- **Description**: Move weather reception, navigation, and rendering into a non-blocking scene while preserving the existing dual-modulation scan and sensor history.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_

### Phase 2 — TPMS receiver app
- **Description**: Add TPMS-scoped decoding, a bounded reception history, a scene with detail/config navigation, and host-side tests.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_

### Phase 3 — Integration and validation
- **Description**: Register firmware and host-test sources, update menu and user-facing docs/changelog, then run host tests and firmware build/RAM checks.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_
