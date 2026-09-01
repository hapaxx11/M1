# Phase Checklist — ESP-NOW Remote Trigger

## PR Metadata
- **PR Title**: M1↔M1 peer link over ESP-NOW compatibility layer
- **PR Description**: Wires the existing ESP-NOW remote-trigger protocol into the Peer Link UI with explicit sender selection, receiver consent, guarded saved-capture replay, tests, and docs.

## Phases

### Phase 1 — Discovery and plan
- **Description**: Inspect the existing trigger FSM, Peer Link scene manager, saved-capture replay paths, and safety/UX constraints.
- **Status**: ✅ Complete
- **Commit**: `Constrain remote trigger routing`

### Phase 2 — Pure helper and tests
- **Description**: Add host-tested pure routing helpers for trigger-kind replay decisions and action labeling/path handling.
- **Status**: ✅ Complete
- **Commit**: `Constrain remote trigger routing`

### Phase 3 — Sender/receiver scene wiring
- **Description**: Add Peer Link Remote Trigger scenes for selecting a saved capture, sending a request to the paired peer, polling status/result frames, and consenting to inbound requests.
- **Status**: ✅ Complete
- **Commit**: `Wire remote trigger scenes`

### Phase 4 — Replay execution adapters
- **Description**: Execute accepted trigger requests through the existing module replay/emulate functions with danger gating and hardware cleanup.
- **Status**: ✅ Complete
- **Commit**: `Wire remote trigger scenes`

### Phase 5 — Docs, validation, cleanup
- **Description**: Update directly related docs/changelog, run targeted tests, attempt firmware build, remove this checklist, and reply to the PR comment.
- **Status**: ✅ Complete
- **Commit**: `Document remote trigger wiring`
