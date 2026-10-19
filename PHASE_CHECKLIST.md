# Phase Checklist — ProtoPirate remaining parity

## PR Metadata
- **PR Title**: Complete portable ProtoPirate app features and automotive decoders
- **PR Description**: Port the remaining ProtoPirate features that have a viable M1 implementation: saved-capture match/update, real automotive RX decoders, the models database, and an SI4463-native Remote Analyzer. Preserve the existing exclusion for Renault V1 full-key recovery and avoid adding private key material or unverified TX capability.

## Phases

### Phase 1 — Saved-capture match/update
- **Description**: Add pure identity/counter matching, scan both `.sub` and `.sgh` saved captures, persist decoded Serial/Btn/Cnt fields, and offer Update in Receiver Info when a capture matches an existing file. Cover matching, counter margins, field persistence, and update behavior with host tests.
- **Status**: ✅ Complete
- **Commit**: `Add ProtoPirate saved-capture match and update flow`

### Phase 2 — Honda decoder family
- **Description**: Replace Honda Static, V1, and V2 placeholders with pure frame parsers and waveform decoders derived from ProtoPirate. Audit registry modulation, timing, bands, and frequencies; add synthetic positive/negative waveform tests.
- **Status**: ✅ Complete
- **Commit**: `Port ProtoPirate Honda decoders`

### Phase 3 — Ford decoder family
- **Description**: Replace Ford V0, V1, and V2 placeholders with real RX decoders and host-tested frame parsing, including the FM/Manchester variants and field persistence required by their captures.
- **Status**: 🔲 Not started
- **Commit**: `Port ProtoPirate Ford V0-V2 decoders`

### Phase 4 — Kia decoder family
- **Description**: Replace Kia V0, V1, V2, V3/V4, V5, and V7 placeholders with real RX decoders. Reuse the upstream shared Kia V3/V4 implementation without duplicating divergent logic, and test every registry entry independently.
- **Status**: 🔲 Not started
- **Commit**: `Port ProtoPirate Kia decoder family`

### Phase 5 — Fiat and Renault V0 decoders
- **Description**: Port Fiat V0/V1 and Renault V0 RX decoding. Keep Fiat V1 key-dependent TX/key-resolution behavior out of this decoder phase unless separately provisioned; do not reopen the excluded Renault V1 full-key path.
- **Status**: 🔲 Not started
- **Commit**: `Port ProtoPirate Fiat and Renault V0 decoders`

### Phase 6 — Mazda, Subaru, and Chrysler decoders
- **Description**: Replace the remaining Mazda V0, Subaru, and Chrysler V0 placeholders with host-tested frame parsers and waveform decoders; persist their protocol-specific second-key/check fields where needed.
- **Status**: 🔲 Not started
- **Commit**: `Port remaining ProtoPirate automotive decoders`

### Phase 7 — Models database
- **Description**: Add a bounded, host-tested v2 models-file parser and receiver preset selection. Map standard Flipper AM/FM presets and the published Honda custom profile to M1 SI4463 configuration rather than consuming CC1101 register writes; document SD installation and validation.
- **Status**: 🔲 Not started
- **Commit**: `Add ProtoPirate models database support`

### Phase 8 — SI4463 Remote Analyzer
- **Description**: Implement the Remote Analyzer as an asynchronous M1 scene using existing SI4463 RSSI/frequency-scan infrastructure. Extract pure frequency-pair/modulation classification for host tests, preserve radio cleanup/cancellation, and validate detection on hardware.
- **Status**: 🔲 Not started
- **Commit**: `Add SI4463 Remote Analyzer scene`

### Phase 9 — Parity validation and documentation closeout
- **Description**: Run the full host suite and firmware build, inspect RAM headroom, update protocol/feature inventories and the emulation-status matrix, and record any items that still require on-device validation. Remove this temporary checklist before the final PR.
- **Status**: 🔲 Not started
- **Commit**: `Finalize ProtoPirate parity validation`
