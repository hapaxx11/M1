---
name: esp32-coprocessor
description: ESP32-C6 coprocessor reference: AT vs binary-SPI firmware variants, SPI transport, capability probing, memory footprint, and how to build/flash the ESP32 firmware. Load for any WiFi, Bluetooth, BLE, 802.15.4, or ESP32 AT-command work. Deep firmware detail lives in documentation/esp32_firmware.md.
---

# ESP32-C6 Coprocessor

> Extracted from CLAUDE.md. See also documentation/esp32_firmware.md for the
> full AT-command and firmware reference.

## ESP32-C6 Coprocessor

### Firmware Source

> **Two distinct ESP32 firmware variants are supported.  Choose based on which features you need.**

#### CD3-AT firmware (legacy AT-command — bedge117 / neddy299 / dag)
- **Source repo**: [`bedge117/esp32-at-monstatek-m1`](https://github.com/bedge117/esp32-at-monstatek-m1) (CD3-AT — not to be confused with the newer CD3 native `m1-esp32-brain` firmware below)
- **Deauth fork**: [`neddy299/esp32-at-monstatek-m1`](https://github.com/neddy299/esp32-at-monstatek-m1) (adds `AT+DEAUTH` + `AT+STASCAN`)
- **dag fork**: [`dagnazty/esp32-at-monstatek-m1`](https://github.com/dagnazty/esp32-at-monstatek-m1) (additional AT command extensions)
- All are forks of Espressif's official `esp-at`, customised for M1's SPI transport
- Pre-built binaries available on the GitHub Releases pages of each repo
- Enables: Bad-BT / BLE HID, 802.15.4 (Zigbee/Thread), WiFi connect (`AT+CWJAP`) + NTP sync (`AT+CIPSNTPCFG` / `AT+CIPSNTPTIME?`), and — on the neddy299 / dag forks — **WiFi attacks**: deauth (`AT+DEAUTH` on neddy299, `AT+M1DEAUTH` on dag) plus, on dag, beacon spam (`AT+M1BEACON`), karma (`AT+M1KARMA`), evil portal (`AT+M1EVILTWIN`), probe flood (`AT+M1PROBE`), and PMKID / handshake capture (`AT+M1PMKID` / `AT+M1HSCAP`)
- Does NOT support (AT firmware): WiFi packet sniffers / signal monitor, MAC tracker / wardrive, BLE wardrive / BLE sniffers, network scanners, station scan (currently binary-SPI-only in M1; no `AT+STASCAN` dispatch), BT device management
- See [`documentation/esp32_firmware.md`](../../../documentation/esp32_firmware.md) for the full reference

#### Binary SPI firmware (recommended — sincere360 / SiN360)
- **Source repo**: [`sincere360/M1_SiN360_ESP32`](https://github.com/sincere360/M1_SiN360_ESP32) (NimBLE binary SPI slave)
- Pre-built binaries on the Releases page (`factory_ESP32C6-SPI-XIAO.bin` + `.md5`)
- **Enables** (vs AT firmware): WiFi packet sniffers (All/Beacon/Probe/Deauth/EAPOL/SAE/Pwnagotchi), signal monitor, station scan, MAC tracker, wardrive, BLE wardrive (regular/continuous/Flock), BLE sniffers (Analyzer/Generic/Flipper/AirTag/Flock), BLE Spam (SourApple/SwiftPair/Samsung/Flipper/All/AirTag Spoof), BLE Detectors (Skimmers/Flock/Meta), WiFi attacks (Deauth/Beacon/Clone/Rickroll/Evil Portal/Probe Flood/Karma/Karma+Portal), network scanners (Ping/ARP/SSH/Telnet/Port scan), **Bad-BT/BLE HID** (v0.9.1.0+, via CMD_BLE_HID_START/STOP/STATUS/REPORT)
- **Disables** (vs AT firmware): WiFi connect/NTP sync (stub returns "not available"), 802.15.4 scan — AT-layer modules still compile and these features will be restored once the SiN360 ESP32 firmware gains equivalent support
- Uses a 64-byte binary SPI packet protocol (`m1_esp32_cmd.c/h`) — NOT AT text commands
- Handshake via HANDSHAKE pin (PD7) after each TX; CS on PB10 (ESP32_SPI3_NSS)
- Same SPI3 Mode 1 hardware as AT firmware — no hardware changes required to switch between variants

#### CD3 native binary RPC firmware (next-gen — bedge117/m1-esp32-brain)
- **Source repo**: [`bedge117/m1-esp32-brain`](https://github.com/bedge117/m1-esp32-brain) (native ESP-IDF, no AT stack)
- Pre-built binaries on the Releases page
- **Architecture**: **full-duplex `spi_slave`** (NOT ESP-AT `spi_slave_hd`), M1_RPC binary protocol (magic `0x4D31` "M1"), fixed **512-byte** (`M1_ESP32_M1LINK_MTU`) transactions in both directions, same GPIO/CS/HANDSHAKE pins as AT and SiN360 — no hardware changes required. The slave pipelines its reply onto a **later** transaction, so the host must poll with IDLE filler frames.
- **Enables**: Native ESP-IDF WiFi (scan, join, deauth, beacon, probe, karma, portal, pktmon, sta_scan), NimBLE (scan, adv, HID, GATT), 802.15.4, M1-to-M1 peer link over ESP-NOW, GPIO API
- **Reserved, NOT yet implemented in shipped releases** (verified against public source, 2026-07-21): **PMKID capture** (`M1_ESP32_CAP_PMKID` / `M1_RPC_OFF_PMKID_CAPTURE`) and **ESP32 OTA self-update** (`M1_ESP32_CAP_OTA` / `M1_RPC_SYS_OTA_BEGIN/DATA/END`) — message IDs exist in the shared `m1_rpc.h` but have no dispatch case in `main.c`, so they NAK with `ERR_UNSUPPORTED`. **WPA handshake/EAPOL capture** (`M1_ESP32_CAP_HANDSHAKE`) is dispatched and functional and is advertised by current brain firmware capability bitmaps.
- **Does NOT include** (v1): NETSCAN (no ping/ARP scanner), BT Classic management
- Detected by the M1 via **M1_RPC PING** in `m1_esp32_caps_init()`; capability bitmap reported via M1_RPC GET_STATUS
- Firmware identifier: `fw_name = "m1-native"` in the GET_STATUS response — a **bare identifier with no dotted version**. The real semver is fetched via the separate `M1_RPC SYS_GET_FW_VERSION` (0x0003) opcode and folded into the cached name as `"m1-native X.Y.Z[ <hash>]"` (`caps_cd3_fetch_fw_version()` + the pure `m1_esp32_rpc_format_fw_version()` helper). **This versioned string is required for qMonstatek compatibility**: the desktop app keys "compatible" off a parseable `X.Y.Z` in the device-info `esp32_version` (its `parseVerNums()`), so the bare `"m1-native"` reported as "incompatible firmware". Mirrors the C3 reference's `esp_fw_status_str()` ("m1_link X.Y.Z <hash>").
- **Conservative fallback profile**: `M1_ESP32_CAP_PROFILE_CD3` applied if GET_STATUS unavailable (early firmware)
- Discriminator in `esp32_feature_map.c`: `esp32_firmware_is_cd3()` — `HANDSHAKE` set AND a CD3-unique bit (`802154_TX` or `BLE_SPAM`) set. **Do NOT key off `OTA`** — the shipped brain firmware advertises `HANDSHAKE` but intentionally omits `OTA` (see the OTA/PMKID note above), so an `OTA`-gated discriminator misclassified every brain device as AT and broke all ESP32 features. The real `M1_FW_CAPS` (HANDSHAKE set, OTA clear, 802154_TX/BLE_SPAM set) now resolves to `ESP32_TRANSPORT_RPC`.

### Transport compatibility layer (host side)

> **Two CD3 firmwares exist — do not confuse them.** The legacy **CD3-AT** speaks
> AT text commands; the native **brain CD3** (`m1-esp32-brain`) speaks binary
> M1_RPC. Both are supported, on different transports.

The M1 host classifies the attached firmware into one of three wire transports
via `esp32_firmware_transport(cap_bitmap)` (`esp32_feature_map.c`), returning
`esp32_transport_t`:

| Firmware | Discriminator | Transport |
|----------|---------------|-----------|
| MtkCore (Legacy-SPI compat) | `MTKCORE` host-classifier bit (zero on-wire bitmap + dotted-semver `fw_name`) | `ESP32_TRANSPORT_RPC` |
| brain CD3 (`m1-esp32-brain`) | `HANDSHAKE && (802154_TX \|\| BLE_SPAM)` | `ESP32_TRANSPORT_RPC` |
| SiN360 | `BLE_HID && !WIFI_JOIN` | `ESP32_TRANSPORT_BINARY_SPI` |
| AT builds **incl. legacy CD3-AT** | any other non-zero bitmap | `ESP32_TRANSPORT_AT` |
| unknown / not probed | zero bitmap | `ESP32_TRANSPORT_NONE` |

- **MtkCore (`Monstatek/MonstaTek-Esp32-Core`) Legacy-SPI compatibility:** MtkCore's
  *"Legacy SPI Compatibility"* adapter speaks our exact m1_link wire protocol (magic
  `0x4D31`, 8-byte header, 512-byte full-duplex cells, CRC-16/CCITT-FALSE, flat 16-bit
  `msg_id`). Its legacy `GET_STATUS (0x0002)` returns a well-formed devstatus but
  **deliberately hard-codes `cap_bitmap` to all zeros** (the authors declined to guess
  legacy bit indices; real negotiation is over the canonical `GET_CAPABILITIES`, which
  the legacy adapter does not expose). Our host therefore fingerprints MtkCore as
  **`cap_bitmap == 0` AND a dotted-semver `fw_name` (e.g. `"0.8.1.0"`)** in
  `m1_esp32_caps_init()` (Probe 2), synthesises `M1_ESP32_CAP_PROFILE_MTKCORE`, and sets
  the host-only `M1_ESP32_CAP_MTKCORE` classifier bit so `esp32_firmware_transport()`
  routes to `ESP32_TRANSPORT_RPC` (not AT). Without this it misdetects as AT and every
  feature is gated off — same failure class as the brain-CD3 OTA-discriminator bug above.
  Cached `fw_name` is prefixed `"MtkCore <semver>"` for device-info display (the semver
  still satisfies qMonstatek's `parseVerNums()`).
  - **Supported over compat_spi:** WiFi scan/sta-scan/join/disconnect, deauth, beacon,
    handshake capture, SoftAP, packet monitor/raw-TX, captive portal, BLE scan/adv, GATT.
  - **NOT supported over compat_spi (excluded from the profile):** ESP-NOW
    (service `0x0006`), 802.15.4 (service `0x0007`), PMKID, probe-flood, karma,
    WiFi-mode-set, time-sync, BLE HID/spam. Full parity for those needs MtkCore's
    canonical **Native M1 SPI v1** transport (see next bullet).

- **MtkCore Native M1 SPI v1 (`m1_esp32_native.c/.h`) — host codec + client:**
  MtkCore's full-feature-parity transport, implemented host-side as a pure-logic
  codec plus a thin client. Wire format (all authoritative from
  `Monstatek/MonstaTek-Esp32-Core@main`): magic `"M1S1"` (`0x3153314D` LE),
  fixed **1024-byte cells**, a **40-byte little-endian header**
  (magic/major/minor/msg_class/flags/service/opcode/status/payload_len/
  request_id/packet_seq/boot_epoch/message_len/fragment_offset/crc32c),
  **CRC32C** (Castagnoli, poly `0x82F63B78`, covers header[0..35] ∥ payload),
  16-bit **service + opcode** addressing (System `0x0000`, WiFi `0x0001`, BLE
  `0x0002`, GATT `0x0003`, Capture `0x0004`, Diagnostics `0x0005`, ESP-NOW
  `0x0006`, 802.15.4/RCP `0x0007`), request-id **fragment reassembly**
  (`fragment_offset` == bytes-so-far, ceiling `MTK_SPI_NATIVE_MAX_MESSAGE 8192`,
  one inbound reassembly at a time), and a **paginated per-opcode
  `GET_CAPABILITIES`** (no bitmap — each `{service,opcode}` carries its own
  `capability_id` + `state`; re-request with `start_index = next_index` until
  `next_index == 0`). `m1_esp32_native.h` is the codec (header struct, CRC32C,
  cell build/verify, reassembly, PING/GET_API_IDENTITY/GET_CAPABILITIES
  decoders); `m1_esp32_native.c` is the client (HELLO handshake, single-/multi-
  cell `mtk_native_call`, request-id sequencing) over an **injectable 1024-byte
  exchange fn-pointer** (`mtk_native_xfer_fn`). Buffers are heap-allocated — the
  module adds **no static buffers** (RAM budget is critically tight). All host-
  verifiable behaviour is covered by `tests/test_esp32_native.c`.
  - **`ESP32_TRANSPORT_NATIVE_V1`** exists in `esp32_transport_t` but
    `esp32_firmware_transport()` **never returns it** — native has no capability
    bitmap to classify from, so it is selected only by a live HELLO /
    `GET_API_IDENTITY` exchange, never by bitmap inference. Existing device
    detection is unchanged.
  - **Native SPI is exposed only by the `universal` and `mtkcore-154` build
    images, not `mtkcore-154-rcp`** ("Spinel owns the link"). A host **must**
    branch on live `GET_CAPABILITIES`, never on variant name.
  - **Deferred to an on-hardware follow-up (author-disclosed gaps — do NOT
    guess):** the HELLO_ACK negotiation payload is **empty / undefined** in the
    accepted contract, and the physical **512→1024 cell-size handshake** has no
    explicit byte layout in MtkCore source. The physical SPI exchange primitive
    and live transport activation therefore cannot be validated without the
    device, and are intentionally left unimplemented per the repo rule against
    asserting unverified capability.

- **MtkCore CAPTURE service (`m1_esp32_capture.c/.h`) — MonstaShark-equivalent
  802.11 → PCAPNG:** host-side codec + client driver for the native **Capture
  service `0x0004`** (opcodes START `0x01`, STOP `0x02`, STATUS `0x03`,
  SESSION_INFO `0x04`, STATS `0x05`, POLL_READ `0x06`). Wire contract
  (authoritative from `Monstatek/MonstaTek-Esp32-Core@main`, commit `3e21a6a`):
  **CAPTURE_START** req is a tight-LE 18-byte body (`u8 mode; u16 snap_len;
  u32 duration_ms; channel_plan{u8 mode;u8 channel;u8 band;u16 hop_dwell_ms};
  filter{u8 bssid[6]}`), firmware rejects `snap_len == 0 || > 1000`, and replies
  **ACCEPTED (status 1, not OK)** with a `u32 operation_token` — so the driver
  treats OK and `MTK_STATUS_ACCEPTED` alike. **CAPTURE_POLL_READ** req is the
  `u32` token; the response is a tagged union — **EMPTY = zero body bytes**
  (nothing buffered, success) or a **20-byte record header** (`u32 sequence;
  u64 timestamp_us; u8 link_type(0=IEEE80211); u8 channel; i8 rssi; u8 flags
  (bit0=truncated); u16 original_len; u16 captured_len`) followed by
  `captured_len` raw frame bytes (a POLL record can be 1020 B → spans 2 cells;
  native reassembly handles it). **CAPTURE_STOP** req is `u32 token; u8 reason`.
  A **capability gate** (`mtk_capture_caps_page_supported`) scans the native
  `GET_CAPABILITIES` entries for a SUPPORTED CAPTURE_START before use. The
  **PCAPNG half is `wifi_pcapng.c/.h`** — a pure-logic encoder emitting a
  Wireshark-openable SHB/IDB/EPB stream with `LINKTYPE_IEEE802_11_RADIOTAP`
  (each EPB = a 15-byte radiotap header carrying Flags/Channel/dBm-signal, then
  the raw 802.11 frame). Everything is stack/buffer-only (**no static buffers**;
  RAM unchanged) and host-tested (`tests/test_esp32_capture.c`,
  `tests/test_wifi_pcapng.c`).
  - **Live SD capture IS wired — on the RPC transport (`m1_pcap_capture.c/.h`).**
    The grounded raw-frame producer `m1_esp32_rpc_monitor_read()` already returns
    true raw 802.11 frames + channel + RSSI over the shipping RPC "M1 Link", so
    the RPC packet sniffers (`wifi_sniffer_run` → All/Beacon/Probe/Deauth/SAE/
    Pwnagotchi) now open an `m1_pcap_session_t`, write each frame through the
    `wifi_pcapng` encoder, and save `capture/sniffNNN.pcapng` (auto-indexed
    0..999, best-effort) to SD. The SD-write glue is **host-tested through the
    stdio-backed FatFS stub** (`tests/test_pcap_capture.c`) — the file is read
    back and its PCAPNG/radiotap byte layout validated. Snaplen is 512 B; the EPB
    `original_len` keeps the true frame length even when truncated. The session is
    heap/buffer-only (no new static RAM; link RAM unchanged at 85.82%).
  - **Still deferred to the on-hardware follow-up:** the *native* CAPTURE
    service's physical 1024-byte SPI exchange primitive and probe-time activation
    are not wired to live SPI (its byte layout is author-disclosed-incomplete),
    so the native-transport SD path is validated only once that link is driven
    on-device. The EAPOL and binary-SPI `CMD_PKTMON_NEXT` sniffers deliver
    *decoded* records (not raw frames) and intentionally do not emit a capture
    file. The native codec, capability gate, driver, and PCAPNG encoder are
    complete and host-verified today.

- **`m1_esp32_rpc.c/.h`** is the reusable M1_RPC feature layer for brain CD3:
  the canonical opcode map (`m1_esp32_rpc_id_t`, mirrored from the shared
  `bedge117/m1-esp32-brain` `m1_rpc.h`), payload structs, and a NAK/status-aware
  `m1_esp32_rpc_call(msg_id, req, len, resp, cap, *rlen, timeout)` that frames,
  sends over the **512-byte full-duplex "M1 Link" transport**
  (`spi_m1link_send_recv_bin`, the default), and decodes. The framing/pipelining
  is a pure, host-tested helper — `m1_esp32_m1link_send_recv()` — that issues the
  request then follow-up IDLE transactions, scanning each 512-byte frame for the
  matching `RESP`/`NAK` (skipping IDLE/EVENT/mismatched frames, reassembling
  FRAGs). On-target `spi_m1link_send_recv_bin()` (`esp_app_main.c`) supplies the
  single-transaction primitive via `HAL_SPI_TransmitReceive` on `hspi_esp` (SPI3)
  with manual CS (PB10) + HANDSHAKE (PD7), and does **not** need the ESP-AT RTOS
  task. The AT presence / `AT+CMD?` probes stay on `spi_AT_send_recv_bin`.
  **Poll budget / pacing**: the on-target transport scales its follow-up poll
  budget from the caller's timeout (seconds) and paces each poll on the slave's
  HANDSHAKE with a scheduler yield (`vTaskDelay`), plus `HAL_SPI_Abort` self-heal
  on a failed transaction. A slow bulk-list reply (WiFi/BLE scan takes the brain
  ~1s+ before it queues its RESP) therefore gets a real multi-second window —
  the old fixed 8-poll, busy-spin budget returned "AP scan failed" before the
  brain finished scanning. Mirrors the proven C3 `m1_link` master.
- **ESP-NOW** (`m1_espnow_hal.c`) is the first consumer of this client. Other
  WiFi/BLE/802.15.4 features adopt it via the per-feature layer below.
- **`m1_esp32_rpc_features.c/.h`** is the per-feature layer on top of the client:
  `esp32_feature_rpc_opcode(feature_id, &op)` maps each `esp32_feature_id_t` to
  its native `M1_ESP32_RPC_*` opcode, and there is one small action wrapper per
  feature action (`m1_esp32_rpc_wifi_scan()`, `m1_esp32_rpc_deauth_start()`,
  `m1_esp32_rpc_ble_hid_key()`, `m1_esp32_rpc_zb_sniff_get()`, the `*_start` /
  `*_stop` triggers, …) that builds the payload, calls `m1_esp32_rpc_call()`,
  and decodes the reply. Wire a feature in by branching:
  `if (m1_esp32_active_transport() == ESP32_TRANSPORT_RPC) { m1_esp32_rpc_*(); }
  else { /* existing AT / binary-SPI path */ }`. The 802.15.4 sniffer/flood
  (`m1_802154.c`) is a worked example. The whole layer is transport-injectable
  and host-tested in `tests/test_esp32_rpc_features.c`.
- **CD3-AT is never re-routed to M1_RPC** — it advertises `WIFI_JOIN` without the
  brain-CD3 `HANDSHAKE + 802154_TX/BLE_SPAM` combination, so it falls through to
  `ESP32_TRANSPORT_AT` and keeps using the existing AT command paths unchanged.
- Frame constants and the pure build/parse inline helpers live in
  `m1_esp32_caps.h`; `m1_esp32_rpc.h` reuses them (no duplication).

### Communication
- M1 ↔ ESP32-C6 uses **SPI** for AT commands (NOT UART)
- SPI Mode 1 (CPOL=0, CPHA=1) — hardcoded in `m1_esp32_hal.c:529`
- Firmware flashing uses UART (ROM bootloader), but runtime AT uses SPI
- Espressif's stock AT firmware downloads are UART-only — they will NOT work with M1

### ESP32 Firmware Requirements
- Must be built with `CONFIG_AT_BASE_ON_SPI=y` (NOT UART)
- Must use `CONFIG_SPI_MODE=1` (matches M1's STM32 SPI master)
- Module config: `ESP32C6-SPI` (NOT `ESP32C6-4MB` which is UART)
- Build project: `D:\M1Projects\esp32-at-hid\`
- Full setup script (submodules + tools + build): `D:\M1Projects\esp32-at-hid\build_spi_at.bat`
- ESP-IDF cannot build from Git Bash (MSYSTEM detection) — use cmd.exe or PowerShell

### Memory Footprint Estimates — `M1_ESP32_FALLBACK_*`

> **For firmware developers and debugging agents only** — these values are not
> transmitted over the SPI wire protocol and are not visible to users.

`bss_bytes` and `free_heap_bytes` are not part of the `CMD_GET_STATUS` response
payload.  The M1 derives them from compile-time constants in
`m1_csrc/m1_esp32_caps.h` (`M1_ESP32_FALLBACK_*`) for use in OOM diagnostics
and buffer-sizing decisions when investigating SubGhz Read Raw, BLE sniff, or
other memory-intensive features.

#### How the estimate is selected

After `m1_esp32_caps_init()` resolves the capability bitmap (either from a live
probe or the compile-flag fallback), it applies a four-way discriminator in
priority order:
- **`HANDSHAKE` and (`802154_TX` or `BLE_SPAM`) present** → CD3 (brain) profile
- **`WIFI_JOIN` and `BEACON` both present** → dag T-800 profile
- **`WIFI_JOIN` present, `BEACON` absent** → CD3-AT profile
- **`WIFI_JOIN` absent** → SiN360 profile

The selected constants are written to `s_bss_bytes` and `s_free_heap_bytes` and
returned by `m1_esp32_caps_bss_bytes()` / `m1_esp32_caps_free_heap()`.

#### Current estimates and their derivation

| Profile | Constant | Value | Source |
|---------|----------|-------|--------|
| SiN360 | `M1_ESP32_FALLBACK_BSS_SIN360` | 200 KB | sincere360/M1_SiN360_ESP32 v0.9.0.8, ESP-IDF 5.5.4 |
| SiN360 | `M1_ESP32_FALLBACK_HEAP_SIN360` | 160 KB | NimBLE with `MSYS_BUF_FROM_HEAP=y`; 10×1600 B static WiFi RX; `ap_records[64]` ≈ 14 KB |
| CD3-AT | `M1_ESP32_FALLBACK_BSS_AT` | 284 KB | bedge117/esp32-at-monstatek-m1 v2.0.2, ESP-AT v4.0.0.0 |
| CD3-AT | `M1_ESP32_FALLBACK_HEAP_AT` | 112 KB | Full AT infrastructure + SPI ring buffers + BLE HID + 802.15.4 |
| dag T-800 | `M1_ESP32_FALLBACK_BSS_T800` | 290 KB | Estimated from ESP-AT base + dag custom modules |
| dag T-800 | `M1_ESP32_FALLBACK_HEAP_T800` | 105 KB | AT + deauth/beacon/monitor static buffers |
| CD3 | `M1_ESP32_FALLBACK_BSS_CD3` | 185 KB (est.) | bedge117/m1-esp32-brain v1.x — native ESP-IDF, no AT overhead (estimate, to be refined) |
| CD3 | `M1_ESP32_FALLBACK_HEAP_CD3` | 175 KB (est.) | Native WiFi/BLE/802.15.4 without AT ring buffers (estimate, to be refined) |

#### How to update estimates when a new firmware release appears

1. **Fetch the firmware repository** (sincere360/M1_SiN360_ESP32 or
   bedge117/esp32-at-monstatek-m1, i.e. CD3-AT) and check out the new release tag.

2. **Measure BSS from the map file**: after a build, open the `.map` file
   and sum all BSS-section contributions, or use the linker symbols:
   ```python
   # ESP-IDF: from the elf file
   $ xtensa-esp32c6-elf-size -A build/<name>.elf | grep -E '\.bss|\.noinit'
   ```
   Alternatively, subtract `(_ebss - _sbss)` from the IDA/Ghidra BSS view.

3. **Measure free heap from a running device**:
   - Flash the new firmware to the XIAO test bench.
   - After boot, send `AT+GMR\r\n` (for AT builds) or a known status command to
     let the firmware settle.
   - Read `esp_get_free_heap_size()` — for AT builds this is reported by
     `AT+SYSRAMINFO`; for SiN360 it appears in the boot log on the UART console.

4. **Update the four constants** in `m1_csrc/m1_esp32_caps.h` with the new
   measured values (round down to the nearest KB for conservatism):
   ```c
   #define M1_ESP32_FALLBACK_BSS_SIN360   (NNN * 1024u)
   #define M1_ESP32_FALLBACK_HEAP_SIN360  (NNN * 1024u)
   ```

5. **Update the invariant tests** in `tests/test_esp32_caps.c`:
   - `test_fallback_at_bss_exceeds_sin360` — must still pass (AT BSS > SiN360 BSS)
   - `test_fallback_sin360_heap_exceeds_at` — must still pass (SiN360 heap > AT heap)
   If a new firmware reverses either invariant, update the test and the comment.

6. **Update this table** and the table in `documentation/esp32_firmware.md`
   with the new values and their source (firmware version + analysis method).

7. **Update `README.md`**: whenever a `M1_ESP32_CAP_*` bit is added/removed, a
   `M1_ESP32_CAP_PROFILE_*` macro's cap count changes, or a new firmware
   variant/profile is introduced, update the ESP32 firmware comparison table
   in the "Hardware" section of `README.md` (caps-supported counts, the
   `X / N total caps` denominator, and the firmware list/links) so it stays
   consistent with `m1_esp32_caps.h`, the capability matrix in
   `documentation/esp32_firmware.md`, and this skill's tables. If you're only
   syncing README (no firmware source changes), no firmware build is required.

### ESP32 Build — How to Build from Claude Code

**CRITICAL: `cmd.exe /C` piped through Git Bash loses all output and fails silently with batch files.
The ONLY reliable method is a `.ps1` script executed via `powershell.exe -File`.**

**What DOES NOT work (do not attempt):**
- `cmd.exe /C "some.bat"` — runs but captures no output, fails silently
- `cmd.exe /C "call some.bat" 2>&1` — same problem
- `cmd.exe /C "... && ..." 2>&1 | tail` — piping eats all output
- Inline PowerShell via `powershell.exe -Command "..."` — `$` escaping between bash and PowerShell is broken; `foreach`, `$matches`, etc. all fail

**What DOES work:**
1. Write a `.ps1` file to disk using `cat > file.ps1 << 'EOF' ... EOF`
2. Execute it: `powershell.exe -NoProfile -ExecutionPolicy Bypass -File "D:\M1Projects\esp32-at-hid\build_now.ps1" 2>&1`
3. Run as background task with 600s timeout (full rebuild takes ~5 minutes)

**Reference build script** (`build_now.ps1`):
```powershell
Set-Location "D:\M1Projects\esp32-at-hid"
$env:MSYSTEM = ""
$env:IDF_PATH = "D:\M1Projects\esp32-at-hid\esp-idf"
$env:ESP_AT_PROJECT_PLATFORM = "PLATFORM_ESP32C6"
$env:ESP_AT_MODULE_NAME = "ESP32C6-SPI"
$env:ESP_AT_PROJECT_PATH = "D:\M1Projects\esp32-at-hid"
$env:SILENCE = "0"

$envVars = python esp-idf\tools\idf_tools.py export --format=key-value 2>&1
foreach ($line in $envVars) {
    if ($line -match '^([^=]+)=(.+)$') {
        [System.Environment]::SetEnvironmentVariable($matches[1], $matches[2], 'Process')
    }
}
$env:PATH = "C:\Program Files\Git\cmd;" + $env:PATH

python esp-idf\tools\idf.py -DIDF_TARGET=esp32c6 build 2>&1
```

**Post-build:** The build auto-generates the factory image at `build/factory/factory_ESP32C6-SPI.bin`. Then generate the MD5:
```python
python -c "
import hashlib
with open('D:/M1Projects/esp32-at-hid/build/factory/factory_ESP32C6-SPI.bin', 'rb') as f:
    md5 = hashlib.md5(f.read()).hexdigest().upper()
with open('D:/M1Projects/esp32-at-hid/build/factory/factory_ESP32C6-SPI.md5', 'wb') as f:
    f.write(md5.encode('ascii'))
"
```

### ESP32 Firmware Flashing via M1
- M1's flasher requires both `.bin` and `.md5` files on SD card
- **MD5 file must be UPPERCASE hex, exactly 32 bytes, no newline** — M1 uses uppercase in `mh_hexify()`
- Factory image at offset 0x000000 (contains bootloader + partition table + app)
- Recovery files: `D:\M1Projects\esp32_recovery\`
