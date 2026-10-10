# Phase Checklist — BLE Monitoring and Detectors

## PR Metadata
- **PR Title**: Implement BLE monitoring and detector tools
- **PR Description**: Add passive AirTag monitoring and heuristic BLE detectors for skimmer modules, Flock devices, and Ray-Ban Meta advertisements. Include host-tested signature parsing, BLE capability gates, and clear confidence wording.

## Phases

### Phase 1 — Pure BLE signature rules
- **Description**: Extract advertisement parsing and name-based candidate matching into host-testable logic with boundary tests.
- **Status**: ✅ Complete
- **Commit**: `Add host-tested BLE detector signatures`

### Phase 2 — Monitors and detector scenes
- **Description**: Implement raw-ad AirTag/Meta scans and name-based detector results; gate scan scenes on BLE capability.
- **Status**: 🔄 In progress
- **Commit**: _(pending)_

### Phase 3 — Validation and cleanup
- **Description**: Add changelog fragment, run host tests and firmware build, inspect RAM budget, review diff, and remove this checklist.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_
