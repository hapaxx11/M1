# Phase Checklist — PR #800 Review Feedback

## PR Metadata
- **PR Title**: Fix NFC/RFID write safety and BLE scan responsiveness
- **PR Description**: Fix T5577 password clearing, validate Type 2 tag capacity before NDEF writes, allow explicit open Wi-Fi NDEF records, and make continuous BLE tracking scans asynchronous and cancellable.

## Phases

### Phase 1 — RFID correctness
- **Description**: Correct the T5577 password-clear command sequence and prevent overflow in the password input buffer. Add host-side regression coverage.
- **Status**: ✅ Complete
- **Commit**: `Fix T5577 password clear sequence`

### Phase 2 — NFC authoring safety
- **Description**: Reject NDEF records exceeding the Type 2 tag's advertised user memory before any write, and make open versus secured Wi-Fi submission explicit. Add host-side coverage.
- **Status**: ✅ Complete
- **Commit**: `Reject NDEF writes beyond tag capacity`

### Phase 3 — BLE scan responsiveness
- **Description**: Move continuous BLE signal tracking and AirTag monitoring into scene-driven asynchronous scans that process BACK cancellation without blocking the UI task.
- **Status**: 🔄 In progress
- **Commit**: _(pending)_

### Phase 4 — Validation and cleanup
- **Description**: Run targeted host tests and firmware build, review RAM usage and final diff, and remove this temporary checklist.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_
