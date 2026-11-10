# Phase Checklist — Excluded-scene source UX audit

## PR Metadata
- **PR Title**: Bring menu scenes into visual UX compliance
- **PR Description**: Audit and correct scenes that cannot be exercised safely on hardware, using source-level evidence and host-only validation.

## Phases

### Phase 1 — Source-only excluded-scene audit
- **Description**: Map HIL exclusions to implementations and inspect rendering, navigation, action bars, headers, selectors, and saved-item flows.
- **Status**: ✅ Complete
- **Commit**: `Audit excluded scenes for source UX compliance`

### Phase 2 — Excluded-scene corrections
- **Description**: Fix confirmed UX violations without running or flashing hardware and add source regressions.
- **Status**: ✅ Complete
- **Commit**: `Fix excluded scene UX violations`

### Phase 3 — Host validation and PR update
- **Description**: Run host suites and build, then commit, push, and document source-only evidence.
- **Status**: 🔄 In progress
- **Commit**: `Validate excluded scene UX fixes`

### Phase 4 — Finalize
- **Description**: Remove this temporary checklist before updating the draft PR.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_
