# Phase Checklist — Restaurant Pager

## PR Metadata
- **PR Title**: Add a native restaurant pager app
- **PR Description**: Add a native Sub-GHz restaurant pager workflow with protocol-specific decoding/editing, saved stations, and replay, following M1 hardware and storage constraints.

## Phases

### Phase 1 — Protocol logic
- **Description**: Add host-testable decode, encode/edit, and format-detection logic for supported restaurant pager encodings.
- **Status**: 🔄 In progress
- **Commit**: _(pending)_

### Phase 2 — Receiver and replay scene
- **Description**: Add the native scene for live capture, history, field editing, and asynchronous transmission using the built-in SI4463.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_

### Phase 3 — Saved stations and categories
- **Description**: Add bounded station persistence, named categories, saved-signal actions, and scan configuration supported by M1.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_

### Phase 4 — Documentation and validation
- **Description**: Update the feature documentation and changelog, run host tests and the firmware build, inspect RAM usage, and remove this temporary checklist.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_
