<!-- See COPYING.txt for license details. -->

# ESP-NOW Peer Link — Design & Implementation Plan

**Status:** Draft — planning only, no implementation started.
**Issue:** hapaxx11/M1#661 (closes hapaxx11/M1#661)
**Related:** hapaxx11/M1#660 (deferred from SPI-slave brain architecture)

---

## 1. Background & Motivation

The original M1-to-M1 ESP-NOW peer link concept (CRC32 file transfer,
handshake, peer Tic-Tac-Toe) was deferred in #660 because it depended on the
rejected SPI-slave brain architecture.  This document re-evaluates the feature
as a from-scratch design against Hapax's existing ESP32 coprocessor abstraction.

ESP-NOW is an Espressif connectionless protocol that allows short (up to 250
bytes per frame) encrypted or plaintext data exchange between ESP32 devices
without needing a WiFi access point.  It operates on the 2.4 GHz WiFi channels
and supports both unicast (peer-addressed) and broadcast (all-listen) modes.

### Use Cases

1. **Peer Discovery** — Two M1 devices within range can discover each other
   and establish a session via a visual/button-based pairing confirmation.
2. **CRC32 File Transfer** — Push Sub-GHz captures (`.sub`/`.sgh`), IR
   databases, or arbitrary small files between paired M1 devices with CRC32
   integrity verification.
3. **Peer Tic-Tac-Toe** — A simple two-player game to demonstrate real-time
   bidirectional communication over ESP-NOW.

---

## 2. CD3 Reference Implementation (bedge117/m1-esp32-brain)

The CD3 firmware **already implements** ESP-NOW peer link in the
`components/esp_now_link` component.  Our design defers to their implementation
as the reference protocol since they were first.

### 2.1 CD3 Wire Protocol (existing, we adopt this)

Every ESP-NOW radio frame uses the wire format:

```
[ 'M' (0x4D) ][ '1' (0x31) ][ type:1 ][ payload:0..240 ]
```

| Type | Name | Payload |
|------|------|---------|
| `0x00` | `ANNOUNCE` | Device name string (up to 23 bytes) |
| `0x01` | `DATA` | User message bytes (up to 240 bytes) |

Constants: `ENL_NAME_MAX = 23`, `ENL_MSG_MAX = 240`, `ENL_MAX_PEERS = 16`.

### 2.2 CD3 M1_RPC Commands (existing, range `0x0600..0x06FF`)

| msg_id | Name | Direction | Payload |
|--------|------|-----------|---------|
| `0x0600` | `M1_RPC_NOW_START` | REQ→ESP | ch(1) + name string → RESP status(1) + mac(6) |
| `0x0601` | `M1_RPC_NOW_STOP` | REQ→ESP | none → RESP status(1) |
| `0x0602` | `M1_RPC_NOW_ANNOUNCE` | REQ→ESP | none → RESP status(1) |
| `0x0603` | `M1_RPC_NOW_PEERS_GET` | REQ→ESP | none → RESP count(1) + [mac(6)+rssi(1)+namelen(1)+name]×N |
| `0x0604` | `M1_RPC_NOW_SEND` | REQ→ESP | mac(6) + data → RESP status(1) |
| `0x0605` | `M1_RPC_NOW_RECV_GET` | REQ→ESP | none → RESP count(1) + [mac(6)+len(2 LE)+data]×N |

**Key CD3 design choices we inherit:**
- **No encryption** — `encrypt = false` always.  ESP-NOW CCMP encryption is not
  used.  This is acceptable for our use case (local device-to-device, visual
  pairing confirmation provides MITM awareness).  If a future CD3 release adds
  encryption we will support it, but we do not require it.
- **Channel set on init** — the caller chooses the channel at `NOW_START` time.
  There is no built-in channel negotiation in the ESP-NOW protocol itself.
- **Broadcast for discovery** — presence is announced via broadcast (`FF:FF:FF:FF:FF:FF`).
  Discovered peers are tracked in a 16-slot table (LRU eviction).
- **Polling model** — the STM32 polls `NOW_RECV_GET` to drain received messages.
  The ESP32 buffers up to 32 inbound messages in a ring buffer.

### 2.3 Capability Bit (not yet self-reported by CD3)

CD3 implements the `M1_RPC_NOW_*` handlers but does **not** yet set a
capability bit in `M1_FW_CAPS`.  We will:

1. Reserve `M1_ESP32_CAP_ESPNOW` (bit 21) on the STM32 side.
2. Gate all peer-link scenes on `m1_esp32_require_cap(M1_ESP32_CAP_ESPNOW, ...)`.
3. Propose that CD3 adds `M1_CAP_ESPNOW = (UINT64_C(1) << 21)` to its
   `m1_rpc.h` and ORs it into `M1_FW_CAPS`.

Until CD3 self-reports the bit, we can use a fallback probe: attempt
`M1_RPC_NOW_START` + immediate `M1_RPC_NOW_STOP` — if both succeed, infer
the capability is present and cache it (same pattern as the HANDSHAKE detection
workaround documented in `m1_esp32_caps.h`).

---

## 3. Architecture Constraints

### 3.1 Firmware-Agnostic Capability Gate

The M1 supports multiple ESP32 firmware variants (SiN360, CD3-AT, CD3, dag
T-800).  Every ESP32-dependent feature is gated at runtime by the
`M1_ESP32_CAP_*` bitmap system (`m1_esp32_caps.h`).  ESP-NOW peer link MUST
follow the same pattern:

- A new capability bit (`M1_ESP32_CAP_ESPNOW`, bit 21) indicates that the
  connected ESP32 firmware supports ESP-NOW operations.
- All peer-link scenes gate on `m1_esp32_require_cap(M1_ESP32_CAP_ESPNOW,
  "ESP-NOW Peer Link")` before attempting any ESP-NOW operation.
- A new `ESP32_FEATURE_ESPNOW` entry in `esp32_feature_map.h/.c` maps the
  feature to its required cap bit and UI label.

### 3.2 Transport Independence

The STM32 host must not care which protocol variant the ESP32 speaks.  The
ESP-NOW commands MUST be deliverable over:

| Firmware | Transport | Path |
|----------|-----------|------|
| CD3 (native RPC) | M1_RPC binary frames | `M1_RPC_NOW_*` range `0x0600..0x0605` (already implemented) |
| CD3-AT / dag T-800 | AT text commands | `AT+M1ESPNOW=...` custom command (future) |
| SiN360 | Binary SPI opcodes | Not supported (SiN360 won't set `M1_ESP32_CAP_ESPNOW`) |

The STM32-side implementation uses a thin HAL-layer function
(`m1_espnow_send_cmd()`) that internally dispatches to the correct transport
based on the detected firmware class, mirroring the pattern used by WiFi attack
functions (`m1_wifi.c` checks `m1_esp32_has_cap(M1_ESP32_CAP_WIFI_JOIN)` and
switches between AT and binary paths).

### 3.3 SPI MTU and Message Size

The CD3 M1_RPC transport uses a larger MTU than the 64-byte SPI-HD transaction
size.  The `M1L_MTU` constant in the CD3 `m1_rpc.h` is **512 bytes**, and
`M1_RPC_MAX_PAYLOAD` accommodates the full ESP-NOW message size (240 bytes +
framing).  Therefore:

- `M1_RPC_NOW_SEND` carries the full ESP-NOW payload (up to 240 data bytes +
  6 MAC bytes) in a single RPC transaction — no chunking needed.
- `M1_RPC_NOW_RECV_GET` returns multiple queued messages packed in one response.

For file transfer (which sends application-layer chunks over ESP-NOW), the
chunking happens at the application protocol level (see §6), not the SPI
transport level.

### 3.4 Idle Power-Off Interaction

The ESP32 auto powers off after 60s idle (esp32_idle.c state machine).  While
an ESP-NOW session is active, the idle timer must be suppressed.  The peer-link
scene will call `esp32_idle_activity()` (or equivalent) to keep the ESP32
powered during active peer sessions.

---

## 4. Channel Coordination

ESP-NOW does **not** define a built-in channel negotiation mechanism.  All peers
must be on the same WiFi channel to communicate.  The Espressif ESP-NOW
specification leaves channel management entirely to the application layer.

**Our standard (no public protocol-level standard exists):**

The **acknowledging device** (responder) always hops to the initiator's channel.
If the responder does not hop, the initiator treats it as a declined connection
(equivalent to a pairing rejection).  Rationale:

- The initiator is already broadcasting on its channel — changing it would break
  ongoing discovery for other devices listening on that channel.
- The responder explicitly accepts the peer request, so hopping is a conscious
  action tied to the user pressing "Accept" on the pairing screen.
- This matches the CD3 `esp_now_link_start(channel, name)` API — the caller
  sets the channel, so the STM32 host scene on the responder simply calls
  `NOW_START` with the initiator's channel (included in the ANNOUNCE beacon).

**Concurrent WiFi implications:**

When the acknowledging device hops to the initiator's channel, its WiFi STA
connection (if active) is unaffected IF already on the same channel, or the
STA will be forced to the new channel (ESP32-C6 single-radio limitation).  The
STM32 host does NOT need to prompt for WiFi disconnection — the channel hop is
handled transparently by the ESP32 when accepting the peer link.  If the
responder's WiFi STA is on a different channel, the STA will temporarily lose
connectivity for the duration of the ESP-NOW session (acceptable tradeoff,
since the user explicitly accepted the peer link).

---

## 5. Peer Discovery & Handshake Protocol

### 5.1 Discovery Phase

1. Device A enters "Peer Link" scene → calls `M1_RPC_NOW_START(channel, name)`
   to initialise ESP-NOW and begin broadcasting ANNOUNCE beacons.
2. The CD3 firmware automatically broadcasts `['M','1', 0x00, name...]` on
   start and can be triggered again with `M1_RPC_NOW_ANNOUNCE`.
3. Device A periodically polls `M1_RPC_NOW_PEERS_GET` to retrieve the
   discovered-peer table (MAC + RSSI + name for each peer in range).
4. User selects a discovered peer from the list → initiates pairing.

### 5.2 Pairing Confirmation

1. Device A sends a unicast `PAIR_REQUEST` (via `M1_RPC_NOW_SEND`) to Device B's MAC.
2. Device B displays "Pair with <name>?" confirmation screen.
3. Device B hops to Device A's channel (if different) by calling
   `M1_RPC_NOW_START(initiator_channel, name)`.
4. Device B responds with `PAIR_ACCEPT` or `PAIR_REJECT` via `M1_RPC_NOW_SEND`.
5. On `PAIR_ACCEPT`, both devices are now communicating on the same channel.
   A 4-digit visual confirmation code derived from
   `CRC32(mac_A || mac_B)[0:2]` is shown — user confirms they match
   (basic MITM awareness, no encryption required).

### 5.3 Session State Machine (STM32 side, pure logic)

```
IDLE → SCANNING → PEER_FOUND → PAIR_SENT → PAIRED → (FILE_TX | GAME | ...) → IDLE
                                          ↓
                                    PAIR_REJECTED → SCANNING
```

This state machine will be implemented as a pure-logic module
(`espnow_peer_session.c/h`) under `m1_csrc/`, host-testable with no HAL deps.

---

## 6. CRC32 File Transfer Protocol

### 6.1 Transfer Framing

Built on top of the ESP-NOW peer link session (section 5):

| Byte offset | Size | Field |
|-------------|------|-------|
| 0 | 1 | Message type: `FILE_OFFER=0x10`, `FILE_ACCEPT=0x11`, `FILE_REJECT=0x12`, `FILE_DATA=0x13`, `FILE_ACK=0x14`, `FILE_COMPLETE=0x15`, `FILE_ABORT=0x16` |
| 1 | 1 | Sequence number (wraps at 255) |
| 2..N | varies | Type-specific payload |

### 6.2 Transfer Flow

1. **Sender** sends `FILE_OFFER`: filename[32] + file_size(4 LE) + crc32(4 LE) + chunk_size(1).
2. **Receiver** displays "Accept <filename> (<size> bytes)?" → responds `FILE_ACCEPT` or `FILE_REJECT`.
3. **Sender** streams `FILE_DATA` chunks: seq(1) + offset(4 LE) + data[0..200].
   Each chunk is individually ACK'd by the receiver (`FILE_ACK` with matching seq).
4. After all chunks sent, sender sends `FILE_COMPLETE`.
5. Receiver verifies CRC32 over the reassembled file:
   - Match → saves to SD card, shows success screen.
   - Mismatch → sends `FILE_ABORT` with error code, sender retries or aborts.

### 6.3 Flow Control

- Window size = 1 (stop-and-wait ARQ) for simplicity in v1.
- Timeout: 500 ms per chunk ACK; 3 retries before abort.
- **Streaming-to-SD** — the receiver writes chunks directly to SD card via
  FatFS as they arrive (no full-file RAM buffer).  This is the obvious choice
  given the unified FreeRTOS heap-4 architecture (#526) where RAM is a shared
  resource with known constraints.  CRC32 is accumulated incrementally as each
  chunk is written.  No practical file-size limit beyond SD card capacity.

### 6.4 Pure-Logic Module

`espnow_file_transfer.c/h` — state machine + CRC32 accumulation + chunk
reassembly.  Host-testable.  The HAL boundary is a small
`espnow_ft_hal_ops_t` struct with function pointers for:
- `send(mac, data, len)` — transmit an ESP-NOW frame
- `file_open(path, mode)` / `file_write(buf, len)` / `file_close()` — SD access
- `millis()` — timeout source

---

## 7. Peer Tic-Tac-Toe Protocol

### 7.1 Game Messages (over ESP-NOW unicast)

| Type | Value | Payload |
|------|-------|---------|
| `GAME_INVITE` | `0x20` | game_id(1) = TTT(0x01) |
| `GAME_ACCEPT` | `0x21` | game_id(1) |
| `GAME_REJECT` | `0x22` | game_id(1) |
| `GAME_MOVE` | `0x23` | game_id(1), cell(1) [0-8, row-major] |
| `GAME_STATE` | `0x24` | game_id(1), board[9] (0=empty, 1=X, 2=O), turn(1) |
| `GAME_END` | `0x25` | game_id(1), result(1) [0=draw, 1=host_win, 2=peer_win] |
| `GAME_QUIT` | `0x26` | game_id(1) |

### 7.2 Pure-Logic Core

`espnow_tictactoe.c/h` — board validation, win detection, move application.
No display/HAL deps.  The scene (`m1_espnow_scene_tictactoe.c`) handles draw +
input, calling into the pure-logic core for state transitions.

---

## 8. STM32-Side Module Structure

```
m1_csrc/
├── espnow_peer_session.c/h    # Discovery + pairing state machine (pure logic)
├── espnow_file_transfer.c/h   # File transfer protocol (pure logic)
├── espnow_tictactoe.c/h       # Tic-Tac-Toe game logic (pure logic)
├── m1_espnow_hal.c/h          # HAL glue: transport dispatch (CD3 RPC / AT / SiN360)
├── m1_espnow_scene_main.c     # Peer Link main menu scene
├── m1_espnow_scene_scan.c     # Peer discovery / scan list scene
├── m1_espnow_scene_pair.c     # Pairing confirmation scene
├── m1_espnow_scene_transfer.c # File send/receive progress scene
└── m1_espnow_scene_tictactoe.c # Tic-Tac-Toe game scene

tests/
├── test_espnow_peer_session.c
├── test_espnow_file_transfer.c
└── test_espnow_tictactoe.c
```

---

## 9. Capability Bit & Feature Map Changes

### 9.1 New Capability Bit

```c
/* m1_esp32_caps.h — bit 21 */
/** ESP-NOW peer-to-peer communication (discovery, unicast, broadcast) */
#define M1_ESP32_CAP_ESPNOW         (UINT64_C(1) << 21)
```

### 9.2 Feature Map Addition

```c
/* esp32_feature_map.h */
/** ESP-NOW peer link (discovery + file transfer + games) */
ESP32_FEATURE_ESPNOW,

/* esp32_feature_map.c */
{ M1_ESP32_CAP_ESPNOW,  "ESP-NOW Peer Link" },
```

### 9.3 Profile Updates

- `M1_ESP32_CAP_PROFILE_CD3` will include `M1_ESP32_CAP_ESPNOW` once CD3
  adds the bit to its `M1_FW_CAPS` (the RPC handlers already exist).
- AT firmware variants will gain the capability when they implement
  `AT+M1ESPNOW`.
- SiN360 profile is unlikely to add this (no planned development).

---

## 10. Multi-Firmware Support Summary

| Concern | Resolution |
|---------|-----------|
| How does the STM32 know if ESP-NOW is available? | `M1_ESP32_CAP_ESPNOW` bit in probed `cap_bitmap` (or fallback probe via `NOW_START`/`NOW_STOP`) |
| How does the STM32 send ESP-NOW commands to CD3? | `M1_RPC_NOW_*` messages `0x0600..0x0605` (already implemented in CD3) |
| How does the STM32 send ESP-NOW commands to AT FW? | `AT+M1ESPNOW=...` custom AT command (future) |
| How does the STM32 send ESP-NOW commands to SiN360? | Not supported; SiN360 won't set `M1_ESP32_CAP_ESPNOW` |
| What if the user has a firmware without ESP-NOW? | `m1_esp32_require_cap()` shows "Feature not supported" screen |
| Can we add ESP-NOW without touching the ESP32 FW? | CD3 already has it — STM32 side only needs the HAL glue |
| Does this block other ESP32 features? | No — ESP-NOW coexists with WiFi STA/AP on ESP32-C6 |
| Does this affect the SPI transport? | No — same transport, existing msg_ids |

---

## 11. Implementation Phases

### Phase 1: Capability Infrastructure (STM32 only)
- Add `M1_ESP32_CAP_ESPNOW` bit (21) to `m1_esp32_caps.h`
- Add `ESP32_FEATURE_ESPNOW` to `esp32_feature_map.h/.c`
- Add host tests for the new feature map entry
- Implement fallback capability probe (`NOW_START`/`NOW_STOP`) in
  `m1_esp32_caps.c` for CD3 builds that don't yet self-report the bit
- Stub `m1_espnow_hal.c/h` with `m1_esp32_require_cap()` gate

### Phase 2: Pure-Logic Protocol Modules (STM32, host-testable)
- `espnow_peer_session.c/h` — state machine with full host test coverage
- `espnow_file_transfer.c/h` — streaming-to-SD transfer + CRC32 + ARQ
- `espnow_tictactoe.c/h` — game logic

### Phase 3: UI Scenes (STM32)
- Main menu entry under a suitable top-level module (WiFi or new "Peer" category)
- Scan/discovery, pairing, transfer progress, Tic-Tac-Toe scenes

### Phase 4: ESP32 Firmware Coordination
- CD3 (`bedge117/m1-esp32-brain`): already implements `M1_RPC_NOW_*` handlers —
  only needs to add `M1_CAP_ESPNOW` to `M1_FW_CAPS` for self-reporting
- CD3-AT: implement `AT+M1ESPNOW` command handler (optional, lower priority)

### Phase 5: Integration & Testing
- End-to-end testing with two M1 devices
- Range / reliability characterisation
- UX polish (animations, error handling, timeout screens)

---

## 12. Design Decisions (resolved)

1. **Encryption**: No encryption.  Defer to the CD3 reference implementation
   which uses `encrypt = false` for all ESP-NOW peers.  This is acceptable for
   local device-to-device transfers; visual confirmation codes provide MITM
   awareness.  If a future CD3 release enables CCMP encryption and the standard
   is well-defined, we will support it — but we do not require it.

2. **Channel coordination**: No public protocol-level standard exists for
   ESP-NOW channel negotiation.  Our standard: the **acknowledging device
   (responder) always hops** to the initiator's channel.  If the responder does
   not hop, it is treated as a declined connection.

3. **Concurrent WiFi**: Resolved by decision #2.  The responder hops
   transparently when accepting the peer link — no user prompt for WiFi
   disconnection is needed.  WiFi STA connectivity on the responder may
   temporarily degrade if the new channel differs from the AP channel (single-
   radio constraint), but this is acceptable since the user explicitly accepted.

4. **File size / RAM buffering**: Streaming-to-SD is the only supported mode.
   Given the unified FreeRTOS heap-4 architecture (#526), buffering entire files
   in RAM is unacceptable.  CRC32 is accumulated incrementally as chunks arrive;
   the HAL ops abstraction (`file_open`/`file_write`/`file_close`) keeps FatFS
   out of the pure-logic module.  No practical file-size limit.

5. **Game extensibility**: The `game_id` field allows future games beyond
   Tic-Tac-Toe.  Keep it simple with per-game message types for now; the 1-byte
   game_id prefix is sufficient routing for the foreseeable future.

---

## 13. References

- ESP-IDF ESP-NOW API: https://docs.espressif.com/projects/esp-idf/en/latest/esp32c6/api-reference/network/esp_now.html
- CD3 ESP-NOW component: https://github.com/bedge117/m1-esp32-brain/tree/main/components/esp_now_link
- CD3 M1_RPC header: `components/m1_rpc/include/m1_rpc.h` (msg_ids `0x0600..0x0605`)
- M1 ESP32 capability system: `m1_csrc/m1_esp32_caps.h`
- M1_RPC protocol: `documentation/esp32_firmware.md` §M1_RPC
- ESP32 feature map: `m1_csrc/esp32_feature_map.h/.c`
- Heap memory strategy: hapaxx11/M1#526
- Firmware testing skill: `.github/skills/firmware-testing/SKILL.md`
- ESP32 coprocessor skill: `.github/skills/esp32-coprocessor/SKILL.md`
