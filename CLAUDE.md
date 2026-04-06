# CLAUDE.md — Standing Instructions for AI Assistants

## Project: Monstatek M1 Firmware — Hapax Fork
## Owner: hapaxx11

---

## ABSOLUTE RULES (NEVER VIOLATE)

### 1. NO Co-Author Attribution
- **NEVER** add `Co-Authored-By` lines to git commits
- **NEVER** add any AI attribution to commits, code comments, or files
- Commits must appear as if written solely by the repository owner
- This applies to ALL commits — initial, amend, fixup, squash, etc.

### 2. NO Unauthorized Remote Operations
- **NEVER** push to any remote repository without explicit permission
- **NEVER** create pull requests without explicit permission
- **NEVER** create issues, releases, or any public GitHub artifacts without explicit permission
- Default assumption: all work is LOCAL ONLY
- When pushing is approved, push ONLY to `origin` (hapaxx11/M1), never to `monstatek` (upstream)

### 3. NO Public Exposure
- **NEVER** make code, binaries, or documentation public without explicit permission
- **NEVER** fork, share, or distribute any project files without explicit permission
- Treat all project content as private/confidential by default

---

## Git Commit Rules

- Keep commit messages concise and descriptive
- No AI attribution of any kind in commit messages or trailers
- Stage specific files by name (avoid `git add -A` or `git add .`)
- Do not commit build artifacts, .bat/.ps1 helper scripts, or IDE workspace files unless asked

---

## Workflow Rules

- **Always build after code changes** — if you edit source code, you must build it yourself. Do not tell the user to build; just do it.
- **XIAO/Pico test bench: build → flash → test is YOUR job** — When working on ESP32-C6 features with the XIAO + Pico AT bridge setup, YOU are responsible for the full cycle: build the firmware, flash it to the XIAO (via COM6), and run the test script (via COM8). Do NOT stop after building and tell the user to flash/test. Only ask the user for help when something is physically outside your control (hard reset, USB replug, putting Pico in boot mode, etc.).
- **ESP32 XIAO flashing command**: `python -m esptool --chip esp32c6 --port COM6 --baud 460800 write_flash 0x0 build/factory/factory_ESP32C6-SPI-XIAO.bin` (from `D:\M1Projects\esp32-at-hid\`). After flashing, the XIAO auto-resets — wait 3-5s before testing.

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

## Deploy Locations

- **M1 Firmware**: `D:\M1Projects\m1-firmware\build\` (`.bin`, `.elf`, `.hex` outputs)
- **qMonstatek Desktop App**: `D:\M1Projects\qMonstatek\deploy\qmonstatek.exe` (Qt runtime DLLs already staged there)
- After building qMonstatek, always copy the exe to `D:\M1Projects\qMonstatek\deploy\`

---

## Build Environment

- **Toolchain**: ARM GCC 14.3 inside STM32CubeIDE 2.1.0
  - Path: `C:/ST/STM32CubeIDE_2.1.0/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.14.3.rel1.win32_1.0.100.202602081740/tools/bin/`
- **CMake**: `C:/ST/STM32CubeIDE_2.1.0/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.cmake.win32_1.1.100.202601091506/tools/bin/cmake.exe`
- **Ninja**: `C:/ST/STM32CubeIDE_2.1.0/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.ninja.win32_1.1.100.202601091506/tools/bin/ninja.exe`
- **Build command**: Set PATH to include all three tool directories, then `cmake --build build`
- **Post-build CRC + Hapax metadata**: The CMake post-build step uses `srec_cat` which is NOT installed and will fail. This is expected — the .bin/.elf/.hex files are already generated before that step. After `cmake --build` completes, run the CRC/metadata injection script. The canonical command is in `do_build.ps1` — always use it as the reference. Currently (for a local build defaulting to revision 1):
  ```
  python tools/append_crc32.py build/M1_Hapax_v0.9.0.1.bin --output build/M1_Hapax_v0.9.0.1_wCRC.bin --hapax-revision 1 --verbose
  ```
- **CRITICAL: `--hapax-revision` is MANDATORY** — without it, the Hapax metadata (revision number + build date) will NOT be injected into the binary, and the dual boot bank screen will show only the base version with no `-Hapax.X` suffix or build date. This flag must ALWAYS be included. The input binary name must match the CMake project name (e.g. `M1_Hapax_v0.9.0.1.bin`). CI patches only `FW_VERSION_RC` and `M1_HAPAX_REVISION` in the header; `CMAKE_PROJECT_NAME` is derived automatically from those values at CMake configure time. Local builds use the source-file defaults.

### qMonstatek Desktop App Build

- **Qt**: 6.4.2 MinGW 64-bit
- **Make**: `C:/Qt/Tools/mingw64/bin/mingw32-make.exe`
- **Build directory**: `D:\M1Projects\qMonstatek\build`
- **Build command**: `cd D:/M1Projects/qMonstatek/build && C:/Qt/Tools/mingw64/bin/mingw32-make.exe -j8`
- **Deploy**: Copy `build/src/qmonstatek.exe` → `deploy/qmonstatek.exe`

---

## Hardware Notes

- **MCU**: STM32H573VIT (Cortex-M33, 250MHz, 2MB flash dual-bank, 640KB RAM)
- **WiFi**: ESP32-C6 coprocessor via SPI AT commands (NOT UART — see ESP32 section below)
- **USB**: CDC + MSC composite — COM port drops during power cycle
- **Serial**: COM3 at 115200 baud
- **Debugger**: ST-Link / J-Link available for flashing
- **Flash registers**: STM32H5 uses `FLASH->NSSR` (not `FLASH->SR`), BSY bit is `FLASH_SR_BSY`

---

## ESP32-C6 Coprocessor

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

### Remotes (Monstatek vs hapaxx11)
- `monstatek` remote = Monstatek/M1 (original upstream, still at v0.8.0.0)
- `origin` remote = hapaxx11/M1 (this fork — push here when explicitly told)
- "Stock" firmware means Monstatek, NOT hapaxx11

### Upstream Merge Policy — Why We Don't Merge Monstatek

As of April 2026, **we do not merge from Monstatek/M1 upstream**. Reasons:

1. **Upstream is stale.** Monstatek/M1 has been at `v0.8.0.0` since mid-2024 with no
   public commits. There is nothing new to merge.
2. **Divergence is too large.** Hapax has rewritten the build system (CMake + Ninja
   replacing STM32CubeIDE managed makefiles), added 60+ Sub-GHz protocols, a Flipper
   file compatibility layer (`lib/furi/`), CAN bus support, ESP32-C6 SPI AT integration,
   LF-RFID / NFC / IR Flipper import, and a full CI/CD pipeline. A blind merge would
   produce hundreds of conflicts with no benefit.
3. **Version scheme divergence.** Hapax owns `FW_VERSION_MINOR` (9) and `FW_VERSION_RC`.
   Monstatek's version numbering assumptions no longer apply.
4. **Cherry-pick, don't merge.** If Monstatek ever pushes a meaningful update, the
   correct approach is to **review the diff, cherry-pick relevant changes**, and adapt
   them to the Hapax codebase — not to merge the branch wholesale.

If Monstatek publishes a new release in the future, re-evaluate this policy by:
- Fetching `monstatek/main` and inspecting the diff against our `main`
- Cherry-picking any bug fixes or HAL updates that apply
- Bumping `FW_VERSION_MAJOR` only if upstream introduces a breaking API change

---

## Versioning Scheme

- **`FW_VERSION_MINOR`** is our fork's generation number (currently `9`, matching SiN360's `0.9.x.x` scheme). This is NOT locked to Monstatek upstream — we own MINOR and RC.
- **`FW_VERSION_RC`** maps 1:1 to `M1_HAPAX_REVISION`. Both are the Hapax release counter. First release is `1`, incrementing with each CI build.
- **`FW_VERSION_MAJOR`** and **`FW_VERSION_BUILD`** remain `0` until Monstatek publishes a breaking change.
- **`Hapax` is the project codename**, NOT a version number.
- **`M1_HAPAX_REVISION`** in `m1_fw_update_bl.h` = the Hapax fork revision. Keep in sync with `FW_VERSION_RC`. Source-file default = `1` (= local build default). **CI auto-increments** by querying the latest published release tag before each build.
- **Display format**: `v{major}.{minor}.{build}.{rc}-Hapax.{hapax_revision}` — e.g. `v0.9.0.1-Hapax.1`, `v0.9.0.2-Hapax.2`, etc.
- **File/tag format**: `M1_Hapax_v{major}.{minor}.{build}.{rc}` — e.g. `M1_Hapax_v0.9.0.1_wCRC.bin`, tag `v0.9.0.1`. No `-Hapax.X` suffix in filenames or release tags.
- **CMake project name** is fully dynamic: `M1_Hapax_v{major}.{minor}.{build}.{rc}` — derived entirely at CMake configure time by reading the four `FW_VERSION_*` macros from `m1_fw_update_bl.h`. `CMakeLists.txt` is **never** patched by CI and never needs manual editing for a version bump. All output filenames (ELF, BIN, HEX, wCRC) derive from this automatically.
- **When bumping Hapax revision manually** (e.g. for a local build): update only `FW_VERSION_RC` and `M1_HAPAX_REVISION` in `m1_fw_update_bl.h` — `CMakeLists.txt` is not touched. The CI does this automatically.
- **RPC protocol**: `hapax_revision` is sent as a separate byte in `S_RPC_DeviceInfo`. qMonstatek conditionally appends the `-Hapax.X` suffix only when `hapax_revision > 0`, so stock Monstatek firmware displays without it.

For Flipper protocol import procedures (Sub-GHz, LF-RFID, NFC, IR), see
[`documentation/flipper_import_agent.md`](documentation/flipper_import_agent.md).

For hardware capability assessment (which silicon is present, which features official firmware
does not expose), see
[`documentation/hardware_schematics.md`](documentation/hardware_schematics.md).
Consult this file *secondarily* — source code and build config are primary truth.

---

## Documentation Update Rules

**Every agent session that makes code or documentation changes MUST also update the relevant
markdown files listed below.  Failing to do so is a workflow violation on par with forgetting
to build.**

### CHANGELOG.md — mandatory for every meaningful change

- **Location**: `CHANGELOG.md` (repo root)
- **Format**: [Keep a Changelog](https://keepachangelog.com/en/1.0.0/) — `### Added`,
  `### Changed`, `### Fixed`, `### Removed` subsections under the current version heading.
- **Version label**: Use `[{major}.{minor}.{build}.{rc}]` — e.g. `[0.9.0.9]`.
  The date is the wall-clock date of the change (`YYYY-MM-DD`).
- **`[Unreleased]` heading**: Use `## [Unreleased]` (no date) for changes that will
  NOT trigger a release build — i.e. changes that only touch paths in
  `build-release.yml`'s `paths-ignore` list (`.md` files, `documentation/`, `LICENSE`,
  IDE project files, CI workflow files).  When the next firmware code change merges and
  CI auto-increments the RC, fold the `[Unreleased]` entries into that version's block.
  **Never assign a numbered version heading to a change that won't produce a build** —
  this prevents version numbers from going out of sync with actual releases.
- **One entry per logical change**, not one entry per file edited.  Group related items.
- **When to add an entry**:
  - New firmware feature, protocol, or UI screen → `### Added`
  - Modification to existing behaviour, API, or protocol → `### Changed`
  - Bug fix → `### Fixed`
  - Removal of a feature or file → `### Removed`
  - Documentation / process / tooling change → `### Changed` with a "Documentation" prefix
- **When NOT to add an entry**: Pure whitespace / formatting commits with zero functional
  effect.  Every other change needs an entry.
- If the current version block already exists (e.g. the session is a follow-up fix for
  `0.9.0.1`), append to it rather than creating a new heading.
- If bumping `M1_HAPAX_REVISION` (e.g. a new CI release), create a **new** version heading at the top. The CI auto-increments the RC, so manually-authored changelog entries should use the next expected RC number.

### README.md — update when user-visible descriptions are stale

- Update the Sub-GHz protocol count if decoders are added or removed.
- Update the LF-RFID, NFC, IR, or feature lists if their descriptions no longer match
  reality.
- Update the build instructions if `CMAKE_PROJECT_NAME`, the post-build command, or
  prerequisites change.
- Update the SD card layout section if new top-level folders are introduced.

### documentation/flipper_import_agent.md — update when Flipper-derived files change

- When any file is added to `lfrfid/`, `Sub_Ghz/protocols/`, or `m1_csrc/flipper_*.c/h`:
  - Add it to the correct inventory table (Category 1 / 2 / 4).
  - Update the category subtotal.
  - Update the Grand Total table (file count + directly-copied count).
  - Update the threshold note if the directly-copied count changes significantly.
- When files are removed, remove them from the table and adjust totals accordingly.

### ARCHITECTURE.md — update when project structure changes

- Add new top-level directories to the component table.
- Update the Build System section if new build backends or CMake presets are introduced.

### DEVELOPMENT.md — update when build process or coding standards change

- Update build commands, environment requirements, or workflow rules if they change.
- Keep this in sync with the canonical `CLAUDE.md` build instructions.

---

## Architecture Rules

- **S_M1_FW_CONFIG_t** struct is EXACTLY 20 bytes — NEVER modify it
- CRC extension data lives at fixed offsets AFTER the struct (offset 20+)
- All Flipper file parsers use stack allocation (no heap/malloc)
- FreeRTOS headers must be included before stream_buffer.h / queue.h
- Flipper parser API: functions are named `flipper_*_load()` / `flipper_*_save()`, return `bool`

---

## UI / Button Bar Rules

- **NEVER add "Back" as a menu item or button bar label.** The back button is self-explanatory
  — users do not need a screen element telling them that pressing back goes back. This applies
  to `subghz_button_bar_draw()`, `m1_draw_bottom_bar()`, hand-drawn bottom bars, and any
  selection list / action menu array.
- **NEVER add "Back" as a selectable item in action menus.** If a menu has items like
  "Emulate / Rename / Delete", do NOT append "Back" — the hardware back button already
  handles navigation. Adding it wastes a menu slot and screen space.
- **Button bars should only show actionable hints** — labels for directional buttons (↓ Config,
  OK:Listen, LR:Change, etc.) that tell the user what *non-obvious* actions are available.
  Pass `NULL, NULL` for the left slot instead of showing a back arrow + label.
- If a bottom bar would be entirely empty after removing "Back", that is acceptable — an
  empty bar is better than a redundant one.
- **Selection lists MUST NOT have a button bar with "OK".** When a scene presents a list
  of selectable items (main menus, action menus, option lists), pressing OK to select is
  self-evident.  Do NOT draw `subghz_button_bar_draw()` or `m1_draw_bottom_bar()` with an
  "OK" label at the bottom — it wastes 12px of vertical space and looks wrong.  Instead,
  use that space for additional list items and draw a **scrollbar** on the right edge as a
  position indicator.  Reference implementation: `m1_subghz_scene_menu.c`.
- **Scrollbar pattern for selection lists**: Draw a 3px-wide track frame at x=125 spanning
  the menu area, with a filled handle whose Y position is proportional to the selected
  item index.  Limit the highlight-box width to 124px so it does not overlap the scrollbar.
- **Button bars are appropriate ONLY when they convey non-obvious functionality** — e.g.
  "OK:Browse" (tells user OK opens a file browser), "↓ Config" (tells user down opens
  config), "Send" / "Save" (action confirmation in radio scenes).
- **When adding or removing menu items, redistribute spacing.** If the item count changes,
  recompute the row height so items fill the available vertical zone evenly.  Calculate:
  `row_height = available_zone_height / item_count`.  The available zone is between the
  separator line (or header bottom) and the bottom bar (y=52 if a bar is shown, y=64 if not).
  Adjust the highlight box height and text baseline offset to match the new row height.

---

## Scene-Based Application Architecture

**The scene manager is the target architecture for ALL M1 application modules.**
Every module that has more than one screen or any non-trivial navigation must use
scene-based architecture.  The `SubGhzSceneHandlers` pattern
(`on_enter` / `on_event` / `on_exit` / `draw` callbacks with stack-based push/pop
navigation) keeps state management clean, testable, and consistent across the device.

### Migration status

Sub-GHz is the **reference implementation** — it has a complete scene manager
(`m1_subghz_scene.h/c`) with 17 scenes.  All other modules still use the legacy
`S_M1_Menu_t` menu system in `m1_menu.c`.  They must be migrated to the scene
pattern progressively.

| Module | Current | Target | Entry point |
|--------|---------|--------|-------------|
| **Sub-GHz** | ✅ Scene manager | Done | `sub_ghz_scene_entry()` → `m1_subghz_scene.c` |
| **125KHz RFID** | ❌ Legacy menu | Scene manager | `m1_rfid.c` functions |
| **NFC** | ❌ Legacy menu | Scene manager | `m1_nfc.c` functions |
| **Infrared** | ❌ Legacy menu | Scene manager | `m1_infrared.c` functions |
| **GPIO** | ❌ Legacy menu | Scene manager | `m1_gpio.c` functions |
| **WiFi** | ❌ Legacy menu | Scene manager | `m1_wifi.c` functions |
| **Bluetooth** | ❌ Legacy menu | Scene manager | `m1_bt.c` functions |
| **BadUSB** | ❌ Legacy menu | Scene manager | `m1_badusb.c` functions |
| **Games** | ❌ Legacy menu | Scene manager | `m1_games.c` functions |
| **Settings** | ❌ Legacy menu | Scene manager | `m1_settings.c` functions |

### Agent instructions for scene migration

When an agent is tasked with **any work on a legacy-menu module** (bug fix, new
feature, UI change), the agent **SHOULD** migrate that module to the scene
architecture as part of the same change, if the scope is reasonable.  At minimum:

1. **Create a scene manager** for the module:
   - `m1_<module>_scene.h` — scene ID enum, event enum, app context struct,
     handler table extern declarations.  Copy structure from `m1_subghz_scene.h`.
   - `m1_<module>_scene.c` — scene registry, push/pop/replace, main event loop.
     Copy structure from `m1_subghz_scene.c`.

2. **Create a menu scene** (`m1_<module>_scene_menu.c`) that lists all module
   functions using the scrollbar pattern from `m1_subghz_scene_menu.c`.

3. **Wrap existing functions** as blocking-delegate scenes.  Each legacy function
   that runs its own event loop gets a thin scene file:
   ```c
   static void scene_on_enter(AppContext *app) {
       legacy_function();          /* blocking — runs own event loop */
       app->running = true;        /* prevent premature exit */
       module_scene_pop(app);      /* return to parent scene */
   }
   ```
   This is the fastest migration path — no rewrite of the legacy function needed.

4. **Update `m1_menu.c`** so the module's top-level menu entry calls the new
   scene entry point instead of expanding the legacy sub-menu.

5. **Scene-native rewrites** of individual functions can happen later, one at a
   time.  The blocking-delegate wrapper is an acceptable permanent state until
   someone has reason to refactor the underlying function.

### Rules for all scene menus

- Scene menus use the scrollbar pattern (no "OK" button bar).
- Never add "Back" as a menu item or button bar label.
- When item count changes, verify `MENU_VISIBLE` and scrolling math.
- Every scene file must be added to `cmake/m1_01/CMakeLists.txt`.

---

## Sub-GHz Menu Structure

The Sub-GHz scene menu (`m1_subghz_scene_menu.c`) must contain **exactly 11 items** in this
order, matching C3 parity.  Do not remove items, reorder, or add "Back" entries.

| # | Label | Scene ID | Implementation |
|---|-------|----------|----------------|
| 1 | Read | SubGhzSceneRead | Scene-native (protocol decode) |
| 2 | Read Raw | SubGhzSceneReadRaw | Scene-native (raw capture) |
| 3 | Saved | SubGhzSceneSaved | Scene-native (file browser) |
| 4 | Playlist | SubGhzScenePlaylist | Blocking delegate |
| 5 | Frequency Analyzer | SubGhzSceneFreqAnalyzer | Blocking delegate → `sub_ghz_frequency_reader()` |
| 6 | Spectrum Analyzer | SubGhzSceneSpectrumAnalyzer | Blocking delegate → `sub_ghz_spectrum_analyzer()` |
| 7 | RSSI Meter | SubGhzSceneRssiMeter | Blocking delegate → `sub_ghz_rssi_meter()` |
| 8 | Freq Scanner | SubGhzSceneFreqScanner | Blocking delegate → `sub_ghz_freq_scanner()` |
| 9 | Weather Station | SubGhzSceneWeatherStation | Blocking delegate → `sub_ghz_weather_station()` |
| 10 | Brute Force | SubGhzSceneBruteForce | Blocking delegate → `sub_ghz_brute_force()` |
| 11 | Add Manually | SubGhzSceneAddManually | Blocking delegate → `sub_ghz_add_manually()` |

**"Blocking delegate"** scenes call a legacy function that runs its own event loop and
drawing.  The thin scene wrapper (`m1_subghz_scene_<name>.c`) calls the function in
`on_enter`, then pops itself when the function returns.  This integrates legacy code with
the scene manager without rewriting each tool.

---

## Remote Configuration

- `origin` = hapaxx11/M1 (this fork — push here when explicitly told)
- `bedge117` = bedge117/M1 (upstream C3 reference, DO NOT PUSH)
- `sincere360` = sincere360/M1_SiN360 (upstream SiN360 reference, DO NOT PUSH)
- `monstatek` = Monstatek/M1 (upstream reference, DO NOT PUSH)
- Feature branch: `copilot/update-flipper-apps-and-references`

---

## Public Forks Tracker

Track all known public forks of Monstatek/M1 so agents can determine whether
a fresh analysis is warranted.  All timestamps are **UTC**.

### Known Forks

| Fork | Owner | Activity | Latest Commit (SHA) | Latest Commit Date (UTC) | Last Reviewed by Hapax | Notes |
|------|-------|----------|---------------------|--------------------------|------------------------|-------|
| [Monstatek/M1](https://github.com/Monstatek/M1) | Monstatek | **Active** (upstream) | `217ca99b` | 2026-04-01 16:38 | 2026-04-02 03:21 | Original upstream. Was stale at v0.8.0.0 for months; just pushed v0.8.0.1 binary. Cherry-pick only. |
| [bedge117/M1](https://github.com/bedge117/M1) | bedge117 | **Active** | `8842866048f7` | 2026-03-26 05:17 | 2026-04-06 05:42 | C3 enhanced firmware (C3.12). All features imported: PicoPass, 56 protocols (Hapax has 100), NFC overhaul, SI4463 32MHz fix, RTC/NTP, BadUSB forced type, AES custom key, choice dialog, ISM persistence, SD RPC. Only unmerged: experimental feature/bad-bt (BLE HID, requires custom ESP32 FW). |
| [sincere360/M1_SiN360](https://github.com/sincere360/M1_SiN360) | sincere360 | **Active** | `997c9ce099` | 2026-03-20 02:02 | 2026-04-02 03:21 | v0.9 lineage — LCD settings, IR remote, screen orientation. Hapax version scheme derived from SiN360. |
| [rgomez31UAQ/Monstatek-M1_STM32H573VIT6_Firmware](https://github.com/rgomez31UAQ/Monstatek-M1_STM32H573VIT6_Firmware) | rgomez31UAQ | Inactive | `024b4c16` | 2026-02-27 19:25 | 2026-04-02 03:21 | Fork of stock + build doc PR. No custom firmware work. |
| [steveAG/monstatek-m1](https://github.com/steveAG/monstatek-m1) | steveAG | Inactive | `2df97efc` | 2026-02-20 21:22 | 2026-04-02 03:21 | Mirror of stock at time of fork. No custom commits. |
| [fengjuan0/Monstatek-M1](https://github.com/fengjuan0/Monstatek-M1) | fengjuan0 | Inactive | `2df97efc` | 2026-02-24 03:45 | 2026-04-02 03:21 | Mirror of stock at time of fork. No custom commits. |

### How to Update This Table

1. **When to check**: Before starting any task that involves porting features,
   comparing implementations, or auditing upstream changes.  Also check
   periodically (roughly monthly) or when the user asks.
2. **Discover new forks**: Search GitHub for `M1 in:name fork:true monstatek`
   and for forks of related repos (`bedge117/m1-sdk`,
   `bedge117/esp32-at-monstatek-m1`).  Add any new public fork to the table.
3. **Update existing rows**: For each **Active** fork, fetch the latest commit
   SHA and date.  Compare with the "Latest Commit" column.  If the SHA has
   changed, the fork has new work — update the SHA, date, and bump "Last
   Reviewed by Hapax" to the current UTC timestamp after reviewing.
4. **Classify activity**:
   - **Active** — fork has custom commits beyond stock Monstatek and has been
     updated within the last 60 days.
   - **Inactive** — no custom commits, or last update > 60 days ago.
5. **Record cherry-picks**: When a commit or feature is cherry-picked from a
   fork into Hapax, note it in the "Notes" column (e.g. "C3.4 crash fix
   cherry-picked").
6. **Never remove rows** — mark forks as Inactive instead, so agents know
   the fork was already evaluated.
