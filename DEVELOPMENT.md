<!-- See COPYING.txt for license details. -->

# M1 Development Guidelines

## Build Environment

### Prerequisites

| Tool | Version | Notes |
|------|---------|-------|
| ARM GCC | 14.2+ (14.3 recommended) | `arm-none-eabi-gcc` |
| CMake | 3.22+ | |
| Ninja | 1.10+ | |
| Python 3 | 3.8+ | For CRC injection script |
| STM32CubeIDE | 1.17+ (optional) | Includes toolchain; tested with 1.17.0 and 2.1.0 |

On Debian/Ubuntu: `sudo apt install gcc-arm-none-eabi cmake ninja-build python3`

### Build with CMake (recommended)

```bash
# Configure
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release

# Build (post-build CRC injection runs automatically)
cmake --build build
```

The CMake `POST_BUILD` step automatically runs `tools/append_crc32.py` to inject CRC
and Hapax metadata into the binary.  For non-CMake builds (STM32CubeIDE), run manually:

```bash
python tools/append_crc32.py build/M1_Hapax_v<VERSION>.bin \
    --output build/M1_Hapax_v<VERSION>_wCRC.bin \
    --hapax-revision 1 --verbose
```

Replace `<VERSION>` with the version from `m1_fw_update_bl.h` (e.g. `0.9.0.1`).
The `--hapax-revision` flag is **mandatory** — without it, the dual-boot bank screen
will not show the Hapax revision or build date. CI auto-increments the revision on each
merge.

See [`documentation/mbt.md`](documentation/mbt.md) for STM32CubeIDE and SRecord setup.

### Output Files

| File | Description |
|------|-------------|
| `M1_Hapax_v{ver}.elf` | ELF with debug symbols |
| `M1_Hapax_v{ver}.bin` | Raw binary |
| `M1_Hapax_v{ver}.hex` | Intel HEX |
| `M1_Hapax_v{ver}_wCRC.bin` | Binary with CRC + Hapax metadata (for SD card / OTA) |

## Coding Standards

- Follow STM32 HAL coding conventions
- Use meaningful variable and function names
- Prefer camelCase for variables/functions, UPPER_CASE for constants
- Use appropriate integer types (`uint8_t`, `uint16_t`, etc.)
- Add comments for non-obvious logic

## Architecture

All modules with submenus use the **scene-based architecture** — a stack-based scene
manager with `on_enter` / `on_event` / `on_exit` / `draw` callbacks. See
[`ARCHITECTURE.md`](ARCHITECTURE.md) for the full module table.

- **Sub-GHz** has its own scene manager (`m1_subghz_scene.h/c`) with radio-specific
  event handling.
- **All other modules** share the generic framework (`m1_scene.h/c`) with blocking
  delegates wrapping legacy functions.
- **New modules** should follow the scene pattern from the start. Use
  `m1_games_scene.c` as a minimal template.

### Saved Item Actions (mandatory for modules with saved files)

Every module that loads files from SD card **must** implement the standard saved-item
action menu following the Flipper `*_scene_saved_menu.c` pattern.  This is the
**canonical UX standard** for the project and takes precedence over other UX
preferences when they conflict.

Required core verbs: **Emulate/Send**, **Info**, **Rename**, **Delete**.

See [`CLAUDE.md`](CLAUDE.md) § "Saved Item Actions Pattern" for the full
specification, optional verbs per module, and implementation examples.

## Button Model

The M1 has a 5-way directional pad (UP/DOWN/LEFT/RIGHT/OK) plus a BACK
button.  These roles are enforced across **all** scenes:

| Button | Role |
|--------|------|
| **OK** | Primary action — start, select, confirm, send |
| **BACK** | Go back / exit current scene |
| **LEFT / RIGHT** | Change value (frequency, modulation) in selector contexts |
| **UP / DOWN** | Scroll list items, navigate menus |

**Exception:** Read Raw intercepts BACK during Recording to stop capture and
transition to Idle (preserving the file) instead of exiting.  This matches
Momentum firmware behaviour — exiting mid-capture would corrupt the file.

## UI / Button Bar Rules

These rules ensure a consistent user experience across all M1 modules.

### Button-to-column mapping (3-column bar)

The `subghz_button_bar_draw()` API provides three columns: LEFT, CENTER,
and RIGHT.  **Each column must correspond to its physical button:**

| Column | Physical button | Typical labels |
|--------|----------------|----------------|
| LEFT | LEFT button | Config, Erase, Stop |
| CENTER | OK button | REC, Stop, Send |
| RIGHT | RIGHT button | Save |

**Never** put a DOWN action label in the CENTER column or an OK action in
the RIGHT column — this creates a confusing mismatch between what the user
sees and what happens when they press a button.

If an action is triggered by UP or DOWN (which have no dedicated column),
either omit it from the button bar or use the LEFT column with a ↓ icon as
a visual hint (since DOWN is physically adjacent to LEFT on the M1 keypad).

### No "Back" labels

**Never** add "Back" as a menu item, selectable action, or button bar label.
The hardware BACK button is self-explanatory — users do not need a screen
element telling them it exists.  This applies to `subghz_button_bar_draw()`,
`m1_draw_bottom_bar()`, and any selection list / action menu array.

### No "OK" button bar on selection lists

When a scene presents a list of selectable items (main menus, action menus,
option lists), pressing OK to select is self-evident.  **Do not** draw a
bottom bar with an "OK" label — it wastes 12px of vertical space.  Instead,
use that space for additional list items and draw a **scrollbar** on the
right edge as a position indicator (3px-wide track at x = 125).

**Button bars are appropriate only** when they convey non-obvious functionality
— e.g. "OK:Download" (tells user OK starts a download), "Send", "Save",
"↓ Config" (tells user DOWN opens a config panel).

### Scrollbar pattern for selection lists

Draw a 3px-wide track frame at `M1_MENU_SCROLLBAR_X` (125) spanning the
menu area, with a filled handle whose Y position is proportional to the
selected item index.  Limit the highlight-box width to `M1_MENU_TEXT_W`
(124px) so it does not overlap the scrollbar.

## Font-Aware Menu Implementation

The user can change the menu text size in **Settings → LCD & Notifications →
Text Size**.  The setting is stored as `m1_menu_style` (declared in
`m1_system.h`, persisted via `menu_style=N` in `settings.cfg`).

| `m1_menu_style` | Label  | Row Height | Visible Items | Font |
|-----------------|--------|------------|---------------|------|
| 0 (default)     | Small  | 8 px       | 6             | `NokiaSmallPlain_tf` |
| 1               | Medium | 10 px      | 5             | `profont12_mr` |
| 2               | Large  | 13 px      | 4             | `nine_by_five_nbp_tf` |

### Helper functions (declared in `m1_scene.h`)

| Helper | Returns |
|--------|---------|
| `m1_menu_item_h()` | Row height in pixels (8, 10, or 13) |
| `m1_menu_max_visible()` | Max items in the 52px menu area (6, 5, or 4) |
| `m1_menu_font()` | Pointer to the u8g2 font for the current style |
| `M1_MENU_VIS(count)` | `min(count, m1_menu_max_visible())` — convenience macro |

### Layout constants (in `m1_scene.h`)

| Constant | Value | Purpose |
|----------|-------|---------|
| `M1_MENU_AREA_TOP` | 12 | Y below title + separator |
| `M1_MENU_AREA_H` | 52 | Available vertical space (64 − 12) |
| `M1_MENU_TEXT_W` | 124 | Highlight/text width (leaves 4px for scrollbar) |
| `M1_MENU_SCROLLBAR_X` | 125 | Scrollbar left edge |
| `M1_MENU_SCROLLBAR_W` | 3 | Scrollbar track width |

### Rules for all scrollable lists

1. **Never hardcode visible item counts or row heights.** Use
   `m1_menu_max_visible()` and `m1_menu_item_h()`.
2. **Always use `m1_menu_font()`** for list item text — do not hardcode a
   specific font constant.
3. **Use `M1_MENU_VIS(count)`** when computing the visible slice of a list.
4. **Text baseline offset is `m1_menu_item_h() − 1`** — places text 1px above
   the bottom of its row.
5. **Highlight box width must be `M1_MENU_TEXT_W` (124px)**, not 128px.
6. **Include `m1_scene.h`** in any `.c` file that draws a scrollable list.

### Justified exceptions

Some specialized displays intentionally use fixed row heights because they
are **not** standard selectable menus — they are compact data displays with
constrained vertical space:

| File | Define | Justification |
|------|--------|---------------|
| `m1_subghz_scene_read.c` | `HISTORY_ROW_H 8`, `HISTORY_VISIBLE 3` | Compact RX history in split-screen |
| `m1_subghz_scene_saved.c` | `DECODE_ROW_H 8`, `DECODE_VISIBLE 3` | Offline decode results in dual-view |
| `m1_sub_ghz.c` | `FREQ_SCANNER_VISIBLE_ROWS 5` | Frequency scanner data display |
| `m1_sub_ghz.c` | `SUBGHZ_HISTORY_ROW_HEIGHT 6`, `SUBGHZ_HISTORY_VISIBLE_ITEMS 5` | Legacy history (tiny rows for max density) |
| `m1_display.c` | `SUB_MENU_TEXT_ITEMS 4` | Legacy `m1_gui_submenu_update()` API |

## Hardware State Management

### SI4463 Radio — `menu_sub_ghz_init()` / `menu_sub_ghz_exit()`

`menu_sub_ghz_exit()` powers off the SI4463 radio completely.  After this,
the chip is unresponsive.  `menu_sub_ghz_init()` performs a full PowerUp +
patch + config reset to restore the radio.

**Rules:**
- If you call `sub_ghz_replay_flipper_file()`, call `menu_sub_ghz_init()`
  immediately after it returns (it calls `menu_sub_ghz_exit()` internally).
- If you write a new **blocking delegate**, call `menu_sub_ghz_init()` at
  the top and `menu_sub_ghz_exit()` at the bottom.
- If you write a new **RX scene**, call `menu_sub_ghz_init()` before
  starting RX — never assume the radio is already powered on.

### ESP32 Coprocessor — `m1_esp32_init()` / `m1_esp32_deinit()`

Every blocking delegate that uses the ESP32 **must** call
`m1_esp32_deinit()` on **all** exit paths — including early returns, error
paths, and normal completion.  Failing to deinit leaves the SPI transport
and GPIO interrupts active.

**Init pattern:**
```c
if (!m1_esp32_get_init_status())  m1_esp32_init();
if (!get_esp32_main_init_status()) esp32_main_init();
// ... do work ...
m1_esp32_deinit();  /* ← on EVERY exit path */
```

### NFC Worker — `Q_EVENT_NFC_*` Events

The NFC worker task (`nfc_worker_task`) uses a 4-state machine:
WAIT → INITIALIZE → PROCESS → DONE → WAIT.  Always send a stop event to
the worker before exiting an NFC blocking delegate — never rely on
`vTaskDelete()` to clean up, as that skips `nfc_deinit_func()`.

### IR Hardware — `infrared_encode_sys_init()` / `_deinit()`

Every call to `infrared_encode_sys_init()` **must** be followed by
`infrared_encode_sys_deinit()` — even if the TX complete event
(`Q_EVENT_IRRED_TX`) is not received.  Skipping deinit on timeout leaves
timer/DMA resources allocated.

## ESP32-C6 Coprocessor

The M1 communicates with the ESP32-C6 via **SPI AT commands** (not UART). Key points:

- Stock Espressif UART-based AT firmware downloads **will not work**
- ESP32 firmware must be built with `CONFIG_AT_BASE_ON_SPI=y` and `CONFIG_SPI_MODE=1`
- Module config: `ESP32C6-SPI` (not `ESP32C6-4MB` which is UART)
- UART is only used for firmware flashing (ROM bootloader), not runtime communication

## Branch Naming and Commit Messages

See [`.github/GUIDELINES.md`](.github/GUIDELINES.md) for branch naming conventions and
Conventional Commits format used in this repository.

## CI/CD

- Every push/PR to `main` triggers a firmware build check (`ci.yml`)
- Every merge to `main` auto-creates a GitHub Release with firmware artifacts
- Builds that only touch docs, databases, IDE configs, or CI workflow files are
  automatically skipped (see `paths-ignore` in `ci.yml`)

## Testing

- Test on hardware when possible
- Document test scenarios and edge cases
- Ensure NFC, RFID, and Sub-GHz functionality are verified
- **Bug fixes require regression tests** — every bug fix **must** include one or more
  host-side unit tests (under `tests/`) that would **fail before the fix** and **pass
  after the fix**.  If the buggy code is a pure-logic function, test it directly.  If
  it involves hardware-dependent code, extract the core logic into a testable helper
  and test that.  A bug fix without a corresponding regression test is incomplete.

### Running Unit Tests

```bash
cmake -B build-tests -S tests -DCMAKE_BUILD_TYPE=Debug -DENABLE_SANITIZERS=ON
cmake --build build-tests
ctest --test-dir build-tests --output-on-failure
```

### Preferred Modularization Pattern

When a source file mixes pure logic (parsers, encoders, filters, data conversion)
with hardware-coupled code (HAL, RTOS, display), **extract the pure logic into a
standalone `.c`/`.h` module**.  Use callback function pointers to decouple from
hardware when the logic needs to invoke hardware-side operations.  This makes the
logic independently testable and easier to maintain.

See `CLAUDE.md` § "Preferred Modularization Pattern" for the full specification
with examples.

### Preferred Unit Testing Pattern

All new unit tests must follow the **stub-based extraction** pattern:

1. Identify **pure-logic functions** (protocol mappings, parsers, encoders, filters,
   data conversion) in firmware source files.
2. Create **minimal header stubs** in `tests/stubs/` for HAL/RTOS/FatFS dependencies —
   provide only types and constants, not function bodies.  Reuse existing stubs where
   possible (see `tests/stubs/` for `stm32h5xx_hal.h`, `ff.h`, `irmp.h`, `lfrfid.h`,
   etc.).
3. If the function is `static`, make it non-static or extract it into a separate
   compilation unit.  For functions in HAL-heavy files, a test-only copy in
   `tests/stubs/` is acceptable when documented.
4. Write the test as `tests/test_<module>.c` using the vendored Unity framework.
5. Add a CMake target in `tests/CMakeLists.txt` linking against `unity` and
   optionally `sanitizers`.

**What to test:** protocol name→enum mappings, file format parsers, data
encode/decode, bit manipulation, string matching, filter logic.

**What NOT to test on host:** AT command construction, GPIO manipulation, RTOS task
orchestration, u8g2 rendering — these need hardware integration testing.

See `CLAUDE.md` § "Preferred Unit Testing Pattern" for the full specification with
code examples.
