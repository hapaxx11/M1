# Phase Checklist — POCSAG Receiver

## PR Metadata
- **PR Title**: Add a Sub-GHz POCSAG receiver app
- **PR Description**: Add an M1-native POCSAG receiver inspired by Momentum's pager app, including a tested streaming decoder, live message history and detail views, and safe radio lifecycle handling.

## Phases

### Phase 1 — Decoder and tests
- **Description**: Add a host-testable POCSAG bitstream decoder and bounded message-history logic with tests for frame sync, baud detection, codeword decoding, and message text.
- **Status**: 🔄 In progress
- **Commit**: _(pending)_

### Phase 2 — Receiver scene
- **Description**: Integrate the decoder into a non-blocking Sub-GHz receiver scene with message list/detail views, configuration access, menu routing, and radio cleanup.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_

### Phase 3 — Documentation and validation
- **Description**: Update user-facing documentation and changelog, run host tests and firmware build, verify RAM usage, request code review, and remove this checklist.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_
