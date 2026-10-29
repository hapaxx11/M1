# Phase Checklist — Meshtastic Review Fixes

## PR Metadata
- **PR Title**: Fix Meshtastic review findings
- **PR Description**: Address all actionable findings in the PR #833 review thread, including channel behavior, UART ownership, async servicing, persistence, and licensing.

## Phases

### Phase 1 — Protocol and app behavior
- **Description**: Fix channel routing/decoding, secure PSK generation, SNR display, keyboard text input, settings directory creation, and nanopb notices.
- **Status**: ✅ Complete
- **Commit**: `Fix Meshtastic protocol review findings`

### Phase 2 — UART ownership lifecycle
- **Description**: Make bridge handoff, forced app cleanup, and CDC line-coding restoration reliable.
- **Status**: ✅ Complete
- **Commit**: `Harden Meshtastic UART ownership lifecycle`

### Phase 3 — Non-blocking app servicing
- **Description**: Keep UART RX and heartbeat service active during the keyboard and notifications without blocking alerts.
- **Status**: ✅ Complete
- **Commit**: `Service Meshtastic app without blocking`

### Phase 4 — Regression checks and cleanup
- **Description**: Add/execute targeted regression coverage, build checks, review the final diff, and remove this checklist.
- **Status**: 🔄 In progress
- **Commit**: _(pending)_
