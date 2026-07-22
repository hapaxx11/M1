# Phase Checklist — Signal Identifier Enhancement Plan

## PR Metadata
- **PR Title**: Signal ID Phase 4A: Timing capture — RSSI-burst → te_us/repetition fingerprinting
- **PR Description**: Implements Phase 4A of the Signal Identifier enhancement plan. Adds `rf_timing_capture.c/h` — a pure-logic, host-tested module that converts the RSSI burst collected during each sweep detection into a mark/space timing array. This array is fed to `rf_fingerprint_from_subghz_raw()` so the sweep fingerprint now carries `te_us`, `pulse_count`, `est_bits`, and `repetition` — the discriminating features that were previously always zero. The scoring engine can now distinguish protocols by timing class, dramatically improving identification accuracy. Phases 1A, 1B, 2A, and 3B were already implemented in earlier commits.

## Phases

### Phase 1A — Frequency per hit (trivial)
- **Description**: Include measured frequency in each hit row rendered on screen.
- **Status**: ✅ Complete
- **Commit**: `Add Signal ID display improvements: frequency per hit, security prefix, tag stripping, repeat-and-confirm`

### Phase 1B — Clean up tag display (small)
- **Description**: Strip parenthetical tags (e.g. "(fixed)", "(868)") from protocol names before rendering.
- **Status**: ✅ Complete
- **Commit**: `Add Signal ID display improvements: frequency per hit, security prefix, tag stripping, repeat-and-confirm`

### Phase 2A — Smart Scan integration (moderate)
- **Description**: Freq Scanner pre-scan feeds Signal ID so identification dwells only on active channels.
- **Status**: ✅ Complete
- **Commit**: `Phase 2A: Smart Scan integration — Freq Scanner feeds Signal ID`

### Phase 3B — Repeat-and-confirm (simple)
- **Description**: Show "?%" confidence until a signal has been seen SIGID_MIN_HITS (2) times.
- **Status**: ✅ Complete
- **Commit**: `Add Signal ID display improvements: frequency per hit, security prefix, tag stripping, repeat-and-confirm`

### Phase 4A — Timing capture (ambitious)
- **Description**: Convert the per-detection RSSI burst into a pseudo-timing array; feed to rf_fingerprint_from_subghz_raw() to populate te_us, pulse_count, est_bits, and repetition. Adds rf_timing_capture.c/h (pure logic, 17 host tests). Both sub_ghz_signal_identifier() and sub_ghz_smart_signal_id() updated.
- **Status**: ✅ Complete
- **Commit**: `Phase 4A: Timing capture — RSSI burst → te_us/repetition fingerprint upgrade`
