# Phase Checklist — Meshtastic review fixes

## PR Metadata
- **PR Title**: Add Meshtastic external app
- **PR Description**: Bundle a UART Meshtastic client with Hapax UI, firmware UART lifecycle integration, host tests, and release packaging.

## Phases

### Phase 1 — UART ownership and DMA handoff
- **Description**: Correct USART bridge task-handle ownership, acknowledge both bridge directions before UART reconfiguration, and add regression tests.
- **Status**: ✅ Complete
- **Commit**: `Fix Meshtastic UART bridge handoff`

### Phase 2 — Fragment-based changelog
- **Description**: Restore the Meshtastic entry to a changelog fragment and let the normal workflow stamp version 0.9.5.1.
- **Status**: ✅ Complete
- **Commit**: `Restore Meshtastic changelog fragment`

### Phase 3 — Verification and cleanup
- **Description**: Run host tests, firmware build, RAM and app-symbol checks, then remove this checklist before pushing the PR update.
- **Status**: 🔲 Not started
- **Commit**: `Remove Meshtastic review phase checklist`
