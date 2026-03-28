# Sub-GHz Improvement Plan

> **Temporary tracking document** — delete once all phases are complete.
>
> Last updated: 2026-03-28

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

## Phase 2 — Frequency Hopping / Auto-Detect 🔲 PLANNED

### Goals
- Implement Flipper-style frequency hopping during Read
- Cycle through common frequencies (315, 390, 433.92, 868.35 MHz) when no
  signal is detected within a timeout
- Store the detected frequency with each history entry (already supported by
  `SubGHz_History_Entry_t.frequency`)
- Add hopping toggle to the Config screen (`subghz_cfg.hopping`)

### Key files
- `m1_sub_ghz.c` — hopper logic in the RX event handler
- `m1_sub_ghz.c` — `subghz_hopper_freqs[]` already defined (6 entries)
- `m1_sub_ghz.c` — Config screen UI

### Open questions
- Dwell time per frequency before hopping?
- Use RSSI threshold to detect activity before committing to decode?

---

## Phase 3 — Enhanced Save Workflow 🔲 PLANNED

### Goals
- Save individual signals from the history list (not just the whole recording)
- Save as Flipper-compatible `.sub` file directly from history detail view
- Add DOWN = "Save" button to the signal detail sub-view
- Prompt for filename via virtual keyboard

### Key files
- `m1_sub_ghz.c` — save logic (currently in DOWN handler of COMPLETE state)
- `flipper_subghz.c` / `flipper_subghz.h` — `.sub` file writer

---

## Phase 4 — RAW Capture Visualization 🔲 PLANNED

### Goals
- Show a real-time pulse waveform during recording (like Flipper's Read RAW)
- Visualize the raw pulse timing data from `subghz_rx_rawdata_rb`
- Separate "Read RAW" mode vs. protocol-decoded "Read"

### Key files
- `m1_sub_ghz.c` — new display mode
- `m1_ring_buffer.h` — raw sample ring buffer

### Notes
- 128px wide display = 128 time bins. Need to decide time scale and
  whether to show mark/space or just envelope.

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
