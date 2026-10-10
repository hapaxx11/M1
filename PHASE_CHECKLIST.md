# Phase Checklist — ProtoPirate keystore input formats

## PR Metadata
- **PR Title**: Support decrypted ProtoPirate keystore secret formats
- **PR Description**: Accept the decrypted Kia standard-entry list and VAG offset hexdump as build secrets. Add synthetic regression tests and document supported formats without committing key material.

## Phases

### Phase 1 — Keystore format support and tests
- **Description**: Extend the strict parser for the supplied headerless formats and test valid and invalid inputs using synthetic values.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_

### Phase 2 — Documentation and validation
- **Description**: Update key-injection documentation and changelog; run Python and host-side suites and review the final diff.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_

### Phase 3 — Checklist cleanup
- **Description**: Remove this temporary checklist and verify it is absent from the final changes.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_
