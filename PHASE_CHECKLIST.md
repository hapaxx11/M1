# Phase Checklist — NFC and RFID Momentum Tools

## PR Metadata
- **PR Title**: Add NFC authoring and LF-RFID raw tools
- **PR Description**: Add a multi-record NFC NDEF authoring workflow and LF-RFID raw capture/emulation support, with tested pure logic and safe resource handling.

## Phases

### Phase 1 — NFC NDEF authoring
- **Description**: Extend the existing NFC URL writer into a guided authoring flow for URI, text, phone, and Wi-Fi records using the existing NDEF encoders.
- **Status**: ✅ Complete
- **Commit**: `Implement NFC NDEF authoring workflow`

### Phase 2 — T5577 password clearing
- **Description**: Add a utility to clear a T5577 password when the caller supplies the current password, with tested input validation and careful write sequencing.
- **Status**: ✅ Complete
- **Commit**: `Add T5577 password clearing tool`

### Phase 3 — Validation and cleanup
- **Description**: Run host tests and firmware build, inspect RAM usage, request code review, and remove this temporary checklist.
- **Status**: 🔄 In progress
- **Commit**: _(pending)_
