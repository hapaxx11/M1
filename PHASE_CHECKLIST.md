# Phase Checklist — PR #822 Review Fixes

## PR Metadata
- **PR Title**: Complete portable ProtoPirate app features and automotive decoders
- **PR Description**: Address all actionable comments in review 5617614217 with minimal code and regression-test changes.

## Phases

### Phase 1 — Decoder capture paths
- **Description**: Fix Ford V2 sync bits and sync jitter handling; admit short Honda Static/Ford V1 pulses in live and offline capture paths with pipeline tests.
- **Status**: ✅ Complete
- **Commit**: `Fix automotive decoder capture paths`

### Phase 2 — Replay and analyzer state
- **Description**: Fix V3/V4 rolling-counter advancement, Remote Analyzer AM state retention, and explicit model preset hopping behavior.
- **Status**: 🔄 In progress
- **Commit**: _(pending)_

### Phase 3 — Saved capture matching and updates
- **Description**: Fix static/rolling identity matching and nested search; make updates failure-safe and preserve recovered capture metadata.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_

### Phase 4 — Validation and final cleanup
- **Description**: Run focused host tests and firmware build/RAM check when available, review all changes, remove this checklist, and reply to review threads.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_
