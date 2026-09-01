# Phase Checklist — M1↔M1 Peer Link over ESP-NOW

## PR Metadata
- **PR Title**: M1↔M1 peer link over ESP-NOW — Phases 0–5 pure-logic implementation
- **PR Description**: Implements the phased peer-link plan on Hapax's ESP-NOW
  compat layer. Adds host-tested pure-logic modules for payload chunking,
  capability decision, short messaging, danger-gated remote trigger, and
  app-layer authenticated encryption, plus the sender-side capture-sharing
  helper. Hardware scene wiring and `m1-esp32-brain` firmware changes are gated
  behind `M1_ESP32_CAP_ESPNOW` and called out as out-of-scope for STM32 commits.

## Phases

### Phase 0 — Enablement (capability decision + payload chunking)
- **Description**: `espnow_appmsg.h` unified app-type registry (collision-free
  type ranges). `espnow_chunk.c/h` pure splitter/reassembler so payloads > 42
  bytes ride multiple NOW_SEND/NOW_RECV_GET calls. `espnow_caps_decide.h` pure
  capability-availability decision. Host tests for all.
- **Status**: ✅ Complete
- **Commit**: `espnow: Phase 0 — payload chunking + app-type registry + cap decision`

### Phase 1 — Finish capture sharing (sender side)
- **Description**: `espnow_shareable.c/h` pure helper: which saved-item
  extensions are shareable, basename extraction, receive-path builder. Host
  tests. (File-browser scene wiring is bench-gated.)
- **Status**: ✅ Complete
- **Commit**: `espnow: Phase 1 — sender-side capture-sharing helper`

### Phase 2 — Peer messaging (short text)
- **Description**: `espnow_message.c/h` pure compose/frame/parse + inbox ring
  buffer over the DATA channel (app type 0x20). Host tests.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_

### Phase 3 — Remote trigger (danger-gated)
- **Description**: `espnow_trigger.c/h` pure request/consent/execute/result FSM
  with capture-name validation and explicit consent gating (app type 0x30).
  Host tests.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_

### Phase 4 — Authenticated encryption (app-layer AES)
- **Description**: `espnow_crypto.c/h` pure AEAD wrapper over the existing
  `m1_crypto` AES-256-CBC with an auth tag + nonce, keyed off the pairing
  confirm secret. Host tests (round-trip, tamper detection, wrong key).
- **Status**: 🔲 Not started
- **Commit**: _(pending)_

### Phase 5 — Polish, docs, changelog
- **Description**: Update `documentation/esp32_firmware.md` ESP-NOW section and
  the peer-link plan doc; add `.changelog/*.added.md` fragments; update the
  esp32-coprocessor skill if needed. Remove this checklist before PR.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_
