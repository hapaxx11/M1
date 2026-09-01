# Phase Checklist — ESP-NOW Long Message Chunking

## PR Metadata
- **PR Title**: M1↔M1 peer link over ESP-NOW compatibility layer
- **PR Description**: Extends Peer Link Messages to use STM32-side app-layer chunking for full-length text messages while keeping short plaintext interop and user hinting for other M1 firmware.

## Phases

### Phase 1 — Review and plan
- **Description**: Inspect current Messages UI cap, secure-link send/receive wrapper, chunk helper, and host-test coverage.
- **Status**: ✅ Complete
- **Commit**: `Plan ESP-NOW long messages`

### Phase 2 — Chunked plaintext transport
- **Description**: Teach the optional secure-link wrapper to fragment/reassemble plaintext app payloads when they exceed the direct 42-byte send budget, without requiring ESP32 firmware changes.
- **Status**: ✅ Complete
- **Commit**: `Support chunked ESP-NOW plaintext`

### Phase 3 — Messages UI polish
- **Description**: Raise the Messages composer to the full protocol text length and add concise on-screen hint/status text for chunked Hapax messages versus short compatibility messages.
- **Status**: ✅ Complete
- **Commit**: `Enable long ESP-NOW messages`

### Phase 4 — Docs, validation, cleanup
- **Description**: Update related docs/changelog, run targeted tests, attempt firmware build, remove this checklist, and reply to the PR comment.
- **Status**: 🔄 In progress
- **Commit**: _(pending)_
