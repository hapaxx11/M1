# Sub-GHz Improvement Plan

> **Temporary tracking document** — delete once all phases are complete.
>
> Last updated: 2026-03-28 (Phase 4 complete)

---

## Overview

Multi-phase enhancement of the M1 Hapax Sub-GHz Read/Record experience.
The goal is to bring the Sub-GHz workflow closer to Flipper Zero's UX while
leveraging the M1's superior radio hardware (SI4463).

---

## Phase 1 — Signal History ✅ COMPLETE

**Branch:** `copilot/fix-no-apps-found`
**Commits:** `8eca8f7`, `0391ef4`

### What was done

| Item | Status | Files |
|------|--------|-------|
| `SubGHz_History_t` ring buffer (50 entries) | ✅ | `m1_sub_ghz_decenc.h`, `m1_sub_ghz_decenc.c` |
| `SubGHz_Dec_Info_t` extended with serial, rolling code, button ID | ✅ | `m1_sub_ghz_decenc.h`, `m1_sub_ghz_decenc.c` |
| `subghz_reset_data()` clears extended fields | ✅ | `m1_sub_ghz_decenc.c` |
| Continuous decoding (doesn't stop after first match) | ✅ | `m1_sub_ghz.c` |
| Duplicate-signal deduplication (consecutive count) | ✅ | `m1_sub_ghz_decenc.c` |
| History list sub-view (UP to open, UP/DOWN scroll) | ✅ | `m1_sub_ghz.c` |
| Signal detail sub-view (OK to view from list) | ✅ | `m1_sub_ghz.c` |
| Protocol-aware live display (SN/RC/Button when available) | ✅ | `m1_sub_ghz.c` |
| History count badge `[N]` on live view | ✅ | `m1_sub_ghz.c` |
| `arrowup_8x8` icon for "Hist" hint | ✅ | `m1_display_data.c`, `m1_display.h` |
| Named constants (`SUBGHZ_HISTORY_ROW_HEIGHT`, etc.) | ✅ | `m1_sub_ghz.c` |
| CHANGELOG entry for 0.9.0.3 | ✅ | `CHANGELOG.md` |

### Button mapping (ACTIVE recording state)

| Sub-view | UP | DOWN | OK | BACK | LEFT/RIGHT |
|----------|-----|------|-----|------|------------|
| **Live** | Open history list | — | Stop recording | Stop recording | — |
| **History list** | Scroll up | Scroll down | View detail | Return to live | — |
| **Signal detail** | — | — | — | Return to list | — |

### Architecture notes

- History buffer: `SubGHz_History_t` in `m1_sub_ghz_decenc.h` — circular ring buffer
  indexed 0 = newest. Dedup via matching protocol + key + bit_len on consecutive entries.
- Three state booleans in `m1_sub_ghz.c`: `subghz_history_view_active`,
  `subghz_history_detail_active` control which sub-view is rendered.
- Rendering is in the `SUBGHZ_RECORD_DISPLAY_PARAM_ACTIVE` case of
  `subghz_record_gui_update()`.
- Buzzer only fires on genuinely new signals (not duplicates).

---

## Phase 2 — Frequency Hopping / Auto-Detect ✅ COMPLETE

**Branch:** `copilot/fix-no-apps-found`
**Commit:** `0453a64`

### What was done

| Item | Status | Details |
|------|--------|---------|
| Hopper state variables | ✅ | `subghz_hopper_idx`, `subghz_hopper_freq`, `subghz_hopper_active` |
| Helper: `subghz_hopper_retune_next()` | ✅ | Pauses RX → retunes SI4463 → restarts RX (full opmode path) |
| Helper: `subghz_read_rssi()` | ✅ | Reads current RSSI via `SI446x_Get_ModemStatus()` |
| Timeout-based queue receive | ✅ | `portMAX_DELAY` → `pdMS_TO_TICKS(150)` when hopping active |
| Frequency hop on timeout | ✅ | Cycles through 6 Flipper-default frequencies (310–868 MHz) |
| RSSI threshold gating | ✅ | Stays on freq if RSSI ≥ −70 dBm (signal activity detected) |
| Hopper init on record start | ✅ | Sets first hopper freq, overrides scan config |
| Hopper cleanup on stop | ✅ | Clears `subghz_hopper_active` on OK/BACK stop |
| READY display: "Hopping" | ✅ | Shows "Hopping AM650" instead of "433.92 AM650" |
| ACTIVE display: current freq | ✅ | Top-right shows current hopping frequency (e.g. "433.92") |
| ACTIVE display: "Scanning..." | ✅ | Shows "Scanning..." instead of "Recording..." when hopping |
| History uses hopper freq | ✅ | `cur_freq_hz = subghz_hopper_freq` in decode handler |
| Config UI toggle | ✅ | Already existed (Phase 1 scaffolding) |
| CHANGELOG entry | ✅ | Added to [0.9.0.3] |

### Design decisions

- **Dwell time: 150ms** — short enough for responsive scanning, long enough for
  the SI4463 to settle and for the protocol decoder to catch short-burst signals.
- **RSSI threshold: −70 dBm** — above typical noise floor (~−95 dBm), below
  typical signal level (~−50 dBm). Prevents hopping away while a signal is active.
- **Full opmode retune** — uses `sub_ghz_set_opmode()` rather than just
  `SI446x_Set_Frequency()` to handle band-switch, frontend select, and VCO
  recalibration automatically.
- **Hopping frequencies** — same 6 as Flipper Zero: 310, 315, 318, 390, 433.92, 868.35 MHz.
  Uses the existing `subghz_hopper_freqs[]` array.

### Button mapping (unchanged from Phase 1)

Hopping adds no new buttons. The Config screen already has the Hopping ON/OFF toggle.
During ACTIVE recording with hopping, all buttons work identically to non-hopping mode.

---

## Phase 3 — Enhanced Save Workflow ✅ COMPLETE

**Branch:** `copilot/fix-no-apps-found`

### What was done

| Item | Status | Details |
|------|--------|---------|
| `subghz_save_history_entry()` helper | ✅ | Builds `flipper_subghz_signal_t` from history entry, saves as `.sub` |
| Virtual keyboard filename prompt | ✅ | Uses `m1_vkb_get_filename()` with default name from protocol + key |
| DOWN = "Save" in detail view | ✅ | Keypad handler dispatches to save helper when detail active |
| "Save" hint in bottom bar | ✅ | Down-arrow icon + "Save" text shown in detail view |
| Success/failure message box | ✅ | `m1_message_box()` shows path or error after save |
| CHANGELOG entry | ✅ | Added to [0.9.0.3] |

### Design decisions

- **Filename default**: `{Protocol}_{Key_hex}` — e.g. "Princeton_1A2B3C" — user
  can edit via virtual keyboard before saving.
- **Save path**: `/SUBGHZ/{filename}.sub` — same directory as existing signal files.
- **No raw data in saved file**: History entries don't store raw pulse data, so saved
  `.sub` files are PARSED type only (Protocol/Bit/Key/TE). Raw recordings are still
  available via the COMPLETE state save which writes both `.sgh` + `.sub`.
- **Reuses existing infrastructure**: `flipper_subghz_save()`, `m1_vkb_get_filename()`,
  `m1_message_box()` — no new dependencies.

### Button mapping update (ACTIVE state)

| Sub-view | UP | DOWN | OK | BACK | LEFT/RIGHT |
|----------|-----|------|-----|------|------------|
| **Live** | Open history list | — | Stop recording | Stop recording | — |
| **History list** | Scroll up | Scroll down | View detail | Return to live | — |
| **Signal detail** | — | **Save .sub** | — | Return to list | — |

---

## Phase 4 — RAW Capture Visualization ✅ COMPLETE

**Branch:** `copilot/fix-no-apps-found`

### What was done

| Item | Status | Details |
|------|--------|---------|
| `subghz_raw_waveform[]` buffer | ✅ | 128-byte circular buffer (one byte per display column) |
| `subghz_raw_waveform_push()` | ✅ | Maps pulse duration (μs) to display columns at 500μs/col |
| `subghz_raw_waveform_draw()` | ✅ | Draws mark/space VLines from center reference line |
| `subghz_raw_waveform_reset()` | ✅ | Clears waveform buffer and sample counter |
| `subghz_raw_view_active` flag | ✅ | Controls RAW waveform sub-view rendering |
| Waveform feed from RX handler | ✅ | Processes saved ring buffer data after `sub_ghz_rx_raw_save()` |
| LEFT button toggle | ✅ | Toggles RAW view from live ACTIVE view, resets waveform on entry |
| BACK button exit | ✅ | Returns from RAW view to live view |
| "RAW" hint in live bottom bar | ✅ | Left-arrow + "RAW" shown in normal recording bottom bar |
| RAW view bottom bar | ✅ | "Back" + "Stop" buttons |
| RAW info overlay | ✅ | Shows "RAW Nk" (sample count) and "Ns" (recording time) |
| Reset on record start | ✅ | Waveform cleared alongside history reset |
| CHANGELOG entry | ✅ | Added to [0.9.0.3] |

### Design decisions

- **500μs per column** — each of the 128 display columns represents 500μs.
  This gives 64ms total visible window, which captures several complete protocol
  packets (typical te=300–500μs, packet=20–60 pulses).
- **Mark/Space rendering** — marks draw upward (18px) from center reference line,
  spaces draw downward (18px). Center reference is a dashed line for visual clarity.
- **Circular buffer** — 128-byte circular buffer with head pointer, automatically
  scrolls left as new data arrives (oldest data falls off the left edge).
- **Pulse clamping** — single pulses clamped to max 16 columns to prevent long
  inter-packet gaps from flooding the display with empty space.
- **Data sourced post-save** — waveform is fed from `subghz_ring_read_buffer` after
  `sub_ghz_rx_raw_save()` drains the ring buffer. This avoids double-reading the
  ring buffer and ensures zero impact on the existing recording/decode pipeline.
- **No separate "Read RAW" mode** — instead, RAW visualization is an overlay toggled
  with LEFT during any recording. This keeps the UI simple and avoids duplicating
  the entire record state machine.

### Button mapping update (ACTIVE state)

| Sub-view | UP | DOWN | LEFT | OK | BACK | RIGHT |
|----------|-----|------|------|-----|------|-------|
| **Live** | Open history | — | **Toggle RAW** | Stop | Stop | — |
| **History list** | Scroll up | Scroll down | — | View detail | Return to live | — |
| **Signal detail** | — | Save .sub | — | — | Return to list | — |
| **RAW waveform** | — | — | — | Stop | **Return to live** | — |

---

## Phase 5 — Protocol-Specific Emulation 🔲 PLANNED

### Goals
- Direct protocol-based TX (not just raw replay) for static-code protocols
- "Send" button in signal detail view for appropriate protocols
- Generate TX pulse train from decoded key + protocol parameters
- Protocols: Princeton, CAME, Nice FLO, Linear, Gate TX, etc.

### Key files
- `m1_sub_ghz.c` — TX initiation
- `Sub_Ghz/protocols/` — individual encoder implementations needed
- `m1_sub_ghz_decenc.c` — `subghz_protocols_list[]` parameters

### Notes
- Only for static codes. Rolling-code protocols (KeeLoq, Security+, etc.)
  cannot be replayed from decoded data alone.
- Raw replay (`sub_ghz_replay_start()`) already works for all protocols.

---

## Backlog / Ideas

- **Weather station display** — parse and display sensor data (temp, humidity)
  for weather protocols (Oregon v2, Acurite, LaCrosse, etc.)
- **Signal strength graph** — RSSI history over time
- **Preset management** — save/load frequency + modulation presets
- **BinRAW improvements** — better visualization of unknown protocol captures
- **Notification LED patterns** — different blink patterns for different
  protocol families during capture

---

## File Map

Key source files for Sub-GHz:

| File | Purpose |
|------|---------|
| `m1_csrc/m1_sub_ghz.c` | Main Sub-GHz UI, recording, replay, config |
| `m1_csrc/m1_sub_ghz.h` | Sub-GHz types, band/modulation enums, function decls |
| `Sub_Ghz/m1_sub_ghz_decenc.c` | Decoder engine, protocol dispatch, history buffer |
| `Sub_Ghz/m1_sub_ghz_decenc.h` | Decoder types, protocol enum, Dec_Info_t, History_t |
| `Sub_Ghz/protocols/` | Individual protocol decoders (Princeton, KeeLoq, etc.) |
| `m1_csrc/flipper_subghz.c` | Flipper `.sub` file read/write |
| `m1_csrc/m1_display.h` | Display constants, icon externs |
| `m1_csrc/m1_display_data.c` | Bitmap icon data (arrows, etc.) |
