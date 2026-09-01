# Phase Checklist — ESP-NOW Optional Encryption Wiring

## PR Metadata
- **PR Title**: M1↔M1 peer link over ESP-NOW compatibility layer
- **PR Description**: Wires optional app-layer encryption into Peer Link payload exchange while preserving a clear plaintext fallback if the encrypted session handshake is unavailable.

## Phases

### Phase 1 — Review and plan
- **Description**: Inspect existing crypto envelope, pairing context, live send/receive paths, and host-test coverage.
- **Status**: ✅ Complete
- **Commit**: `Plan optional ESP-NOW encryption`

### Phase 2 — Pure optional crypto session
- **Description**: Add host-tested pure helpers for deriving a stable paired-session key, deciding when to encrypt, sealing outbound app payloads, and opening encrypted/plaintext inbound payloads with fallback.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_

### Phase 3 — Firmware scene wiring
- **Description**: Store session crypto state for the selected peer and route Messages and Remote Trigger payloads through the optional encrypted send/receive helpers.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_

### Phase 4 — Docs, validation, cleanup
- **Description**: Update docs/changelog, run targeted tests, attempt firmware build, remove this checklist, and reply to the PR comment.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_
