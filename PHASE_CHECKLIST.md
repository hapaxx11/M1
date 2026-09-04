# Phase Checklist — ProtoPirate Tier-A MVP integration

## PR Metadata
- **PR Title**: Add ProtoPirate Tier-A automotive keyfob TX emulation
- **PR Description**: Add `Sub_Ghz/subghz_proto_pirate.c/h` as a pure-logic encoder dispatcher for ProtoPirate automotive protocols, wire it into the existing key-encoder and registry, and expose an "Emulate" entry in the Proto Pirate menu. Tier-A OOK/Manchester protocols only; crypto-locked protocols deferred.

## Phases

### Phase 1 — Infrastructure scaffolding
- **Description**: Create `Sub_Ghz/subghz_proto_pirate.h` with dispatcher API and protocol catalog; create `.c` with shared helpers and first encoder (Kia V7). Wire dispatcher into `subghz_key_encoder.c`. Add enum/registry entries and CMakeLists source.
- **Status**: 🔄 In progress
- **Commit**: _(pending)_

### Phase 2 — Host regression tests
- **Description**: Add `tests/test_subghz_proto_pirate.c` covering dispatcher routing and at least one known-key encode for the first protocol.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_

### Phase 2 — Host regression tests
- **Description**: Add `tests/test_subghz_proto_pirate.c` covering dispatcher routing and at least one known-key encode for the first protocol.
- **Status**: ✅ Complete
- **Commit**: `ProtoPirate Tier-A/B scaffolding: dispatcher, Kia V7 encoder, registry, UI Emulate scene`

### Phase 3 — Tier-B encoder implementations
- **Description**: Add encoders for Honda V1/V2, Ford V1/V2, Kia V3/V4/V5, Fiat V1 in `subghz_proto_pirate.c`. Honda V1 (PWM 68-bit), Honda V2 (Manchester 81-bit), Ford V1 (Manchester 136-bit + CRC-16), Ford V2 (Manchester 104-bit), Kia V3/V4 (PWM + KeeLoq + CRC sweep), Kia V5 (Manchester + custom mixer), Fiat V1 (PWM + Hitag2 authenticator).
- **Status**: 🔄 In progress
- **Commit**: `ProtoPirate Tier-B partial: Honda V1/V2 + Ford V2 encoders with host tests`

### Phase 4 — Additional Tier-A encoders
- **Description**: Add encoders for Ford V0, Mazda V0, Honda Static, Kia V0/V1/V2, Subaru, Chrysler V0, Renault V0, Fiat V0 as ProtoPirate source is audited.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_

### Phase 5 — Per-encoder host regression tests
- **Description**: Expand `tests/test_subghz_proto_pirate.c` with known-key/known-structure tests for each implemented Tier-A/B encoder.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_

### Phase 6 — UI Emulate scene
- **Description**: Add "Emulate" to `m1_subghz_scene_proto_pirate_menu.c` and create `m1_subghz_scene_proto_pirate_emulate.c` for protocol picker + parameter entry.
- **Status**: ✅ Complete
- **Commit**: `ProtoPirate Tier-A/B scaffolding: dispatcher, Kia V7 encoder, registry, UI Emulate scene`

### Phase 7 — Build, RAM check and changelog
- **Description**: ARM firmware build; verify linker RAM summary. Add `.changelog/` fragment and remove this checklist before PR.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_
