# ESP32 CD3 Compatibility — Root-Cause Review & Resolution Plan (Issue #719)

> **Status:** Planning only. This document does **not** change firmware behaviour.
> It captures a thorough review of the 7 prior fix attempts, narrows the current
> failure, enumerates the remaining candidate root causes, and proposes a
> staged, testable resolution. Implementation will be directed from here.

---

## 1. Symptom & what it definitively tells us

The Settings Dashboard (added in #712) reports **`ESP32 Unknown (fallback)`** and
every ESP32 feature fails:

- WiFi Scan/Connect → "AP Scan Failed please try again"
- Recon Beacon Sniff → "Not supported by Unknown (fallback)"
- Attack Deauth → "No Targets found"

These are all downstream of one fact: **the cached capability bitmap is `0`.**

### 1.1 The "Unknown (fallback)" string is a precise diagnostic

Reading `m1_esp32_caps_init()` (`m1_csrc/m1_esp32_caps.c`), the string
`"Unknown (fallback)"` is written at exactly **one** place — the final
fail-closed block (`s_bitmap = 0u`) reached only after *every* probe fails.
Crucially, the intermediate outcomes cache **different** names/bitmaps:

| Outcome | Cached name | Cached bitmap |
|---|---|---|
| Binary `CMD_GET_STATUS` parsed | `fw_name` from payload | real bits |
| `CMD_PING` "PONG" only | `"SiN360 (via PING)"` | `PROFILE_SIN360` |
| `AT+CMD?` valid | `"AT (probed)"` | parsed AT bits |
| **M1_RPC PING ok + GET_STATUS parsed** | `"m1-native X.Y.Z"` | real bits |
| **M1_RPC PING ok + GET_STATUS fails** | `"CD3 (via M1_RPC)"` | `PROFILE_CD3` |
| **all probes fail** | **`"Unknown (fallback)"`** | **`0`** |

Because the dashboard shows **`Unknown (fallback)`** and **not**
`"Scan WiFi for ESP32 info"** (the un-probed state) nor `"CD3 (via M1_RPC)"`:

- `m1_esp32_caps_init()` **ran to completion and cached** (so the SPI HAL was up
  and the early `return`s at the init-status gate and the `should_run_at_probe`
  gate were **not** taken — the host AT task *did* start).
- The binary `CMD_PING`/`CMD_GET_STATUS` probes failed (not SiN360).
- The `AT+CMD?` probe failed (not an AT variant).
- **The M1_RPC `SYS_PING` frame never validated.** A *successful* PING with a
  failed GET_STATUS would have cached `PROFILE_CD3`, not zero.

**Conclusion:** the remaining defect lives in the **brain-CD3 M1_RPC PING path**
— either the 512-byte full-duplex "M1 Link" transport, or the PING frame
format/validation — **not** in feature-level decode, capability classification,
or the qMonstatek version string (all addressed by earlier PRs, all downstream
of a PING that must succeed first).

This is the seventh attempt, so we treat "there may be more than one cause"
seriously: §4 lists every candidate that can independently zero the PING, ranked
by how well it fits the evidence above.

---

## 2. History of the 7 prior attempts (what is already ruled in/out)

| PR | Fix | Layer | Still relevant to "Unknown"? |
|---|---|---|---|
| #684 | Built M1_RPC layer; `esp32_firmware_transport()`; discriminator `HANDSHAKE && OTA`; half-duplex transport | classification + transport | superseded |
| #686 | Discriminator → `HANDSHAKE && (802154_TX \|\| BLE_SPAM)`; full-duplex 512-B "M1 Link" transport (`spi_m1link_send_recv_bin`) | classification + transport | **transport is the live path** |
| #688 | WiFi scan AP count 1-byte→2-byte LE | feature decode | downstream of PING — not the cause |
| #690 | CRC-16 table (46 entries) in `m1_rpc.c` | **qMonstatek USB** protocol | unrelated to ESP32 SPI CRC (which is inline `m1_esp32_rpc_crc16`) |
| #705 | `SYS_GET_FW_VERSION` semver string; poll budget scales with timeout; HANDSHAKE yield + abort self-heal | qMonstatek string + transport pacing | version string is downstream of PING |
| #712 | Dashboard shows detected ESP32 fw | diagnostic UI | this is our instrument |
| #717 | `HAL_SPI_Abort` before each M1 Link xfer; `m1link_parse_frame()` scans buffer for RPC magic | transport FIFO/alignment | **most recent; did not resolve** |

Key takeaways:
- The **transport (#686/#705/#717)** is where every unresolved attempt has landed.
- #717's two mitigations (pre-transaction `HAL_SPI_Abort`, magic-scan parse) did
  not clear the failure on hardware, so either the abort does not actually flush
  STM32H5 SPI3, or the corruption/failure is not (only) a byte-shift, or the PING
  never gets a reply at all (a transport/handshake/config problem rather than a
  parse problem).

---

## 3. Current detection flow (as shipped after #717)

`m1_esp32_caps_init()` probes in this fixed order:

1. **Gate:** `if (!m1_esp32_get_init_status()) return;` (no cache) — SPI HAL up?
2. **Probe 0:** binary `CMD_PING` (half-duplex `spi_AT_send_recv_bin`) → SiN360?
3. **Probe 1:** binary `CMD_GET_STATUS` (half-duplex) → binary-report firmware?
4. **Start host AT task:** `esp32_main_init()` (creates `spi_trans_control_task`),
   then `should_run_at_probe()` — **`return` without caching if it fails to start.**
5. **Probe 2a:** `AT\r\n` presence (half-duplex `spi_AT_send_recv`); no "OK" →
   `goto probe_cd3`.
6. **Probe 2b:** `AT+CMD?` (half-duplex) → AT variant?
7. **Probe 3 (`probe_cd3`):** M1_RPC `SYS_PING` then `SYS_GET_STATUS` over the
   **full-duplex 512-B M1 Link** (`spi_m1link_send_recv_bin`).
8. **Fallback:** `s_bitmap = 0; fw = "Unknown (fallback)"`.

The brain-CD3 device reaches **step 7**, the M1_RPC PING fails, and it lands in
**step 8**. Two structural properties of this flow are themselves suspect:

- **The host AT RTOS task is started (step 4) *before* the brain is probed
  (step 7).** #686 explicitly designed the M1 Link transport to *not* rely on
  that task — yet the current flow guarantees the task is alive and has pumped at
  least one half-duplex transaction on SPI3 by the time the full-duplex PING runs.
- **Four half-duplex transactions precede the first full-duplex frame**, each a
  chance to leave SPI3 FIFO/packing state that #717 tries (and may fail) to undo.

---

## 4. Candidate root causes (ranked, each independently able to zero the PING)

### C1 — SPI3 bus contention between the host AT task and the M1 Link probe **(highest fit)**

- `esp32_main_init()` (step 4) creates `spi_trans_control_task`, which drives
  SPI3 under `spi_mutex_lock()` and is woken by the `AT\r\n` presence probe.
- **`m1link_hal_xfer()` (the full-duplex transfer) does NOT take `spi_mutex`.**
  Confirmed: `spi_mutex_lock/unlock` are used by the AT/SiN360 paths but never by
  the M1 Link path in `esp_app_main.c`.
- If the AT task is mid-/post-transaction (or its interrupt/handshake handling is
  still settling) when the PING's `HAL_SPI_TransmitReceive` runs, the two writers
  corrupt each other's framing on the shared peripheral — exactly producing a
  PING that never validates, on a device that is otherwise healthy.
- **Fits the evidence:** consistent with "reached step 7, PING failed, cached
  Unknown" and with the observation that starting the AT task is unnecessary for
  a brain that speaks only M1_RPC.
- **Confirm:** instrument whether `spi_trans_control_task` is RUNNING/holding the
  mutex during the PING; try probing the brain *before* `esp32_main_init()`.
- **Fix direction:** run **Probe 3 (M1_RPC) before starting the host AT task**
  (re-order: PING/GET_STATUS the brain first; only fall through to AT-task start
  + AT probes if the RPC probe fails), **and/or** make `m1link_hal_xfer()` take
  `spi_mutex_lock()` so it can never overlap the AT task.

### C2 — `HAL_SPI_Abort` does not actually flush STM32H5 SPI3 FIFO/packing

- #717 relies on `HAL_SPI_Abort(&hspi_esp)` to clear residue from the preceding
  half-duplex probes. On STM32H5 the SPI RX FIFO is not guaranteed clear by abort
  alone; recovery typically also requires disabling the peripheral (`CR1.SPE=0`),
  an explicit RX-FIFO drain, and/or re-init.
- If abort leaves packed/residual bytes, the first full-duplex frame is
  byte-shifted; #717's magic-scan parse only recovers when a *complete, valid*
  frame exists somewhere in the 512-byte window — a shift that also corrupts the
  slave's view (CS/clock misalignment) yields no valid frame at all.
- **Confirm:** log `hspi_esp` state + first 16 RX bytes of the PING response;
  check for a fixed byte offset or all-zero/all-0xFF RX.
- **Fix direction:** replace the lone `HAL_SPI_Abort` with a deterministic reset
  (`__HAL_SPI_DISABLE` → drain RXFIFO → `HAL_SPI_Init` or documented H5 flush
  sequence) **or** avoid the mixed HD/full-duplex sequence entirely by probing
  the brain first (see C1 fix), so no HD residue exists.

### C3 — SPI3 configuration is wrong for a 512-byte full-duplex slave exchange

`hspi_esp` init (`m1_esp32_hal.c`) has several settings that are fine for the
short SiN360 command frames but questionable for brain M1 Link:

- `NSSPMode = SPI_NSS_PULSE_ENABLE` combined with `NSS = SPI_NSS_SOFT` is
  contradictory (SS-pulse is a hardware-NSS feature; CS is bit-banged on PB10).
  On H5 this can still influence the master state machine.
- `FifoThreshold = SPI_FIFO_THRESHOLD_01DATA` with a 512-byte transfer and 8-bit
  data is an unusual pairing for a large TransmitReceive.
- **SPI mode:** configured `CPOL=LOW, CPHA=2EDGE` = **Mode 1**. Must be verified
  against the *brain* firmware's `spi_slave` config (the skill says Mode 1, but
  the brain is a different `spi_slave`, not `spi_slave_hd`, so re-verify).
- `BaudRatePrescaler = 16`: verify the resulting SCLK against the brain's stated
  safe ceiling (prior notes cite ~4.7 MHz stable). A too-fast clock silently
  corrupts every transaction.
- **Confirm:** compare each field against the shipped `m1-esp32-brain` slave
  config; scope/logic-analyze CS/SCLK/MISO on a PING if hardware access allows.
- **Fix direction:** align mode/threshold/NSS/clock with the brain slave; if the
  brain needs a different mode than SiN360, apply a per-transport reconfigure
  around the M1 Link xfer.

### C4 — M1_RPC PING frame format mismatch vs the *currently shipped* brain firmware

- Host constants: magic `M1_ESP32_RPC_MAGIC = 0x4D31`, version
  `M1_ESP32_RPC_VERSION = 0x01`, `SYS_PING = 0x0001`, `SYS_GET_STATUS = 0x0002`,
  CRC-16/CCITT (poly `0x1021`, init `0xFFFF`), 512-byte MTU, pipelined reply.
- These are mirrored from `bedge117/m1-esp32-brain`'s `m1_rpc.h`. If the shipped
  release the owner flashed has since bumped the **protocol version**, changed an
  **opcode**, changed the **CRC init/variant**, or uses a **different MTU /
  non-pipelined reply**, every PING frame fails validation → Unknown.
- **Confirm:** check out the exact brain release tag the owner flashed and
  diff `m1_rpc.h` (magic/version/opcodes/CRC/MTU) against `m1_esp32_caps.h` +
  `m1_esp32_rpc.h`. Verify the host CRC helper against a known brain test vector.
- **Fix direction:** re-sync the constants/opcode map with the flashed release;
  add a host unit test that CRCs a captured real brain PING/GET_STATUS frame.

### C5 — HANDSHAKE (PD7) semantics differ for the brain slave, so the master never clocks

- `m1link_wait_handshake()` waits for PD7 **high** ("armed/ready") before each
  clock, bounded by 100 ms, then clocks best-effort anyway.
- The brain pipelines replies onto a *later* transaction and toggles HANDSHAKE on
  its own cadence. If the polarity/meaning is inverted or the brain deasserts
  between the request and the reply, the master may clock at the wrong time and
  never capture a valid reply within the poll budget.
- **Confirm:** log PD7 transitions around a PING; compare against the brain's
  HANDSHAKE contract in its source/README.
- **Fix direction:** align HANDSHAKE polarity/edge and the request→reply poll
  pacing with the brain contract (mirror the proven C3 `m1_link` master timing).

### C6 — Classification discriminator (secondary; only bites *after* PING works)

- Even once PING/GET_STATUS succeed, `esp32_firmware_is_cd3()` requires
  `HANDSHAKE && (802154_TX || BLE_SPAM)`. If the flashed brain's real
  `M1_FW_CAPS` omits both `802154_TX` and `BLE_SPAM`, transport resolves to
  `AT` and features break again — but the name would be `"m1-native …"`/
  `"CD3 …"`, **not** `Unknown`. So this is **not** today's cause, but it is the
  *next* failure to pre-empt.
- **Confirm/Fix:** verify the flashed brain's capability bitmap; if needed,
  broaden the discriminator (e.g. key off HANDSHAKE + any RPC-only bit actually
  advertised, or off the `fw_name`/PING success itself).

---

## 5. Why we cannot resolve this blind — diagnostics come first

Seven code-only attempts have missed because the failure is at a hardware/wire
boundary that host unit tests cannot observe. The plan therefore **front-loads
on-device instrumentation** before any further "fix in the dark."

**Phase 0 — Instrument (no behaviour change to features):**

1. Add temporary, gated diagnostic logging to `m1_esp32_caps_init()` recording,
   for each probe: taken/skipped, return code, and (for the M1_RPC PING) the
   first N RX bytes, detected magic offset (if any), CRC computed vs received,
   and whether `spi_trans_control_task` was running / mutex held.
2. Surface a one-line "last ESP32 probe result" on the dashboard (extends #712)
   so the owner can read it back without a debugger.
3. Capture the **exact brain release** the owner flashed (version + git hash via
   the boot log or `SYS_GET_FW_VERSION`).

The captured RX pattern discriminates the causes directly:

| Observation | Points to |
|---|---|
| PING RX all `0x00`/`0xFF`, HANDSHAKE never asserts | C3 (config/clock) or C5 (handshake), or slave not running |
| Valid magic at a fixed non-zero offset | C2 (FIFO shift) — abort insufficient |
| Valid frame but CRC mismatch | C4 (CRC/version) or C3 (clock corruption) |
| Intermittent success; fails only when AT task active | **C1 (contention)** |
| `802154_TX`/`BLE_SPAM` absent once PING works | C6 (discriminator) |

> **Phase 0 implementation status (delivered).** `m1_esp32_caps_init()` now
> records a `m1_esp32_caps_diag_t` snapshot for every probe run (outcome stage,
> `bin_ping`/`bin_status`/`at_presence`/`at_cmd` flags, `at_task_before/after`,
> and the M1_RPC PING `rc`/`rxlen`/ok flags), exposed via
> `m1_esp32_caps_get_diag()` and rendered by the pure, host-tested
> `m1_esp32_caps_diag_format()`. A new **Dashboard page 4/4** (Settings >
> Dashboard) shows the firmware name, the formatted probe result (e.g.
> `FAIL rc-1 n0 at1`), the resolved `caps` bitmap, and the AT-task state —
> directly reading out the **C1 contention** signal (`at1` = host AT task was up
> at PING time) and the "no reply vs corrupted reply" distinction (`n0`).
> **Known limitation:** capturing the *raw* PING RX bytes / magic offset / CRC
> (needed to fully disambiguate C2 vs C4) requires a small transport hook in
> `spi_m1link_send_recv_bin()` to surface the scratch buffer on failure; that is
> deferred to Phase 1 so this diagnostic phase stays behaviour-neutral. The
> `SYS_GET_FW_VERSION` capture (item 3) already exists from #705 and is shown as
> part of the firmware name on the new page once a PING succeeds.

---

## 6. Proposed resolution phases (direct implementation from here)

- **Phase 0 – Diagnostics (above).** Land instrumentation, read back one real
  probe result + the flashed brain version. *Docs/diagnostic only; no feature
  behaviour change.*
- **Phase 1 – Eliminate contention & mixed-transport residue (C1/C2).** Re-order
  detection so the **M1_RPC brain probe runs before `esp32_main_init()`** and the
  AT probes; only start the host AT task if the RPC probe fails. Make
  `m1link_hal_xfer()` hold `spi_mutex`. This removes both the contention and the
  HD→full-duplex FIFO-residue window in one structural change.

  > **Phase 1 implementation status (delivered).** `m1_esp32_caps_init()` now
  > issues the M1_RPC `SYS_PING`/`SYS_GET_STATUS` probe immediately after the
  > SiN360 binary probes and *before* `esp32_main_init()` runs; the host AT
  > task is only started (and the AT presence / `AT+CMD?` probes only run) if
  > the RPC PING did not validate. `m1link_hal_xfer()` now takes the same
  > shared `pxMutex` the AT task's `spi_trans_control_task` uses, guarded so
  > it is a no-op before the AT task has ever been created (the mutex does
  > not exist yet) — this also covers the field-observed case where the AT
  > task was already running *before* `m1_esp32_caps_init()` ever ran (its own
  > diagnostics reported `ATtask b1 a1`), which the re-ordering alone cannot
  > prevent since caps init did not start that task. Regression coverage:
  > `tests/test_esp32_probe_ordering.c` (source-invariant checks, following
  > the same pattern as `test_esp32_main_deinit_releases_legacy_task.c`, since
  > neither `m1_esp32_caps_init()` nor `m1link_hal_xfer()` have an injectable
  > seam for host execution).
- **Phase 2 – Harden the SPI reset & config (C2/C3).** If Phase 1 diagnostics
  still show shift/garbage, replace the lone `HAL_SPI_Abort` with a deterministic
  H5 disable/drain/re-init, and correct the `hspi_esp` fields (NSSP/threshold/
  mode/clock) for the brain slave.
- **Phase 3 – Re-sync the wire protocol (C4/C5).** Diff and align magic/version/
  opcodes/CRC/MTU/HANDSHAKE against the exact flashed brain release; add host
  tests over captured real frames.
- **Phase 4 – Pre-empt post-PING classification (C6).** Verify the live
  capability bitmap and, if required, broaden `esp32_firmware_is_cd3()` /
  transport selection so a correctly-probed brain always resolves to
  `ESP32_TRANSPORT_RPC`.

Each phase is independently shippable and gated behind a read-back on the
dashboard so we confirm the bitmap becomes non-zero before moving on.

---

## 7. Verification & regression tests

Per the repo's "bug fixes require a host-side regression test" rule, every phase
adds tests under `tests/`:

- **Transport ordering (Phase 1):** a host test asserting the detection state
  machine issues the M1_RPC probe before starting the AT task for a brain-like
  fixture, and that a successful PING short-circuits the AT probes.
- **Mutex/contention (Phase 1):** test (or documented on-target check) that the
  M1 Link xfer path acquires the SPI mutex.
- **FIFO/reset (Phase 2):** extend `tests/test_esp32_m1link.c` with the observed
  real shift/garbage pattern (from Phase 0 capture) and assert recovery.
- **Wire format (Phase 3):** CRC/parse tests over a **captured real** brain
  PING + GET_STATUS frame (not a synthetic one) in `tests/test_esp32_rpc.c` /
  `tests/test_esp32_caps.c`.
- **Classification (Phase 4):** `tests/test_esp32_feature_map.c` cases for the
  flashed brain's actual bitmap → `ESP32_TRANSPORT_RPC`.

**Definition of done:** the dashboard reports a versioned brain name (e.g.
`ESP32 m1-native X.Y.Z`) with a **non-zero** bitmap, and WiFi Scan, Beacon
Sniff, and Deauth all function on the owner's hardware.

---

## 8. Information needed from the owner / hardware

1. The **exact `m1-esp32-brain` release** flashed (tag + git hash, from the boot
   log or `SYS_GET_FW_VERSION`).
2. Whether a **logic analyzer / scope** on CS(PB10)/SCLK/MISO/HANDSHAKE(PD7) is
   available for one PING, or whether we rely solely on on-device RX logging.
3. Confirmation the brain firmware is actually **running** (its own boot/UART
   log), to rule out "no slave" from "misframed slave."
4. Whether the same board ever worked with **SiN360 or AT** firmware (isolates
   host SPI wiring/health from brain-specific issues).

---

## 9. Open questions

- Does `HAL_SPI_Abort` clear SPI3's RX FIFO on STM32H573 in practice, or is an
  explicit disable/drain required? (Drives C2.)
- Is the brain's `spi_slave` SPI mode identical to SiN360's, or does the brain
  need a per-transport reconfigure? (Drives C3.)
- Has the shipped brain protocol (`m1_rpc.h`) drifted from the constants mirrored
  into `m1_esp32_caps.h`/`m1_esp32_rpc.h`? (Drives C4.)
