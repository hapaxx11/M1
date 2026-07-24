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
  - [ ] Audit and fix in a follow-up phase.

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

Not yet individually verified against their event handlers (deferred to a later
pass): `m1_nfc.c:507` (Retry/More), `m1_nfc.c:1168` (Cancel/Write),
`m1_nfc.c:2083` (Stop), and the ~45 other `m1_draw_bottom_bar` call sites across
`m1_badbt.c`, `m1_badusb.c`, `m1_bt.c`, `m1_rfid.c`, `m1_storage.c`,
`m1_gpio_uart.c`, `m1_power_ctl.c`, `app_hex_viewer.c`, `m1_802154.c`,
`m1_esp32_fw_download.c`, `m1_fw_download.c`, `m1_clock.c`, `m1_app_api.c`.
- [ ] Audit and fix in a follow-up phase.

---

## Phase 3 — Extract Button Bar to Common Module

Currently `subghz_button_bar_draw()` lives in `m1_subghz_button_bar.{c,h}` but is
called from `m1_wifi.c` (8 call sites) and any future non-SubGHz scene.  The
function is generic — it draws rounded-corner buttons in a 3-column layout and has
no Sub-GHz–specific logic.

Plan:
- [ ] Create `m1_csrc/m1_button_bar.c` and `m1_csrc/m1_button_bar.h`
  - Move `subghz_button_bar_draw()` implementation here
  - Rename to `m1_button_bar_draw()` (keep a `static inline` forwarding macro
    or `#define subghz_button_bar_draw m1_button_bar_draw` in the old header for
    backward compat during transition)
- [ ] Keep `m1_subghz_button_bar.h` as a thin wrapper that `#include`s
  `m1_button_bar.h` + adds `subghz_status_bar_draw()` and `subghz_rssi_bar_draw()`
  (these ARE Sub-GHz–specific)
- [ ] Update `m1_wifi.c` to `#include "m1_button_bar.h"` directly
- [ ] Update `cmake/m1_01/CMakeLists.txt` to add `m1_button_bar.c`
- [ ] Grep for any other non-subghz users and update includes
- [ ] Build + verify no regressions

---

## Phase 4 — Rename "Phase E" Terminology

"Phase E" is an internal agent-session label (from the session that created scene
submenu model rollout) that leaked into source comments and skills docs.  It has no
meaning to human contributors and should be replaced with the actual concept name.

Replacement: "Phase E submenu model" → "shared submenu model" (or just "submenu
model pattern").

### 4A — Skills docs (`.github/skills/ui-scene-architecture/SKILL.md`)
- [ ] Replace all 12 occurrences of "Phase E" with plain descriptive text
- [ ] Section heading "Phase E submenu model pattern" → "Submenu model pattern"
- [ ] Table entries "✅ Phase E submenu model" → "✅ Submenu model"

### 4B — Source code comments (24 occurrences across 22 files)
- [ ] Global sed: "Phase E:" comment header → "Submenu model:" or remove
- [ ] `m1_submenu.h` / `m1_submenu.c` — update prose references
- [ ] `tests/CMakeLists.txt` / `tests/test_submenu_widget_rollout.c` — update

Build not required (comment-only + docs changes).

---

## Estimated Scope

| Phase | Files touched | Build required | Tests required |
|-------|--------------|----------------|----------------|
| 1 | 3 | Yes | Yes (host tests) |
| 2 | 3–5 | Yes | Yes |
| 3 | 5–7 | Yes | Yes |
| 4 | ~24 | No | No |

Phases are independent and can land in separate commits / sessions.
