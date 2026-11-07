# Phase Checklist — PR 837 review fixes

## PR Metadata
- **PR Title**: Integrate native ESP32 radio suite
- **PR Description**: Integrate native ESP32 WiFi/BLE tools, dual-slot OTA, Hapax release downloads, and clock synchronization with host and hardware validation.

## Phases

### Phase 1 — Catalog and documentation fixes
- **Description**: Make Hapax migration capacity-aware, support both factory filenames in both directions, and correct the Hapax firmware recommendation.
- **Status**: ✅ Complete
- **Commit**: `Fix source catalog migration and aliases`

### Phase 2 — PMKID validation
- **Description**: Bound PMKID KDE parsing by the declared EAPOL body length and add malformed-packet regression coverage.
- **Status**: 🔄 In progress
- **Commit**: `Validate EAPOL body before PMKID export`

### Phase 3 — HIL and staging robustness
- **Description**: Preserve recovery results, flush/eject staged media, redact captures and public errors, enforce transition budgets, and retain unexecuted PR checklist entries.
- **Status**: 🔲 Not started
- **Commit**: `Harden HIL staging and result reporting`

### Phase 4 — Final validation and cleanup
- **Description**: Run full host suites and applicable real-device validation, review the final diff, and remove this checklist from the PR.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_
