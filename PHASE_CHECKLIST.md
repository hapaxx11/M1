# Phase Checklist — Restaurant Pager

## PR Metadata
- **PR Title**: Add a native restaurant pager app
- **PR Description**: Add a native Sub-GHz restaurant pager workflow with protocol-specific decoding/editing, named station categories, configurable replay, and saved-signal actions, following M1 hardware and storage constraints.

## Phases

### Phase 1 — Protocol logic
- **Description**: Add host-testable decode, encode/edit, and format-detection logic for supported restaurant pager encodings.
- **Status**: ✅ Complete
- **Commit**: `Add restaurant pager protocol logic`

### Phase 2 — Receiver and replay scene
- **Description**: Add the native scene for live capture, history, field editing, and asynchronous transmission using the built-in SI4463.
- **Status**: ✅ Complete
- **Commit**: `Add restaurant pager capture and replay scene`

### Phase 3 — Saved stations and categories
- **Description**: Add named-category station saves, saved-signal browsing, confirmed batch transmission, and persistent pager/repeat settings.
- **Status**: ✅ Complete
- **Commit**: `Add restaurant pager saved stations and settings`

### Phase 4 — Documentation and validation
- **Description**: Update the feature documentation and changelog, run host tests and the firmware build, inspect RAM usage, and remove this temporary checklist.
- **Status**: 🔄 In progress
- **Commit**: _(pending)_
