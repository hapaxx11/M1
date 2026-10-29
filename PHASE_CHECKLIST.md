# Phase Checklist — Version workflow review fixes

## PR Metadata
- **PR Title**: Fix version workflow selection and branch updates
- **PR Description**: Use one version-selection rule across manual bumps, releases, and changelog stamping; reject values outside firmware component limits and dispatch agent-branch updates after manual bumps.

## Phases

### Phase 1 — Shared version selection
- **Description**: Add and test shared version-selection logic, then use it in the manual bump, release, and changelog workflows.
- **Status**: ✅ Complete
- **Commit**: `fix: unify release version selection`

### Phase 2 — Dispatch branch updates
- **Description**: Make update-branches dispatchable and run it after the version bump push.
- **Status**: ✅ Complete
- **Commit**: `fix: dispatch agent branch updates`

### Phase 3 — Preserve pending prefix bumps
- **Description**: Ensure automatic release and changelog selection honor pending major, minor, and build bumps as well as pending revisions.
- **Status**: ✅ Complete
- **Commit**: `fix: preserve pending version bumps`
