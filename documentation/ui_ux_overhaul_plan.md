# UI/UX Consistency Overhaul — Phased Plan

Branch: `copilot/overhaul-ui-ux-consistency`

Reference: [UI / Button Bar Rules](/.github/skills/ui-scene-architecture/SKILL.md#ui--button-bar-rules)

---

## Phase 1 — Fix Sub-GHz Scene Button Bar Violations

Confirmed violations in scene-migrated Sub-GHz code:

- [x] **m1_subghz_scene_transmitter.c:512** — Removed "Back" label from LEFT slot
  (error state: bar now empty — hardware BACK button already dismisses the error).
- [x] **m1_subghz_scene_transmitter.c:581** — Removed "Back" label from LEFT slot
  (ready state, single-button case); CENTER already correctly shows OK/"Send".
- [x] **m1_subghz_scene_need_saving.c:109** — Moved "OK" from RIGHT to CENTER
  column (`ok_circle_8x8, "OK"`); LEFT/RIGHT slots now show plain arrow icons
  hinting the choice-cycle action (no "Cancel" label — hardware BACK already
  handles that). Replaced both `u8g2_DrawBox` calls with `u8g2_DrawRBox(..., 2)`
  for the Save/Discard choice highlights.
- [x] **m1_subghz_scene_playlist.c:500** — Moved "Play" to CENTER column with
  `ok_circle_8x8`; LEFT/RIGHT slots now show "R-"/"R+" (repeat count) matching
  the actual LEFT/RIGHT button behavior.

Build & test after each file change — **verified**: `cmake --build build-tests`
succeeds and `ctest --test-dir build-tests` passes all 129 tests (no regressions;
these are UI-only drawing changes with no host-testable pure logic).

---

### Discovered during Phase 1 (deferred, not yet fixed)

- **`m1_subghz_scene_transmitter.c:545`** and **`m1_subghz_scene_playlist.c:494`**
  both draw `arrowleft_8x8, "Stop"` in the LEFT slot, but the actual stop action
  is wired to `SubGhzEventBack` (the hardware BACK button), not the LEFT button.
  This is the same class of bug as the "Back" label violations (a button-bar hint
  pointing at the wrong physical button) but wasn't caught by the original text
  search since the label text is "Stop" rather than "Back". Needs its own pass:
  either rewire LEFT to actually stop, or remove the LEFT-slot hint since BACK
  already stops self-evidently.
  - [x] Audit and fix in a follow-up phase. Confirmed via
    `subghz_transmitter_ctl_event()` (PHASE_TX default case: "OK_PRESS / LEFT /
    RIGHT / TEARDOWN_DONE during TX are ignored") and
    `m1_subghz_scene_playlist.c` `scene_on_event()` (the `SubGhzEventLeft` case
    is guarded by `!app->playlist_running`) that LEFT is a genuine no-op while
    "Stop" was shown. Removed the mislabeled LEFT-slot hint in both
    `m1_subghz_scene_transmitter.c:545` and `m1_subghz_scene_playlist.c:494`
    (now `subghz_button_bar_draw(NULL, NULL, NULL, NULL, NULL, NULL)`) —
    hardware BACK already stops self-evidently, matching the resolution used
    for the "Back" label violations above. Host tests (`ctest --test-dir
    build-tests`) still 129/129 passing (UI-only drawing change, no
    host-testable pure logic affected).

## Phase 2 — Fix Legacy Module Button Bar Violations

Legacy (pre-scene) modules using `m1_draw_bottom_bar()` where OK is the primary
action (rule says use `subghz_button_bar_draw()` with center slot instead):

- [x] **m1_can.c:293** — CAN Send screen shows "Send" in RIGHT slot, but OK sends.
  Replaced `m1_draw_bottom_bar(…, "Send", arrowright_8x8)` with
  `subghz_button_bar_draw(arrowleft_8x8, NULL, ok_circle_8x8, "Send", arrowright_8x8, NULL)`.
- [x] **m1_infrared.c:320** (and 2 sibling call sites at ~287/376) — "Save"/"OK" in
  RIGHT slot for OK-triggered action. Converted all 3 to
  `subghz_button_bar_draw(NULL, NULL, ok_circle_8x8, "…", NULL, NULL)`.
- [x] **m1_ir_universal.c:2129** — "Assign"/"Save"; OK triggers Assign, RIGHT
  triggers Save. Converted to CENTER=`ok_circle_8x8,"Assign"`,
  RIGHT=`arrowright_8x8,"Save"`.
- [x] **m1_nfc.c:1270** (and sibling wipe-confirm bar + Card-Unlocked save bar at
  ~2818) — "Confirm"/"Wipe"/"Save" in RIGHT slot while OK/RIGHT both trigger the
  action and LEFT/Cancel had no real LEFT-button binding (only hardware BACK).
  Converted all 3 to CENTER=`ok_circle_8x8, "<label>"` with empty LEFT/RIGHT.

Build verified: full firmware build via
`cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build`
succeeds (gcc-arm-none-eabi 13.2, RAM 99.72%/FLASH 96.21% used, unchanged from
baseline — no new warnings introduced by these edits). Host tests
(`ctest --test-dir build-tests`) still 129/129 passing (these are UI-drawing-only
changes with no host-testable pure logic).

### Remaining `m1_draw_bottom_bar` sites not yet audited

Phase 2 follow-up audit completed against each screen's button handler:

- [x] **Converted / cleared mismatches**
  - `app_hex_viewer.c:213` — moved `Browse` from the RIGHT slot to
    CENTER=`ok_circle_8x8,"Browse"`; kept LEFT/RIGHT page-scroll arrows.
  - `m1_802154.c:275` — detail dismiss screen now uses CENTER=`ok_circle_8x8,"OK"`
    instead of a mislabeled RIGHT-slot `OK`.
  - `m1_802154.c:483` — scan list now uses CENTER=`ok_circle_8x8,"Info"` with
    LEFT informational page text, rather than showing OK-open on the RIGHT slot.
  - `m1_badbt.c:777`, `m1_badusb.c:299` — progress/done screens now use the
    CENTER slot (`"Stop"` while running, `"OK"` when done) instead of a
    RIGHT-slot `OK`.
  - `m1_badbt.c:830`, `m1_badbt.c:884` — wait-for-connection screens now use
    CENTER=`ok_circle_8x8,"Stop"`; `m1_badbt.c:1072` init screen bar cleared
    because no button action is wired during the transient init draw.
  - `m1_badbt.c:1202`, `m1_badusb.c:584` — file-run confirms now use
    CENTER=`ok_circle_8x8,"Run"`.
  - `m1_bt.c:2318` — BT config screen moved `Edit` from the LEFT slot to
    CENTER=`ok_circle_8x8,"Edit"` (OK is the real trigger).
  - `m1_can.c:340`, `m1_can.c:462`, `m1_can.c:553` — cleared BACK-only
    left-arrow hints on CAN error / placeholder screens.
  - `m1_infrared.c:429` — cleared BACK-only `Cancel` hint on the IR learn screen.
  - `m1_ir_universal.c:617` — `draw_list_screen()` now maps OK actions
    (`Open` / `Pick` / `Send`) to the CENTER slot, keeps `More` on LEFT for the
    command list, and only shows a RIGHT arrow when RIGHT genuinely pages.
  - `m1_nfc.c:1170`, `m1_nfc.c:3074` — write-confirm screens now use
    CENTER=`ok_circle_8x8,"Write"` instead of RIGHT-slot `Write`.
  - `m1_nfc.c:2760` — captured-password follow-up screen now uses
    CENTER=`ok_circle_8x8,"Read"`.
  - `m1_nfc.c:2085`, `m1_nfc.c:2710`, `m1_nfc.c:2884`, `m1_nfc.c:3022` —
    cleared BACK-only `Stop` / `Cancel` / `Exit` hints.
  - `m1_rfid.c:2820` — clone workflow moved `Clone` from the RIGHT slot to
    CENTER=`ok_circle_8x8,"Clone"` while preserving LEFT=`Retry`.
- **Intentionally left unchanged after handler audit**
  - `m1_badbt.c` / `m1_badusb.c` abort loops accept multiple buttons, but the
    converted screens now hint the OK-centered primary action only; no further
    scene restructuring was needed.
  - `m1_can.c:255` — LEFT/RIGHT genuinely cycle baud rate; the message count is
    informational text, not an OK-mapping violation.
  - `m1_clock.c:149` — LEFT/RIGHT genuinely page zones; OK is only an extra
    alias for `Next`, not the sole trigger.
  - `m1_esp32_fw_download.c:422`, `m1_esp32_fw_download.c:513`,
    `m1_fw_download.c:247`, `m1_fw_download.c:386` — explicit `OK:...` text is
    self-documenting and acceptable.
  - `m1_gpio_uart.c:245` — LEFT/RIGHT genuinely change baud, and `OK:Mode`
    explicitly documents the OK action.
  - `m1_nfc.c:509`, `m1_nfc.c:1387`, `m1_nfc.c:1916`, `m1_nfc.c:2060`,
    `m1_nfc.c:2196`, `m1_nfc.c:3513` — `Retry`/`More`/`Delete` and
    `OK:Start` already match their real bindings.
  - `m1_power_ctl.c:484`, `m1_power_ctl.c:604`,
    `m1_storage.c:531`, `m1_storage.c:649`, `m1_storage.c:859` — LEFT/RIGHT
    cancel/confirm actions are genuinely wired to LEFT/RIGHT.
  - `m1_rfid.c:485`, `m1_rfid.c:2086` — `Retry`/`More` and `Cancel`/`Delete`
    already use real LEFT/RIGHT handlers.
- [x] Audit and fix in a follow-up phase. Host tests
  (`cmake --build build-tests -j4 && ctest --test-dir build-tests --output-on-failure`)
  still pass 129/129 after the continued audit.

---

## Phase 3 — Extract Button Bar to Common Module

Currently `subghz_button_bar_draw()` lives in `m1_subghz_button_bar.{c,h}` but is
called from `m1_wifi.c` (8 call sites) and any future non-SubGHz scene.  The
function is generic — it draws rounded-corner buttons in a 3-column layout and has
no Sub-GHz–specific logic.

Plan:
- [x] Create `m1_csrc/m1_button_bar.c` and `m1_csrc/m1_button_bar.h`
  - Moved the generic 3-column renderer (`draw_btn()` helper +
    `m1_button_bar_draw()`) here verbatim from `m1_subghz_button_bar.c`.
- [x] `m1_subghz_button_bar.c` is now a thin wrapper: `subghz_button_bar_draw()`
  forwards to `m1_button_bar_draw()` (kept as a real function, not a macro,
  since it's never taken as a function pointer — preserves the ~20+ existing
  Sub-GHz call sites with zero call-site churn). `subghz_status_bar_draw()`
  and `subghz_rssi_bar_draw()` (genuinely Sub-GHz–specific) remain unchanged
  in this file. Header doc-comment updated to describe the split.
- [x] Updated `m1_wifi.c` and all Phase 2 legacy modules
  (`m1_can.c`, `m1_infrared.c`, `m1_ir_universal.c`, `m1_nfc.c`) to
  `#include "m1_button_bar.h"` directly and call `m1_button_bar_draw()` —
  none of them use the Sub-GHz-specific status/RSSI bars, so there was no
  reason for them to depend on `m1_subghz_button_bar.h` at all.
- [x] Updated `cmake/m1_01/CMakeLists.txt` to add `m1_button_bar.c` (under the
  "Generic Scene Framework" section, alongside `m1_scene.c`/`m1_submenu.c`).
- [x] Grepped for any other non-Sub-GHz users of `subghz_button_bar_draw` —
  none remained; all Sub-GHz scene files keep calling `subghz_button_bar_draw()`
  unchanged (still valid via the forwarding wrapper).
- [x] Build + verify no regressions:
  - Full firmware build (arm-none-eabi-gcc + Ninja, Release):
    RAM 653528B/640KB (99.72%), FLASH 1007896B/1023KB (96.21%) — identical
    to the Phase 2 baseline, confirming the extraction is a pure refactor.
  - Host tests: 129/129 passed (`ctest --test-dir build-tests`).

---

## Phase 4 — Rename "Phase E" Terminology

"Phase E" is an internal agent-session label (from the session that created scene
submenu model rollout) that leaked into source comments and skills docs.  It has no
meaning to human contributors and should be replaced with the actual concept name.

Replacement: "Phase E submenu model" → "shared submenu model" (or just "submenu
model pattern").

### 4A — Skills docs (`.github/skills/ui-scene-architecture/SKILL.md`)
- [x] Replaced all occurrences of "Phase E" with plain descriptive text
- [x] Section heading "Phase E submenu model pattern" → "Submenu model pattern"
- [x] Table entries "✅ Phase E submenu model" → "✅ Submenu model"

### 4B — Source code comments (across 20 `m1_csrc/*.c/.h` files + 2 `tests/` files)
- [x] "Phase E: uses ..." comment header → "Submenu model: uses ..." (18 scene files)
- [x] `m1_submenu.h` / `m1_submenu.c` — updated prose references ("Phase E adds" →
  "Submenu model adds", "Phase E extends" → "Submenu model extends", "Phase E
  convenience wrapper" → "Submenu model convenience wrapper")
- [x] `tests/CMakeLists.txt` / `tests/test_submenu_widget_rollout.c` — updated
  ("Phase E submenu-widget rollout" → "submenu model widget rollout")
- Note: `Drivers/CMSIS/Device/ST/STM32H5xx/Include/stm32h573xx.h` also matched
  "Phase E" in a grep, but it's a false positive (vendor CMSIS header text
  "...Arbitration **Phase E**nable..." — unrelated to the internal label) and
  was left untouched.

Build not required (comment-only + docs changes), but verified anyway:
host tests 129/129 passed after the comment changes (no regressions).

---

## Estimated Scope

| Phase | Files touched | Build required | Tests required |
|-------|--------------|----------------|----------------|
| 1 | 3 | Yes | Yes (host tests) |
| 2 | 3–5 | Yes | Yes |
| 3 | 5–7 | Yes | Yes |
| 4 | ~24 | No | No |

Phases are independent and can land in separate commits / sessions.
