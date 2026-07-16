---
name: firmware-testing
description: Host-side unit testing, pure-logic extraction/modularization patterns, the phase-checklist workflow, and firmware-wide modularization rules for the M1 firmware. Load when adding tests, extracting logic into testable modules, refactoring, or planning moderate-to-complex changes.
---

# Firmware Testing & Modularization

> Extracted from CLAUDE.md. These rules apply whenever you add tests, extract
> pure logic, refactor a module, or plan a moderate-to-complex change.

### Preferred Modularization Pattern — Extract Pure Logic

When a monolithic firmware source file (scene, module, or driver) contains
**pure-logic functions** mixed with hardware-coupled code, the preferred approach
is to **extract the pure logic into a standalone `.c`/`.h` compilation unit**.
This is both a code quality and testability requirement — it applies to all new
development, refactors, and bug fixes.  This rule applies to both human
contributors and AI agents.

#### Why

Monolithic files that mix parsing, protocol encoding, data conversion, and
hardware interaction become difficult to test, review, and maintain.  Extracting
pure logic into its own module:
- Makes the logic **testable on the host** via the stub-based extraction pattern.
- Reduces coupling between business logic and HAL/RTOS/display code.
- Makes the firmware easier to review, since each module has a single concern.
- Enables safe refactoring — the extracted module can be improved independently.

#### The pattern

1. **Identify extractable logic** — look for code blocks that:
   - Take structured inputs and produce outputs without side effects.
   - Do not directly access hardware registers, RTOS primitives, or display state.
   - Can be described independently from the scene/module that contains them.
   - Examples: file format parsers, protocol encode/decode, data conversion,
     path remapping, filter/match logic, ring buffer management.

2. **Create a new `.c`/`.h` pair** in the appropriate source directory (e.g.,
   `Sub_Ghz/`, `m1_csrc/`).  Name the module after the extracted concern:
   - `subghz_key_encoder.c/h` — KEY→RAW OOK PWM encoding
   - `subghz_raw_line_parser.c/h` — RAW_Data line parsing
   - `subghz_raw_decoder.c/h` — offline RAW→protocol decode engine
   - `subghz_playlist_parser.c/h` — Flipper→M1 path remapping

3. **Define a clean interface** in the header:
   - Use opaque structs or simple parameter types.
   - If the logic needs hardware-side operations (e.g., decoder dispatch),
     use a **callback function pointer** to decouple.  The caller provides a
     thin adapter; the module never touches hardware directly.
   - Mark the module as hardware-independent in the file header comment.

4. **Update the original file** to call the extracted module instead of inlining
   the logic.  The original file becomes a thin orchestrator.

5. **Add the new `.c` file to the firmware CMake build** (`cmake/m1_01/CMakeLists.txt`).

6. **Write unit tests** following the stub-based extraction testing pattern below.

#### What NOT to extract

- **AT command strings** — construction is interleaved with SPI send/receive.
- **Display rendering** — tightly coupled to u8g2 state and draw order.
- **RTOS task flow** — queue/semaphore orchestration is inherently side-effectful.
- **Trivial glue code** — one-liner dispatches aren't worth a separate module.

### Preferred Unit Testing Pattern — Stub-Based Extraction

This is the **canonical pattern** for adding host-side unit tests to any M1 firmware
module.  All new test suites **MUST** follow this approach.  It has been successfully
applied to SubGhz (14 suites), Flipper file parsers (RFID/IR/NFC), the LFRFID
Manchester decoder, and the OTA asset filter.

#### The pattern

1. **Identify pure-logic functions** in the firmware `.c` file — protocol mapping
   tables, parsers, encoders/decoders, data conversion, filter logic, math.  These
   functions take inputs and return outputs without touching hardware registers,
   RTOS queues, or global display state.

2. **Create minimal stubs** in `tests/stubs/` for any headers that the `.c` file
   includes transitively (HAL, RTOS, FatFS).  Stubs provide only the **types,
   constants, and struct definitions** needed for compilation — no function bodies.
   Existing stubs to reuse:
   - `stm32h5xx_hal.h` — GPIO/TIM/SPI types, pin macros
   - `ff.h` — FatFS `FRESULT`, `FIL`, `FILINFO` types
   - `app_freertos.h`, `cmsis_os.h`, `main.h` — empty stubs
   - `FreeRTOS.h`, `queue.h`, `stream_buffer.h` — minimal type stubs
   - `irmp.h` — IRMP protocol constants
   - `lfrfid.h` — `LFRFIDProtocol` enum, `lfrfid_evt_t`, `FRAME_CHUNK_SIZE`
   - `lfrfid_hal.h`, `t5577.h` — timer/encoded data types

3. **If a function is `static`**, either make it non-static and declare it in the
   header, or extract it into a new standalone `.c`/`.h` compilation unit.  For
   functions buried in HAL-heavy files (e.g., `m1_fw_source.c`), create a
   test-only copy in `tests/stubs/` that mirrors the production logic — document
   the duplication in the stub file header.

4. **Write the test file** as `tests/test_<module>.c` using Unity:
   ```c
   #include "unity.h"
   #include "<module_header>.h"   /* or include stubs first */

   void setUp(void) { }
   void tearDown(void) { }

   void test_<function>_<case>(void) {
       TEST_ASSERT_EQUAL(...);
   }

   int main(void) {
       UNITY_BEGIN();
       RUN_TEST(test_<function>_<case>);
       return UNITY_END();
   }
   ```

5. **Add a CMake target** in `tests/CMakeLists.txt`:
   ```cmake
   add_executable(test_<module>
       test_<module>.c
       ${M1_ROOT}/path/to/<module>.c
   )
   target_include_directories(test_<module> PRIVATE
       ${STUBS_DIR}
       ${M1_ROOT}/path/to/headers
   )
   target_link_libraries(test_<module> PRIVATE unity)
   if(TARGET sanitizers)
       target_link_libraries(test_<module> PRIVATE sanitizers)
   endif()
   add_test(NAME <module> COMMAND test_<module>)
   ```

6. **Build and run**: `cmake -B build-tests -S tests && cmake --build build-tests
   && ctest --test-dir build-tests --output-on-failure`

7. **Update the CI path filter** — open `.github/workflows/tests.yml` and check
   that every top-level source directory referenced by your new test's
   `add_executable` block already appears in the `paths:` list under both the
   `pull_request` and `push` triggers.  If a directory is missing, add it.
   Without this step, changes to that directory will never trigger CI, and
   regressions will go undetected.

   **Quick audit** (run from the repo root after editing `tests/CMakeLists.txt`):
   ```bash
   python3 - <<'PY'
   import re
   content = open('tests/CMakeLists.txt').read()
   for m in re.finditer(r'add_executable\((\w+)(.*?)add_test\(NAME (\w+)', content, re.DOTALL):
       refs = re.findall(r'\$\{M1_ROOT\}/([^\s\)\$]+)', m.group(2))
       dirs = sorted(set(r.split('/')[0] for r in refs))
       if dirs:
           print(m.group(3), '->', dirs)
   PY
   ```
   Compare output against the `paths:` list; add any new directory before committing.

#### What NOT to unit test

- **AT command construction** (`m1_wifi.c`, `m1_bt.c`, `m1_ble_spam.c`) — entirely
  ESP32 SPI communication, no pure logic to extract.
- **Direct HAL GPIO manipulation** (`m1_gpio.c`) — hardware-only.
- **UI rendering code** — display drawing is tightly coupled to u8g2 state.
- **RTOS task orchestration** — queue/semaphore interactions need the real scheduler.

These modules are tested via hardware integration, not host-side unit tests.

#### Dispatch / routing logic — extract and test the decision, not the action

When firmware code chooses between two hardware-coupled paths based on input
properties (e.g. "use this replay function for format A, that replay function for
format B"), the *dispatch condition* should be extracted into a **named pure
function in the header** and tested in isolation.  This is distinct from testing
the hardware actions on either side of the branch (which cannot run on the host).

**Why this matters:** The actual branching code typically lives inside a scene
`on_event` handler that requires display, queues, and radio — untestable on the
host.  If the dispatch condition is just an inline `if (a && b)` in that handler,
any regression in *a* or *b* (e.g. a flag being dropped, a type changing) will
only be caught during hardware testing.  Naming and exporting the condition gates
it with a deterministic CI check.

**The canonical example** in this codebase is `flipper_subghz_emulate_path()`:

```c
/* flipper_subghz.h */
typedef enum {
    FLIPPER_SUBGHZ_EMULATE_DIRECT,   /* sub_ghz_replay_datafile()     */
    FLIPPER_SUBGHZ_EMULATE_CONVERT,  /* sub_ghz_replay_flipper_file() */
} flipper_subghz_emulate_path_t;

static inline flipper_subghz_emulate_path_t
flipper_subghz_emulate_path(bool is_raw, bool is_m1_native)
{
    return (is_raw && is_m1_native)
           ? FLIPPER_SUBGHZ_EMULATE_DIRECT
           : FLIPPER_SUBGHZ_EMULATE_CONVERT;
}
```

The function is called by both `m1_subghz_scene_saved.c` and
`m1_subghz_scene_playlist.c`.  The tests in `tests/test_flipper_subghz.c`
verify every branch — including end-to-end file-loading variants that confirm
the flags are set correctly by `flipper_subghz_load()`.  This means: if anyone
accidentally routes `.sgh` files through the conversion path again, CI fails
before the firmware is even built.

**Rules for new dispatch logic:**

1. Any `if (condition) call_A() else call_B()` where `call_A/B` are
   hardware-coupled functions **must** have the `condition` extractable.
2. Put the extracted condition in the relevant `.h` as a `static inline` or
   as a named non-static function.  Keep it pure (no side effects).
3. Write both pure-logic tests (no file I/O, cover all four `(T,T)/(T,F)/(F,T)/(F,F)`
   combinations) and end-to-end load+dispatch tests that confirm the flags
   propagate correctly from real file content.
4. Both callers must call the same dispatch function — never two separate
   inline checks that can drift apart.

### Phase Checklist for Moderate-to-Complex Changes

When a code change meets or exceeds **moderate complexity** (multiple files, multi-step logic,
new features, refactors, protocol additions, or anything requiring more than a handful of
small edits), the agent **MUST** create and maintain a temporary phase-tracking checklist file.

#### What is the checklist file?

- **Location**: `PHASE_CHECKLIST.md` in the repository root.
- **Purpose**: Document every implementation phase, track completion, and record commit/PR
  metadata so progress is never lost mid-session.
- **Lifetime**: Committed with each phase so progress is preserved across sessions. It is
  **removed from the branch before the final PR is created** and **MUST NOT appear** in the PR.

#### Checklist file format

```markdown
# Phase Checklist — <short task title>

## PR Metadata
- **PR Title**: <concise title for the pull request>
- **PR Description**: <1–3 sentence summary of the overall change for the PR body>

## Phases

### Phase 1 — <phase name>
- **Description**: <what this phase accomplishes>
- **Status**: ✅ Complete | 🔲 Not started | 🔄 In progress
- **Commit**: `<short commit message used when this phase was committed>`

### Phase 2 — <phase name>
- **Description**: <what this phase accomplishes>
- **Status**: 🔲 Not started
- **Commit**: _(pending)_

<!-- Add as many phases as needed -->
```

#### Rules

1. **Create `PHASE_CHECKLIST.md` before writing any code** — plan all phases up front.
2. **Each phase = one commit.** After completing a phase, stage the relevant source files
   **and** the updated `PHASE_CHECKLIST.md`, then commit with the message documented in the
   checklist. Update the checklist status to ✅ and record the commit message.
3. **Commit `PHASE_CHECKLIST.md` with every phase commit.** This preserves progress across
   sessions so work is never lost if a session ends unexpectedly. The checklist travels with
   the branch until the PR is finalized.
4. **Update the checklist after every commit** — mark the completed phase, note the commit
   message, and move to the next phase.
5. **PR metadata must be maintained** — keep the PR Title and PR Description in the checklist
   current as the work evolves. When it is time to create the PR, use these values.
6. **Remove `PHASE_CHECKLIST.md` before creating the PR.** Delete the file, stage the
   deletion (`git rm PHASE_CHECKLIST.md`), and commit with a message such as
   `Remove phase checklist before PR`. Verify it does not appear in `git status` or in the
   diff against the base branch. The PR must not contain this file.
7. **When to skip**: Trivial changes (typo fixes, single-line edits, config tweaks) do NOT
   require a checklist. Use your judgment — if the change can be fully described in one commit
   message, skip the checklist.

---
## Firmware-Wide Modularization Rules

> All Phases A–J of the firmware-wide Momentum-parity programme are **complete**.
> What follows are the actionable coding rules that resulted from the work.
> Historical implementation logs are in git history.

### WiFi pure-logic extraction

**Do not add `HAL_Delay()` calls to `m1_wifi.c` or `m1_wifi_scene.c`.**  Use
`wifi_wait_dismiss()` for dismissible status screens and `vTaskDelay()` for hardware
timing waits.

`CMD_WIFI_JOIN` async conversion is **not achievable**: SiN360 ESP32 has no async
push mechanism; SPI is strictly master-initiated and `CMD_WIFI_JOIN` blocks in the ESP32
for up to 8 s before returning `RESP_BUSY`.  No further change needed on the STM32 side.

Pure-logic WiFi helpers live in `wifi_ap_record.c/h`, `wifi_mac_utils.c/h`,
`wifi_file_utils.c/h`, `wifi_status_msg.c/h`, `wifi_sta_record.h`,
`wifi_selection.c/h`, and `wifi_deauth_cmd.c/h` — all zero HAL/RTOS deps, all
covered by host tests.

### NFC pure-logic extraction

Pure-logic NFC helpers have been extracted and must be used for all new NFC code:

- **Do not** add inline card-classification logic to `m1_nfc.c` — use `nfc_card_info.h`.
- **Do not** add inline NDEF parsing or encoding to `m1_nfc.c` — use `nfc_ndef_parse.h` / `nfc_ndef_encode.h`.
- **Do not** add inline device-type parsing — use `nfc_file_parse.h` (`nfc_parse_device_type`).
- **Do not** add inline MFC layout/sector/key-parse logic — use `mfc_layout.h`.
- **Do not** add inline NFC-A family classification — use `nfc_classify.h`.
- **Do not** add inline NTAG page-count or Amiibo pwd derivation — use `nfc_ntag.h`.
- **Do not** add inline UID formatting to `nfc_ctx.c` — use `nfc_uid_fmt()` from `nfc_card_info.h`.
- NFC tools are RFAL-hardware-dependent; no ESP32 capability gating is needed in `m1_nfc.c` or `m1_nfc_scene.c`.

### ESP32 capability-probe module

**Do not** add new `DELEGATE_CAPPED(… M1_ESP32_CAP_*, "label")` sites — use
`DELEGATE_FEATURE(… ESP32_FEATURE_*)` and add one row to `esp32_feature_map.c` instead.

### Scene-file granularity

BT, Settings, and WiFi scene managers have been split into per-group files following
the `m1_subghz_scene_*.c` convention.  NFC (8), RFID (7), IR (6), and GPIO (6–9)
are acceptable as single files — **do not split them**.

### Shared submenu-widget rollout

All simple-label-list menus use `subghz_submenu_model_t` + `m1_submenu_event()` +
`m1_submenu_draw()`.  **Do not** use raw `sel`/`scroll` byte pairs for new menus.

IR menus are excluded: `m1_ir_quick_remote.c` has dynamic labels and
`m1_ir_universal.c` has no `m1_scene_menu_event` calls — both are non-trivial
widget candidates and remain as-is.

### IR pure-logic extraction

All pure-logic IR parsing lives in `ir_signal_record.c/h` (protocol name → IRMP ID,
path helpers, `ir_block_reader_t` KV-reader vtable, `ir_cmd_parse()` block parser,
`ir_parse_hex_bytes()`, `ir_parse_int32_array()`) and `ir_button_map.c/h`.

`flipper_ir_parse_block()` in `flipper_ir.c/h` is the vtable-backed Flipper `.ir`
block parser; `flipper_ir_read_signal()` is a thin FatFS adapter wrapping it.

`m1_ir_universal.c` and `m1_ir_quick_remote.c` retain only HAL-dependent and
scene-integrated code.  The 12 `vTaskDelay` / 54 `while` calls in `m1_ir_universal.c`
are timing-sensitive IR hardware ops — **no async conversion planned**.

### BadUSB DuckyScript migration

`m1_badusb.c` uses `badusb_parser.h` for all HID constant tables and classification.
**Do not** re-add inline key/modifier define tables to `m1_badusb.c`.
