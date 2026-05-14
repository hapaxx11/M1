# Phase Checklist — M1 RGB Mod firmware scaffolding

## PR Metadata
- **PR Title**: Add compile-gated RGB backlight driver, settings app, and tests
- **PR Description**: Introduce an initial, hardware-agnostic RGB backlight implementation for the upcoming M1 RGB Mod, wire it into Settings persistence/UI behind `M1_HAS_RGB_BACKLIGHT`, and add host-side unit tests for color conversion and animation math.

## Phases

### Phase 1 — Driver and settings state
- **Description**: Add RGB backlight driver/header APIs and persist RGB settings in `settings.cfg` with compile-time gating.
- **Status**: ✅ Complete
- **Commit**: `Add RGB backlight scaffold with settings persistence`

### Phase 2 — Settings UI integration
- **Description**: Add RGB backlight settings app and integrate it into LCD & Notifications flow using font-aware menu helpers.
- **Status**: ✅ Complete
- **Commit**: `Add RGB backlight scaffold with settings persistence`

### Phase 3 — Tests and build wiring
- **Description**: Add host-side RGB unit tests, wire CMake/test workflow paths, and run validation.
- **Status**: ✅ Complete
- **Commit**: `Add RGB backlight scaffold with settings persistence`
