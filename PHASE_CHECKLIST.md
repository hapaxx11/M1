# Phase Checklist — Meshtastic review fixes

## PR Metadata
- **PR Title**: Add Meshtastic external app
- **PR Description**: Resolve the outstanding review findings about UART DMA handoff and roster SNR precision.

## Phases

### Phase 1 — UART DMA handoff
- **Description**: Serialize USB-to-UART DMA startup with app ownership and add host regression coverage.
- **Status**: ✅ Complete
- **Commit**: `Serialize UART DMA handoff`

### Phase 2 — Roster SNR precision
- **Description**: Preserve fractional SNR in roster storage and details display with regression coverage.
- **Status**: 🔄 In progress
- **Commit**: _(pending)_

### Phase 3 — Final verification
- **Description**: Run targeted tests/builds and remove this temporary checklist.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_
