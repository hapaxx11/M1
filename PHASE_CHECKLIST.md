# Phase Checklist — PR #822 review fixes

## PR Metadata
- **PR Title**: Fix PR #822 review findings
- **PR Description**: Correct Ford V2 timing classification, seed Kia V3/V4 counters from key-only captures, make saved-capture matching asynchronous, and preserve KeeLoq replay metadata on update.

## Phases

### Phase 1 — Ford V2 timing
- **Description**: Prefer the nearer nominal pulse duration for overlapping preamble/sync ranges and cover jittered preambles.
- **Status**: ✅ Complete
- **Commit**: `Fix Ford V2 timing overlap`

### Phase 2 — Kia V3/V4 counter seeding
- **Description**: Resolve key-only capture counters before initializing the replay cache and test consecutive encoder preparations.
- **Status**: ✅ Complete
- **Commit**: `Seed Kia replay counters from key-only captures`

### Phase 3 — Saved capture UI and metadata
- **Description**: Incrementally scan saved captures on scene ticks, reject protocol mismatches early, and retain KeeLoq metadata during updates.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_

### Phase 4 — Final verification
- **Description**: Run targeted host tests, firmware build/RAM check, and automated code review; remove this checklist.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_
