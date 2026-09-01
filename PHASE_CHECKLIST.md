# Phase Checklist — ESP-NOW Peer Messaging and Capture Sharing UI

## PR Metadata
- **PR Title**: M1↔M1 peer link over ESP-NOW compatibility layer
- **PR Description**: Implements STM32-side ESP-NOW peer-link foundations plus user-facing Peer Link UI for peer messaging and capture sharing, with host-tested pure logic and documentation updates.

## Phases

### Phase 1 — Discovery and UI design
- **Description**: Inspect existing Hapax ESP-NOW scene architecture, Dag peer-link UI reference points, and decide the smallest Hapax-standard scene additions for Messages and Send Capture.
- **Status**: ✅ Complete
- **Commit**: _(pending)_

### Phase 2 — Pure helper coverage
- **Description**: Add host-testable pure helper coverage for any new UI decisions such as transport frame budget and shareable sender sizing.
- **Status**: ✅ Complete
- **Commit**: _(pending)_

### Phase 3 — Peer messaging UI
- **Description**: Add a Peer Link Messages scene that sends VKB-composed short messages and polls/queues inbound messages using existing `espnow_message` and `m1_espnow_hal` foundations.
- **Status**: ✅ Complete
- **Commit**: _(pending)_

### Phase 4 — Capture sharing UI
- **Description**: Add a Hapax-standard Send Capture browser/category flow that can select saved Sub-GHz/NFC/RFID/IR files and drive the existing ESP-NOW file-transfer sender path.
- **Status**: ✅ Complete
- **Commit**: _(pending)_

### Phase 5 — Docs, validation, cleanup
- **Description**: Update directly related documentation/changelog, run targeted host tests, attempt firmware build validation, remove `PHASE_CHECKLIST.md`, and reply to the PR comment.
- **Status**: 🔄 In progress
- **Commit**: _(pending)_
