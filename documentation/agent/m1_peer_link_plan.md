# M1↔M1 Peer Link — Phased Implementation Plan (ESP-NOW compatibility layer)

> **Status:** Phases 0–4 implemented as pure-logic, host-tested STM32-side
> modules (see `m1_csrc/espnow_{chunk,shareable,message,trigger,crypto}.c`
> and their `tests/test_espnow_*` suites). Scene/UI wiring and live bench
> validation remain, and depend on the coordinated `bedge117/m1-esp32-brain`
> changes noted in §4. This document is retained for direction by @hapaxx11.
>
> **Origin:** The forks tracker lists **M1↔M1 peer link** (dag `M1_T-1000`
> v0.3.0, `m1_link.c/h` + `m1_link_app.c`, ~2600 lines) as *rejected/deferred*.
> dag's implementation is **915 MHz FSK** on the SI4463 Sub-GHz radio, wired to a
> divergent `AT+M1LINK` command on dag's ESP32 AT firmware. It was rejected as a
> cherry-pick because it is architecturally entangled with that AT command and
> radio path. This plan re-scopes the same *user-facing* feature onto Hapax's own
> transport — **ESP-NOW (2.4 GHz) over the CD3 brain `M1_RPC` compatibility
> layer** — which is a cleaner fit and, crucially, **already largely exists**.

---

## 1. Why ESP-NOW instead of 915 MHz FSK

| Aspect | dag (rejected) | Hapax (this plan) |
|--------|----------------|-------------------|
| Radio | SI4463 Sub-GHz @ 915 MHz FSK | ESP32-C6 @ 2.4 GHz ESP-NOW |
| Control path | `AT+M1LINK` (dag AT firmware) | `M1_RPC_NOW_*` (CD3 brain firmware) |
| Coupling | Entangled with dag AT fork | Uses existing Hapax compat layer |
| Sub-GHz radio contention | Competes with Sub-GHz TX/RX + Read Raw | None — Sub-GHz radio stays free |
| Regulatory / range | 915 MHz ISM (region-dependent) | 2.4 GHz, broadly available |
| Host infrastructure | None in Hapax | **Already built** (see §2) |

Re-using ESP-NOW keeps the SI4463 fully available for its primary Sub-GHz
duties, avoids importing dag's AT command surface, and builds on code Hapax has
already written, reviewed, and host-tested.

---

## 2. What already exists in Hapax (baseline)

The ESP-NOW peer-link stack is **already partially shipped** and reachable from
**WiFi menu → "Peer Link"** (`m1_wifi_scene_menu.c:115,136`). Current pieces:

| Layer | File(s) | State |
|-------|---------|-------|
| RPC opcodes `0x0600..0x0605` | `m1_esp32_rpc.h:147-153` | START / STOP / ANNOUNCE / PEERS_GET / SEND / RECV_GET defined |
| Transport HAL | `m1_espnow_hal.c/.h` | start/stop, announce, poll peers, send, recv, MAC/channel, file adapter |
| RPC response parsing (pure) | `espnow_rpc_parse.c/.h` | PEERS_GET / RECV_GET decoders + host test |
| Peer discovery + pairing FSM (pure) | `espnow_peer_session.c/.h` | IDLE→SCANNING→PEER_FOUND→PAIR_SENT→PAIRED, 4-digit confirm code + host test |
| File transfer protocol (pure) | `espnow_file_transfer.c/.h` | Stop-and-wait ARQ, CRC32, streaming-to-SD + host test |
| Tic-Tac-Toe demo app (pure) | `espnow_tictactoe.c/.h` | Playable over the link + host test |
| Scenes | `m1_espnow_scene_main/scan/transfer/tictactoe.c` | Menu: **Scan Peers / Send File / Tic-Tac-Toe** |
| Capability gate | `M1_ESP32_CAP_ESPNOW` bit 24 (`m1_esp32_caps.h:222`) | Host-only bit; **not self-reported** by any shipped firmware yet |
| Host tests | `tests/test_espnow_{peer_session,file_transfer,rpc_parse,tictactoe}.c` | Passing |
| Reference doc | `documentation/esp32_firmware.md:761-819` | Wire format, opcode table, design decisions |

**Bottom line:** discovery, pairing, an integrity-checked file transfer, and a
game already work through the compat layer. The remaining work is (a) closing
functional gaps versus dag's feature set, and (b) the firmware/hardware
enablement needed to make it usable on a real device.

---

## 3. Gap analysis vs dag's five feature pillars

dag's peer link advertised: **peer messaging, AES-256 encryption, peer
discovery, capture sharing, remote trigger.**

| dag pillar | Hapax today | Gap to close |
|------------|-------------|--------------|
| **Peer discovery** | ✅ `espnow_peer_session` + Scan scene + confirm code | None (polish only) |
| **Capture sharing** | ⚠️ File transfer exists but the transfer scene is **receiver-only**; sender path is stubbed (`m1_espnow_scene_transfer.c:85-86` "future Phase 5 integration") | Add a **sender-side file browser** and a "Send to peer" action from saved Sub-GHz / NFC / RFID / IR items |
| **Peer messaging** | ❌ No text/chat app over the DATA channel | New short-message app + scene |
| **AES-256 encryption** | ❌ `encrypt = false` always (`esp32_firmware.md:808`); only visual confirm codes | Application-layer authenticated encryption over the DATA channel (see §5, Phase 4) |
| **Remote trigger** | ❌ None | New command app: ask a paired peer to replay/transmit a named saved capture (danger-gated) |

Two enablement blockers apply to **all** on-device use, regardless of pillar:

1. **Capability self-report.** `M1_ESP32_CAP_ESPNOW` (bit 24) is host-only and no
   shipped CD3 brain firmware sets it, so the feature gate *fails closed* on real
   hardware today (`m1_esp32_caps.h:216-222`, `esp32_firmware.md:802-805`). The
   brain firmware (`bedge117/m1-esp32-brain`, separate repo) must advertise the
   bit — or Hapax must add a dedicated `NOW`-ping fallback probe.
2. **Payload size.** The fixed 64-byte SPI-HD transaction caps ESP-NOW app data
   at **42 bytes per `NOW_SEND` call** (`esp32_firmware.md:795-800`); full 240-byte
   frames need multi-transaction RPC chunking that is **not yet implemented**.
   Messaging and encryption headers make this limit bite sooner.

---

## 4. Cross-repo / hardware dependencies (call out early)

This feature cannot be fully validated in the firmware repo alone:

- **Two physical M1 devices** (or one M1 + one XIAO/Pico ESP-NOW bench) are
  required to test discovery, pairing, transfer, messaging, and trigger.
- **CD3 brain ESP32 firmware** (`bedge117/m1-esp32-brain`) owns the over-the-air
  ESP-NOW behaviour and the `M1_ESP32_CAP_ESPNOW` self-report. Any new wire
  behaviour (chunking, encryption negotiation, a `NOW_TRIGGER` opcode) needs a
  coordinated change there. Per repo policy that is a **separate repository** and
  is out of scope for STM32-side commits — this plan assumes we either (a) drive
  the brain team, or (b) keep every new capability behind the closed feature gate
  until the brain supports it.
- Per `CLAUDE.md`, **pure-logic layers are host-tested**; hardware-coupled scene
  behaviour is bench-gated. Each phase below lists its host tests explicitly.

---

## 5. Phased plan

Each phase is independently shippable, host-testable where possible, and gated
so nothing breaks when the ESP-NOW capability bit is absent. Phases are ordered
so early phases unblock later ones.

### Phase 0 — De-risk & enablement (no user-visible feature)
- Decide the capability-detection strategy: brain self-reports bit 24, **or** add
  a Hapax-side `NOW_START`/ping fallback probe in `m1_esp32_caps_init()`.
- Specify (in `documentation/esp32_firmware.md`) the multi-transaction **RPC
  chunking** scheme for `NOW_SEND` / `NOW_RECV_GET` so payloads > 42 bytes work;
  implement the pure host-side chunk splitter/reassembler with tests. No brain
  change is required to *reassemble* on receive; sending large frames does need
  brain cooperation — document which side each part lands on.
- **Deliverable:** capability path decided + chunking helper (pure + tested).
- **Host tests:** chunk split/reassemble round-trip, boundary at 42/240 bytes.
- **Hardware gate:** none (pure logic); bench-confirm probe once brain is ready.

### Phase 1 — Finish capture sharing (sender side)
- Wire the **sender** half of the existing file-transfer protocol: a file
  browser to pick a saved item, plus a "Send to peer" action surfaced from saved
  Sub-GHz (`.sub`), NFC (`.nfc`), RFID (`.rfid`), and IR (`.ir`) item menus.
- Reuse `espnow_file_transfer.c` (already bidirectional at the protocol level) and
  the paired-peer MAC from `espnow_peer_session`.
- **Deliverable:** end-to-end save→send→receive→save of a capture between two M1s.
- **Host tests:** extend `test_espnow_file_transfer.c` for the sender FSM
  (offer/accept/data/ack/complete, retry, abort) if not already covered.
- **Hardware gate:** two-device transfer of a real `.sub`/`.nfc` file.

### Phase 2 — Peer messaging (short text)
- New pure message app (`espnow_message.c/.h`): compose/queue/receive short
  messages over the DATA channel, framed under a new app-layer type distinct
  from pairing (`espnow_peer_session.h:44-49`) and file transfer
  (`espnow_file_transfer.h:51-59`).
- New scene under the Peer Link menu ("Messages"), depends on Phase 0 chunking
  for messages longer than one SPI call.
- **Deliverable:** two paired M1s exchange short text messages.
- **Host tests:** `test_espnow_message.c` — framing, ordering, truncation,
  type-demux against pairing/FT/game types.
- **Hardware gate:** two-device chat.

### Phase 3 — Remote trigger (danger-gated)
- New pure command app: a paired M1 requests the peer replay/transmit a **named
  saved capture** it already holds. Explicit request/confirm/execute/result
  states; **no arbitrary remote code**, only "replay saved item X".
- Strong UX guard rails: opt-in "allow remote trigger" per session, on-device
  confirmation on the executing side, and clear on-screen indication of remote-
  initiated TX (legal/safety).
- May need a brain opcode (e.g. `NOW_*` app-type only — can ride the existing
  DATA channel, so possibly **no new RPC opcode** required).
- **Deliverable:** M1-A causes M1-B to replay a chosen saved Sub-GHz/IR capture.
- **Host tests:** `test_espnow_trigger.c` — request/confirm/deny/execute FSM,
  rejection when capability/consent absent, name validation.
- **Hardware gate:** two-device remote replay, with the confirm gate exercised.

### Phase 4 — Authenticated encryption (AES)
- Add application-layer authenticated encryption over the DATA channel keyed off
  the pairing exchange (the confirm code already proves a shared secret exists).
  Evaluate: (a) app-layer AES-GCM/CCM in Hapax pure code, vs (b) ESP-NOW native
  PMK/LMK CCMP once a CD3 release standardises it (`esp32_firmware.md:808-811`).
- Prefer app-layer so it is testable on host and independent of brain firmware;
  fold the AEAD header into the Phase 0 chunk format.
- **Deliverable:** messaging + transfer + trigger payloads are confidential and
  tamper-evident between paired peers.
- **Host tests:** `test_espnow_crypto.c` — encrypt/decrypt round-trip, tamper
  detection (auth-tag failure), replay/nonce handling, key-derivation vectors.
- **Hardware gate:** two-device encrypted session interop.

### Phase 5 — Polish, docs, changelog
- Menu/UX consolidation of the Peer Link submenu (Scan / Messages / Send Capture
  / Remote Trigger / Tic-Tac-Toe), consistent button bars and status.
- Update `documentation/esp32_firmware.md` ESP-NOW section (new app types, chunk
  format, encryption, trigger), the `esp32-coprocessor` skill, and the README
  ESP32 capability table if a cap count changes.
- Add `.changelog/*.added.md` fragments (never edit `CHANGELOG.md` directly).
- **Hardware gate:** full regression pass across all pillars on two devices.

---

## 6. Recommended sequencing & decision points for @hapaxx11

1. **Confirm transport choice** — ESP-NOW (this plan) vs also wanting a 915 MHz
   FSK SI4463 path for parity with dag. Recommendation: ESP-NOW only.
2. **Phase 0 first** — capability detection + chunking unblock everything else and
   are the main firmware/hardware coordination items.
3. **Then pick order** among Phases 1–4 by priority. Capture sharing (Phase 1)
   has the most existing code and the clearest payoff; encryption (Phase 4) is
   the biggest cross-repo dependency.
4. **Brain firmware coordination** — flag which pieces (self-report bit, large-
   frame `NOW_SEND`, any new opcode) need `bedge117/m1-esp32-brain` changes so
   they can be scheduled there.

Direct which phase(s) to implement and I'll proceed with real code + host tests
per the repo's build-and-test rules.
