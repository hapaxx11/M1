# Phase Checklist — TPMS Pressure and Temperature

## PR Metadata
- **PR Title**: Show TPMS pressure and temperature
- **PR Description**: Add Momentum-compatible Schrader GG4 telemetry parsing and display pressure and temperature on TPMS list and detail screens while retaining legacy Schrader decoding.

## Phases

### Phase 1 — Telemetry parsing
- **Description**: Add tested Schrader GG4 frame validation and pressure/temperature conversion.
- **Status**: ✅ Complete
- **Commit**: `Add Schrader GG4 telemetry parsing`

### Phase 2 — Receiver and UI integration
- **Description**: Extend Schrader decoding to 64-bit GG4 frames without losing legacy 40-bit support, display telemetry in TPMS list/detail screens, and update user-facing documentation.
- **Status**: 🔄 In progress
- **Commit**: _(pending)_

### Phase 3 — Finalize
- **Description**: Validate tests/build/RAM, review changes, then remove this checklist.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_
