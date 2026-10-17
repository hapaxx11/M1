# Phase Checklist — PR #809 review feedback

## PR Metadata
- **PR Title**: Implement transport and network features
- **PR Description**: Address all findings from the PR #809 code review with focused regression coverage.

## Phases

### Phase 1 — Payload and reset regressions
- **Description**: Correct SSID payload test expectations and cover USB reset cleanup for mobile ingress.
- **Status**: ✅ Complete
- **Commit**: `Fix USB epoch parser cleanup`

### Phase 2 — RPC response and upload isolation
- **Description**: Return explicit oversized-reply errors, track upload transport ownership, and avoid holding reply routing across deferred handlers.
- **Status**: ✅ Complete
- **Commit**: `Isolate deferred RPC replies by transport`

### Phase 3 — ESP32 lifecycle and SPI serialization
- **Description**: Keep required background networking owners alive, serialize M1 Link transactions independently of the legacy AT task, and coordinate teardown.
- **Status**: 🔄 In progress
- **Commit**: _(pending)_

### Phase 4 — Background polling fairness
- **Description**: Prioritize deferred RPC work and back off failed status polls.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_

### Phase 5 — Validation and cleanup
- **Description**: Verify the existing I2C fix, run targeted tests/build, remove this checklist, and review the final diff.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_

## Supplemental Task — TPMS sensor activation

### Phase 1 — LF activation backend
- **Description**: Add worker-serialized start/stop events and a safe 125 kHz field lifecycle.
- **Status**: ✅ Complete
- **Commit**: `Add worker-owned TPMS activation field`

### Phase 2 — TPMS scene integration and tests
- **Description**: Add receiver-scene activation controls, bounded timing, and host-side coverage.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_

### Phase 3 — Documentation and validation
- **Description**: Add a changelog fragment, run tests and the firmware build/RAM check, then remove this supplemental checklist section.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_
