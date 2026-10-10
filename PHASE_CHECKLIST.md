# Phase Checklist — ProtoPirate keystore input formats

## PR Metadata
- **PR Title**: Support decrypted ProtoPirate keystore secret formats
- **PR Description**: Accept the decrypted Kia standard-entry list and VAG offset hexdump as build secrets. Add synthetic regression tests and document supported formats without committing key material.

## Phases

### Phase 1 — Parser, tests, and documentation
- **Description**: Extend parsers for supplied plaintext formats; add synthetic input/generator coverage and document supported secret formats.
- **Status**: ✅ Complete
- **Commit**: `fix: accept decrypted ProtoPirate keystores`

### Phase 2 — Checklist cleanup
- **Description**: Remove this temporary checklist and verify it is absent from the final changes.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_
