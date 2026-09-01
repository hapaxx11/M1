# Phase Checklist — ESP-NOW Capture Share Polish

## PR Metadata
- **PR Title**: M1↔M1 peer link over ESP-NOW compatibility layer
- **PR Description**: Polishes ESP-NOW capture sharing by adding Hapax-standard saved-item send shortcuts and validation/UX updates on top of the existing Peer Link sender flow.

## Phases

### Phase 1 — Saved-action discovery
- **Description**: Inspect saved Sub-GHz/NFC/RFID/IR action flows and decide the smallest shared send-to-peer hook that preserves Hapax UX standards.
- **Status**: ✅ Complete
- **Commit**: `Polish capture sharing shortcuts`

### Phase 2 — Shared send hook
- **Description**: Extract the capture-send orchestration into a reusable firmware helper callable from Peer Link and saved-item action menus.
- **Status**: ✅ Complete
- **Commit**: `Polish capture sharing shortcuts`

### Phase 3 — Saved-item shortcuts
- **Description**: Add Send to Peer entries to saved capture action menus without adding Back items or hardcoded list geometry.
- **Status**: ✅ Complete
- **Commit**: `Polish capture sharing shortcuts`

### Phase 4 — Docs and validation
- **Description**: Update directly related docs/changelog, run targeted host tests, attempt firmware build, remove this checklist, and reply to the PR comment.
- **Status**: 🔄 In progress
- **Commit**: _(pending)_
