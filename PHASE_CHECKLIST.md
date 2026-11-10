# Phase Checklist — Recursive visual UX compliance

## PR Metadata
- **PR Title**: Bring all menu scenes into visual UX compliance
- **PR Description**: Audit every safely reachable menu scene against the visual UX skill, correct non-compliant rendering and navigation, and verify with host and recursive HIL coverage.

## Phases

### Phase 1 — Scene inventory and baseline audit
- **Description**: Map all audit destinations and unsafe exclusions to implementations, run safe baseline traversal, and record confirmed visual violations.
- **Status**: ✅ Complete
- **Commit**: `Audit menu scenes for visual UX compliance`

### Phase 2 — Shared visual-standard corrections
- **Description**: Fix headers, button bars, selectors, and other shared rendering violations with regression tests.
- **Status**: ✅ Complete
- **Commit**: `Fix shared scene visual UX violations`

### Phase 3 — Module-specific corrections
- **Description**: Fix remaining module-specific UX violations and update recursive HIL coverage where structure changes.
- **Status**: ✅ Complete
- **Commit**: `Fix module scene visual UX violations`

### Phase 4 — Full validation
- **Description**: Run host suites, build and flash firmware, and repeat every safe recursive menu audit on the corrected implementation.
- **Status**: ✅ Complete
- **Commit**: `Validate recursive visual UX compliance`

### Phase 5 — Finalize
- **Description**: Review the final diff and validation evidence, then remove this temporary checklist before any PR is created.
- **Status**: ✅ Complete
- **Commit**: `Review recursive visual UX compliance`
