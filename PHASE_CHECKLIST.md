# Phase Checklist — CD3.148 Feature Parity (excluding qMonstatek)

## PR Metadata
- **PR Title**: Implement selected CD3.148 transport and network features
- **PR Description**: Add the missing M1-side CD3.148 features in independently testable phases, preserving USB/legacy behavior and coordinating protocol changes that require ESP32 firmware support. The qMonstatek relay-task fix is excluded because its source is not in this repository.

## Phases

### Phase 1 — Preserve the ESP32 RX buffer across UART cycles
- **Description**: Stop freeing the reusable ESP32 RX allocation during UART deinit, so repeated init/deinit cycles reuse it; add regression coverage for the allocation lifetime and existing teardown behavior.
- **Status**: ✅ Complete
- **Commit**: `Preserve ESP32 RX buffer across UART cycles`

### Phase 2 — Primary WiFi network and opt-in boot connection
- **Description**: Persist a selected primary saved-network index and WiFi-on-boot preference, expose selection/toggle in Saved Networks, and attempt only that network through the existing firmware-variant dispatch. The current join APIs are synchronous, so the serialized one-shot boot attempt may delay menu interaction until their bounded timeout returns.
- **Status**: ✅ Complete
- **Commit**: `Add primary WiFi boot auto-connect`

### Phase 3 — GPIO and I2C scanner RPC APIs
- **Description**: Add documented USB RPC commands with bounded payloads for a read-only external GPIO snapshot and I2C address-range scan; defer the mutex-protected I2C scan and host-test request validation and response encoding.
- **Status**: ✅ Complete
- **Commit**: `Add GPIO and I2C RPC scanners`

### Phase 4 — Request-scoped transport routing and mobile-session state
- **Description**: Replace USB-only response assumptions with an ingress-aware RPC transport interface, preserve per-request reply routing, and report accurate active-session state without changing existing USB clients.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_

### Phase 5 — BLE Direct (NUS), exclusive client ownership, and screen mirroring
- **Description**: Add opt-in BLE NUS RPC transport and connection lifecycle, arbitrate the single BLE-or-WiFi RPC client, and stream display frames at a bounded 5 fps. Requires a compatible ESP32 firmware protocol/transport implementation and hardware validation.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_

### Phase 6 — Editable SoftAP configuration and NAT passthrough
- **Description**: Complete persisted/editable SoftAP settings, add upstream station plus AP routing/NAT control, and expose lifecycle/status through the RPC feature layer. Requires ESP32 firmware support for concurrent AP/STA and NAT; do not claim parity until on-device validation.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_

### Phase 7 — End-to-end verification and checklist cleanup
- **Description**: Run host tests and available firmware builds with RAM review; validate network, BLE, scanner, session-routing, and reconnect behavior on hardware; document remaining ESP32 firmware dependencies and remove this temporary checklist before any PR.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_
