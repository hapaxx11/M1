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

## 2. Architecture Constraints

### 2.1 Firmware-Agnostic Capability Gate

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

### 2.2 Transport Independence

The STM32 host must not care which protocol variant the ESP32 speaks.  The
ESP-NOW commands MUST be deliverable over:

| Firmware | Transport | Path |
|----------|-----------|------|
| CD3 (native RPC) | M1_RPC binary frames | New msg_id range `0x0060..0x006F` |
| CD3-AT / dag T-800 | AT text commands | `AT+M1ESPNOW=...` custom command |
| SiN360 | Binary SPI opcodes | New CMD_ESPNOW_* opcodes (if SiN360 adds support) |

The STM32-side implementation uses a thin HAL-layer function
(`m1_espnow_send_cmd()`) that internally dispatches to the correct transport
based on the detected firmware class, mirroring the pattern used by WiFi attack
functions (`m1_wifi.c` checks `m1_esp32_has_cap(M1_ESP32_CAP_WIFI_JOIN)` and
switches between AT and binary paths).

### 2.3 64-Byte SPI MTU Constraint

The current SPI-HD transport uses a fixed 64-byte MTU per transaction.  An
ESP-NOW data frame can carry up to 250 bytes.  For file transfer payloads
exceeding 64 bytes, the design uses a chunked RPC pattern (similar to OTA):

- Host sends `ESPNOW_TX_DATA` with a chunk index + up to 48 bytes of payload
  per SPI transaction (64 − 8-byte M1_RPC header − 2-byte CRC16 − 6 reserved).
- ESP32 firmware buffers chunks and transmits the full ESP-NOW frame once all
  chunks for a logical message arrive.
- Received ESP-NOW frames are delivered to the STM32 via unsolicited
  `ESPNOW_RX_DATA` responses (same chunking in reverse).

### 2.4 Idle Power-Off Interaction

The ESP32 auto powers off after 60s idle (esp32_idle.c state machine).  While
an ESP-NOW session is active, the idle timer must be suppressed.  The peer-link
scene will call `esp32_idle_activity()` (or equivalent) to keep the ESP32
powered during active peer sessions.

---

## 3. New M1_RPC Message IDs (CD3 Path)

Reserved in the `0x0060..0x006F` range (peer-link namespace):

| msg_id | Name | Direction | Payload |
|--------|------|-----------|---------|
| `0x0060` | `ESPNOW_INIT` | REQ→ESP | channel (1B), encryption (1B), local_name[16] |
| `0x0061` | `ESPNOW_DEINIT` | REQ→ESP | none |
| `0x0062` | `ESPNOW_BROADCAST` | REQ→ESP | data[0..48] — peer discovery beacon |
| `0x0063` | `ESPNOW_ADD_PEER` | REQ→ESP | mac[6], channel(1), encrypt(1) |
| `0x0064` | `ESPNOW_DEL_PEER` | REQ→ESP | mac[6] |
| `0x0065` | `ESPNOW_TX_DATA` | REQ→ESP | peer_mac[6], chunk_idx(1), chunk_total(1), data[0..48] |
| `0x0066` | `ESPNOW_TX_STATUS` | RESP←ESP | peer_mac[6], status(1) — delivery ACK/NAK |
| `0x0067` | `ESPNOW_RX_DATA` | UNSOL←ESP | src_mac[6], chunk_idx(1), chunk_total(1), data[0..48] |
| `0x0068` | `ESPNOW_SCAN_START` | REQ→ESP | channel(1), duration_ms(2 LE) |
| `0x0069` | `ESPNOW_SCAN_RESULT` | UNSOL←ESP | src_mac[6], rssi(1), name[16] |
| `0x006A` | `ESPNOW_SCAN_STOP` | REQ→ESP | none |

### AT Fallback (CD3-AT / dag)

```
AT+M1ESPNOW=INIT,<channel>,<encrypt>,<name>
AT+M1ESPNOW=DEINIT
AT+M1ESPNOW=BROADCAST,<hex_data>
AT+M1ESPNOW=ADDPEER,<mac>,<channel>,<encrypt>
AT+M1ESPNOW=DELPEER,<mac>
AT+M1ESPNOW=TX,<mac>,<hex_data>
AT+M1ESPNOW=SCAN,<channel>,<duration_ms>
AT+M1ESPNOW=SCANSTOP

Unsolicited responses:
+M1ESPNOW:RX,<src_mac>,<hex_data>
+M1ESPNOW:TX_STATUS,<peer_mac>,<OK|FAIL>
+M1ESPNOW:PEER,<src_mac>,<rssi>,<name>
```

---

## 4. Peer Discovery & Handshake Protocol

### 4.1 Discovery Phase

1. Device A enters "Peer Link" scene → calls `ESPNOW_INIT` then
   `ESPNOW_SCAN_START` → listens for broadcast beacons.
2. Device A also sends periodic `ESPNOW_BROADCAST` beacons containing:
   - Magic: `"M1PL"` (4 bytes)
   - Protocol version: 1 (1 byte)
   - Device name: user-configurable, up to 16 chars (16 bytes)
   - Session nonce: random 4 bytes (replay protection)
3. Both devices see each other's beacons via `ESPNOW_SCAN_RESULT`.
4. User selects a discovered peer from the list → initiates pairing.

### 4.2 Pairing Confirmation

1. Device A sends a unicast `PAIR_REQUEST` (via `ESPNOW_TX_DATA`) to Device B.
2. Device B displays "Pair with <name>?" confirmation screen.
3. Device B responds with `PAIR_ACCEPT` or `PAIR_REJECT`.
4. On `PAIR_ACCEPT`, both devices call `ESPNOW_ADD_PEER` with the other's MAC.
5. Both devices show a 4-digit visual confirmation code derived from
   `SHA256(nonce_A || nonce_B || mac_A || mac_B)[0:2]` — user confirms they
   match (MITM protection for encrypted sessions).

### 4.3 Session State Machine (STM32 side, pure logic)

```
IDLE → SCANNING → PEER_FOUND → PAIR_SENT → PAIRED → (FILE_TX | GAME | ...) → IDLE
                                          ↓
                                    PAIR_REJECTED → SCANNING
```

This state machine will be implemented as a pure-logic module
(`espnow_peer_session.c/h`) under `m1_csrc/`, host-testable with no HAL deps.

---

## 5. CRC32 File Transfer Protocol

### 5.1 Transfer Framing

Built on top of the ESP-NOW peer link session (section 4):

| Byte offset | Size | Field |
|-------------|------|-------|
| 0 | 1 | Message type: `FILE_OFFER=0x10`, `FILE_ACCEPT=0x11`, `FILE_REJECT=0x12`, `FILE_DATA=0x13`, `FILE_ACK=0x14`, `FILE_COMPLETE=0x15`, `FILE_ABORT=0x16` |
| 1 | 1 | Sequence number (wraps at 255) |
| 2..N | varies | Type-specific payload |

### 5.2 Transfer Flow

1. **Sender** sends `FILE_OFFER`: filename[32] + file_size(4 LE) + crc32(4 LE) + chunk_size(1).
2. **Receiver** displays "Accept <filename> (<size> bytes)?" → responds `FILE_ACCEPT` or `FILE_REJECT`.
3. **Sender** streams `FILE_DATA` chunks: seq(1) + offset(4 LE) + data[0..200].
   Each chunk is individually ACK'd by the receiver (`FILE_ACK` with matching seq).
4. After all chunks sent, sender sends `FILE_COMPLETE`.
5. Receiver verifies CRC32 over the reassembled file:
   - Match → saves to SD card, shows success screen.
   - Mismatch → sends `FILE_ABORT` with error code, sender retries or aborts.

### 5.3 Flow Control

- Window size = 1 (stop-and-wait ARQ) for simplicity in v1.
- Timeout: 500 ms per chunk ACK; 3 retries before abort.
- Maximum file size: 256 KB (limited by receiver RAM buffering strategy —
  writes directly to SD card via FatFS in streaming mode for larger files,
  no full-file RAM buffer needed).

### 5.4 Pure-Logic Module

`espnow_file_transfer.c/h` — state machine + CRC32 accumulation + chunk
reassembly.  Host-testable.  The HAL boundary is a small
`espnow_ft_hal_ops_t` struct with function pointers for:
- `send(mac, data, len)` — transmit an ESP-NOW frame
- `file_open(path, mode)` / `file_write(buf, len)` / `file_close()` — SD access
- `millis()` — timeout source

---

## 6. Peer Tic-Tac-Toe Protocol

### 6.1 Game Messages (over ESP-NOW unicast)

| Type | Value | Payload |
|------|-------|---------|
| `GAME_INVITE` | `0x20` | game_id(1) = TTT(0x01) |
| `GAME_ACCEPT` | `0x21` | game_id(1) |
| `GAME_REJECT` | `0x22` | game_id(1) |
| `GAME_MOVE` | `0x23` | game_id(1), cell(1) [0-8, row-major] |
| `GAME_STATE` | `0x24` | game_id(1), board[9] (0=empty, 1=X, 2=O), turn(1) |
| `GAME_END` | `0x25` | game_id(1), result(1) [0=draw, 1=host_win, 2=peer_win] |
| `GAME_QUIT` | `0x26` | game_id(1) |

### 6.2 Pure-Logic Core

`espnow_tictactoe.c/h` — board validation, win detection, move application.
No display/HAL deps.  The scene (`m1_espnow_scene_tictactoe.c`) handles draw +
input, calling into the pure-logic core for state transitions.

---

## 7. STM32-Side Module Structure

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

## 8. Capability Bit & Feature Map Changes

### 8.1 New Capability Bit

```c
/* m1_esp32_caps.h — bit 21 */
/** ESP-NOW peer-to-peer communication (discovery, unicast, broadcast) */
#define M1_ESP32_CAP_ESPNOW         (UINT64_C(1) << 21)
```

### 8.2 Feature Map Addition

```c
/* esp32_feature_map.h */
/** ESP-NOW peer link (discovery + file transfer + games) */
ESP32_FEATURE_ESPNOW,

/* esp32_feature_map.c */
{ M1_ESP32_CAP_ESPNOW,  "ESP-NOW Peer Link" },
```

### 8.3 Profile Updates

- `M1_ESP32_CAP_PROFILE_CD3` will include `M1_ESP32_CAP_ESPNOW` once the CD3
  firmware implements the `0x0060..0x006F` range.
- AT firmware variants will gain the capability when they implement
  `AT+M1ESPNOW`.
- SiN360 profile is unlikely to add this (no planned development).

---

## 9. Multi-Firmware Support Summary

| Concern | Resolution |
|---------|-----------|
| How does the STM32 know if ESP-NOW is available? | `M1_ESP32_CAP_ESPNOW` bit in probed `cap_bitmap` |
| How does the STM32 send ESP-NOW commands to CD3? | M1_RPC messages `0x0060..0x006F` |
| How does the STM32 send ESP-NOW commands to AT FW? | `AT+M1ESPNOW=...` custom AT command |
| How does the STM32 send ESP-NOW commands to SiN360? | Not supported initially; SiN360 won't set `M1_ESP32_CAP_ESPNOW` |
| What if the user has a firmware without ESP-NOW? | `m1_esp32_require_cap()` shows "Feature not supported" screen |
| Can we add ESP-NOW without touching the ESP32 FW? | No — ESP-NOW is an ESP32 radio feature, requires ESP32-side code |
| Does this block other ESP32 features? | No — ESP-NOW coexists with WiFi STA/AP on ESP32-C6 |
| Does this affect the SPI transport? | No — same 64-byte MTU, same SPI-HD mode, new msg_ids only |

---

## 10. Implementation Phases

### Phase 1: Capability Infrastructure (STM32 only)
- Add `M1_ESP32_CAP_ESPNOW` bit (21) to `m1_esp32_caps.h`
- Add `ESP32_FEATURE_ESPNOW` to `esp32_feature_map.h/.c`
- Add host tests for the new feature map entry
- Stub `m1_espnow_hal.c/h` with `m1_esp32_require_cap()` gate

### Phase 2: Pure-Logic Protocol Modules (STM32, host-testable)
- `espnow_peer_session.c/h` — state machine with full host test coverage
- `espnow_file_transfer.c/h` — chunked transfer + CRC32 + ARQ
- `espnow_tictactoe.c/h` — game logic

### Phase 3: UI Scenes (STM32)
- Main menu entry under a suitable top-level module (WiFi or new "Peer" category)
- Scan/discovery, pairing, transfer progress, Tic-Tac-Toe scenes

### Phase 4: ESP32 Firmware Implementation
- CD3 (`bedge117/m1-esp32-brain`): implement `0x0060..0x006F` handlers using
  `esp_now_*()` ESP-IDF API
- CD3-AT: implement `AT+M1ESPNOW` command handler (optional, lower priority)
- Self-report `M1_ESP32_CAP_ESPNOW` in `cap_bitmap` / `AT+CMD?` listing

### Phase 5: Integration & Testing
- End-to-end testing with two M1 devices
- Range / reliability characterisation
- UX polish (animations, error handling, timeout screens)

---

## 11. Open Questions

1. **Encryption**: Should ESP-NOW encryption (CCMP) be mandatory for file
   transfer, or optional?  ESP-NOW supports per-peer PMK; the pairing
   handshake could exchange keys derived from the visual confirmation code.
2. **Channel coordination**: ESP-NOW operates on a single WiFi channel.  If
   both devices are on different channels at discovery time, one must hop.
   The broadcast beacon should include the sender's current channel.
3. **Concurrent WiFi**: ESP-NOW coexists with WiFi STA on ESP32-C6, but
   channel is locked to the STA channel when connected.  Should we require
   WiFi disconnection before peer link, or allow degraded same-channel-only
   mode?
4. **Maximum file size**: 256 KB RAM buffering vs. streaming-to-SD tradeoff.
   SD streaming is preferred for generality but adds FatFS dependency to the
   transfer state machine (resolved by the HAL ops abstraction).
5. **Game extensibility**: The `game_id` field allows future games beyond
   Tic-Tac-Toe.  Should the protocol support arbitrary game messages, or keep
   it simple with per-game message types?

---

## 12. References

- ESP-IDF ESP-NOW API: https://docs.espressif.com/projects/esp-idf/en/latest/esp32c6/api-reference/network/esp_now.html
- M1 ESP32 capability system: `m1_csrc/m1_esp32_caps.h`
- M1_RPC protocol: `documentation/esp32_firmware.md` §M1_RPC
- ESP32 feature map: `m1_csrc/esp32_feature_map.h/.c`
- Firmware testing skill: `.github/skills/firmware-testing/SKILL.md`
- ESP32 coprocessor skill: `.github/skills/esp32-coprocessor/SKILL.md`
