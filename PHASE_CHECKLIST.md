# Phase Checklist — Sub-GHz final review follow-up

## PR Metadata
- **PR Title**: Improve Sub-GHz modem and capture stability
- **PR Description**: Correct the remaining review findings in raw SD persistence, MSK100K modem configuration, Smart ID input handling, guided-hop queue isolation, and HIL cleanup/assertions.

## Phases

### Phase 1 — Capture and input correctness
- **Description**: Make raw SD writes transactional with bounded retries; drain stale guided-hop RX events; defer Smart ID Read until short-press release; add regressions.
- **Status**: ✅ Complete
- **Commit**: `Fix Sub-GHz capture and input review findings`

### Phase 2 — MSK100K WDS profile correction
- **Description**: Regenerate MSK100K with a 325 kHz receive bandwidth and update the complete stream, metadata, hashes, and tests.
- **Status**: ✅ Complete
- **Commit**: `Correct MSK100K receive bandwidth`

### Phase 3 — HIL failure safety and hop assertions
- **Description**: Restore modulation on every HIL path and require fresh post-transition hopper baselines; add injected-failure unit coverage.
- **Status**: 🔄 In progress
- **Commit**: `Harden Sub-GHz HIL cleanup and hop checks`

### Phase 4 — Verification and review responses
- **Description**: Update docs/changelog, run host/firmware/HIL verification, remove this checklist, push, and reply to reviewers.
- **Status**: 🔲 Not started
- **Commit**: `Finalize Sub-GHz review verification`
