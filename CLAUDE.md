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
- **Do NOT build for non-compilation changes** — if a session only modifies files that
  do not affect compilation (`.md` files, `documentation/`, `ir_database/`,
  `subghz_database/`, `subghz_playlist/`, `LICENSE`, `COPYING.txt`, IDE project files
  like `.vscode/`, `.settings/`, `.project`, `.cproject`, `.mxproject`, or CI workflow
  files in `.github/`), a firmware build is **not required**.  These paths match the
  `paths-ignore` lists in both `ci.yml` and `build-release.yml` — CI will also skip
  the build job for such changes.
- **XIAO/Pico test bench: build → flash → test is YOUR job** — When working on ESP32-C6 features with the XIAO + Pico AT bridge setup, YOU are responsible for the full cycle: build the firmware, flash it to the XIAO (via COM6), and run the test script (via COM8). Do NOT stop after building and tell the user to flash/test. Only ask the user for help when something is physically outside your control (hard reset, USB replug, putting Pico in boot mode, etc.).
- **ESP32 XIAO flashing command**: `python -m esptool --chip esp32c6 --port COM6 --baud 460800 write_flash 0x0 build/factory/factory_ESP32C6-SPI-XIAO.bin` (from `D:\M1Projects\esp32-at-hid\`). After flashing, the XIAO auto-resets — wait 3-5s before testing.
- **Bug fixes require regression tests** — every bug fix **MUST** include one or more
  host-side unit tests (under `tests/`) that **fail before the fix and pass after it**.
  If the buggy code is a pure-logic function that can be tested on the host, write the
  test directly.  If the bug involves hardware-dependent code, extract the core logic
  into a testable helper and test that.  A bug fix without a corresponding regression
  test is incomplete — do not consider the fix done until the test exists and passes.
  This rule applies to both human contributors and AI agents.

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

#### What NOT to unit test

- **AT command construction** (`m1_wifi.c`, `m1_bt.c`, `m1_ble_spam.c`) — entirely
  ESP32 SPI communication, no pure logic to extract.
- **Direct HAL GPIO manipulation** (`m1_gpio.c`) — hardware-only.
- **UI rendering code** — display drawing is tightly coupled to u8g2 state.
- **RTOS task orchestration** — queue/semaphore interactions need the real scheduler.

These modules are tested via hardware integration, not host-side unit tests.

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
- **Post-build CRC + Hapax metadata**: The CMake `POST_BUILD` step automatically runs `tools/append_crc32.py` via Python to inject the CRC32 checksum and Hapax metadata (revision number + build date) into the binary, producing `${CMAKE_PROJECT_NAME}_wCRC.bin`. No manual step is needed when building with CMake. For non-CMake builds (STM32CubeIDE, bare Makefile), run the script manually:
  ```
  python tools/append_crc32.py build/M1_Hapax_v<VERSION>.bin --output build/M1_Hapax_v<VERSION>_wCRC.bin --hapax-revision 1 --verbose
  ```
  Replace `<VERSION>` with the current version from `m1_fw_update_bl.h` (e.g. `0.9.0.1`).
- **CRITICAL: `--hapax-revision` is MANDATORY** — without it, the Hapax metadata (revision number + build date) will NOT be injected into the binary, and the dual boot bank screen will show only the base version with no `-Hapax.X` suffix or build date. This flag must ALWAYS be included. CI patches only `FW_VERSION_RC` and `M1_HAPAX_REVISION` in the header; `CMAKE_PROJECT_NAME` is derived automatically from those values at CMake configure time. Local builds use the source-file defaults.

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

### Firmware Source
- **Source repo**: [`bedge117/esp32-at-monstatek-m1`](https://github.com/bedge117/esp32-at-monstatek-m1) (C3 custom AT firmware)
- **Deauth fork**: [`neddy299/esp32-at-monstatek-m1`](https://github.com/neddy299/esp32-at-monstatek-m1) (adds `AT+DEAUTH` + `AT+STASCAN`)
- Both are forks of Espressif's official `esp-at`, customised for M1's SPI transport
- Pre-built binaries available on the GitHub Releases pages of both repos
- See [`documentation/esp32_firmware.md`](documentation/esp32_firmware.md) for the full reference

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
- **File/tag format**: `M1_Hapax_v{major}.{minor}.{build}.{rc}` — e.g. `M1_Hapax_v{ver}_wCRC.bin`, tag `v{ver}`. No `-Hapax.X` suffix in filenames or release tags.
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

- **Location**: `CHANGELOG.md` (repo root).
- **Format**: [Keep a Changelog](https://keepachangelog.com/en/1.0.0/) — `### Added`,
  `### Changed`, `### Fixed`, `### Removed` subsections.
- **Version label**: `[{major}.{minor}.{build}.{rc}]` — e.g. `[0.9.0.9]`.
  The date is the wall-clock date of the change (`YYYY-MM-DD`).

#### Fragment-based workflow (required)

> **DO NOT edit `CHANGELOG.md` directly on feature branches.**  Create a
> changelog **fragment file** instead.  This eliminates merge conflicts when
> multiple branches are in flight simultaneously.

- **Create a fragment file** in `.changelog/` for each logical change:
  ```
  .changelog/<short-description>.<category>.md
  ```
  where `<category>` is one of: `added`, `changed`, `fixed`, `removed`.
- **File content**: the bullet-point text for the entry — one entry per file.
  The leading `- ` dash is optional (the assembler normalises it).
  Multi-line entries are supported; indent continuation lines by 2 spaces.
  ```
  # .changelog/ir-quick-remotes.added.md
  **Infrared: Universal Remote quick-access remotes** — Momentum-style category
    quick-remotes accessible directly from the Universal Remote dashboard.
  ```
- **Multiple entries** in one file are separated by a blank line.
- **Naming**: use lowercase, hyphens, keep it short.  The description is for
  humans browsing the directory — it does not appear in the changelog.
- **CI assembly**: `build-release.yml` runs `scripts/assemble_changelog.py`
  at release time, which reads all fragment files, groups entries by category,
  merges them into the `[Unreleased]` section of `CHANGELOG.md`, deletes the
  fragment files, then stamps the version heading.
- **Local preview**: `python scripts/assemble_changelog.py --preview` shows
  what the assembled output would look like without modifying any files.

#### Category rules

- **When to add a fragment**:
  - New firmware feature, protocol, or UI screen → `<desc>.added.md`
  - Modification to existing behaviour, API, or protocol → `<desc>.changed.md`
  - Bug fix → `<desc>.fixed.md`
  - Removal of a feature or file → `<desc>.removed.md`
  - Documentation / process / tooling change → `<desc>.changed.md`
    with a "Documentation:" prefix in the entry text
- **When NOT to add a fragment**: Pure whitespace / formatting commits with
  zero functional effect.  Every other change needs a fragment.
- **One fragment per logical change**, not one per file edited.  Group related
  items into a single fragment when they form a cohesive feature or fix.

#### Rules that still apply

- Do **not** manually create versioned headings in `CHANGELOG.md` — CI stamps
  the version on release.
- **Retroactive changelog edits** to already-released sections are permitted
  **only** when the change increases accuracy of the released content AND is
  in the public interest and non-harmful.
- **CI stamper safety rule**: The stamper replaces the first occurrence of
  the `[Unreleased]` heading marker.  Never write that literal markdown
  heading string in changelog entry body text, fragment files, or code
  spans.  When referring to the heading in prose, write `[Unreleased]`
  (without the `## ` prefix).
- **Changelog stamp PRs are NOT changelog-worthy.**  The CI workflow creates a
  PR titled `changelog: stamp [Unreleased] as <tag>` after every release.
  These automated stamp PRs **MUST NOT** appear in changelog entries, release
  notes, or PR descriptions.  They are infrastructure housekeeping — they do
  not represent a user-visible change.  If you see one in auto-generated
  release notes, the `.github/release.yml` label exclusion has failed and
  should be investigated.

#### Backward compatibility

  The assembly script also preserves any entries that already exist under
  `[Unreleased]` in `CHANGELOG.md` (e.g. from legacy direct edits).  New
  fragment entries are merged into the correct category sections alongside
  existing entries.  Over time, all contributors should migrate to fragments.

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

### CLAUDE.md Font Inventory — update when fonts change

- When adding, removing, or changing any font in `m1_display.h` macros or the
  `m1_app_api.c` font table, update the **Font Inventory** table in the
  Architecture Rules section of this file.
- See the **Font Maintenance Rules** subsection for the full checklist.

---

## Architecture Rules

- **S_M1_FW_CONFIG_t** struct is EXACTLY 20 bytes — NEVER modify it
- CRC extension data lives at fixed offsets AFTER the struct (offset 20+)
- All Flipper file parsers use stack allocation (no heap/malloc)
- FreeRTOS headers must be included before stream_buffer.h / queue.h
- Flipper parser API: functions are named `flipper_*_load()` / `flipper_*_save()`, return `bool`

### Font Inventory

The M1 firmware compiles ~1987 u8g2 fonts in `Drivers/u8g2_csrc/u8g2_fonts.c`.
The table below inventories fonts referenced by application code via
`m1_csrc/m1_display.h` (display-role macros),
`m1_csrc/m1_app_api.c` (BadUSB scripting API table), and direct
`u8g2_SetFont()` call sites.  It may also include a separate
"defined but unused" entry, which is not necessarily linked into the final
binary when section garbage collection is enabled.

**u8g2 font suffix meanings:**

| Suffix | Charset | Notes |
|--------|---------|-------|
| `_tr` | Printable ASCII 32–127 (transparent mode) | Has lowercase, most common |
| `_tf` | Printable ASCII 32–127 (filled mode) | Has lowercase, filled background |
| `_tu` | Uppercase only 32–90 (transparent) | **No lowercase** |
| `_tn` | Numbers + punctuation 32–57 (transparent) | Digits only |
| `_mr` / `_mf` | Monospaced, reduced / full | Fixed-width |
| `_8r` / `_8f` | Full 256-char set | Extended Latin |
| `_t_all` | Full Unicode set | Large flash footprint |

**Active font table — updated 2026-04-11:**

| Font | Macro / API | Role | Ascent | Lowercase |
|------|-------------|------|--------|-----------|
| `u8g2_font_resoledmedium_tr` | `M1_DISP_MAIN_MENU_FONT_N` + API | Main menu normal text | 7 | Yes |
| `u8g2_font_helvB08_tf` | `M1_DISP_MAIN_MENU_FONT_B` + API | Main menu bold text | 8 | Yes |
| `u8g2_font_helvB08_tr` | `M1_POWERUP_LOGO_FONT` + API | Splash screen "M1 Hapax" | 8 | Yes |
| `u8g2_font_NokiaSmallPlain_tf` | `M1_DISP_SUB_MENU_FONT_N` + API | Sub-menu normal text | 7 | Yes |
| `u8g2_font_squeezed_b7_tr` | `M1_DISP_SUB_MENU_FONT_B` + API | Sub-menu bold text | 7 | Yes |
| `u8g2_font_profont12_mr` | `M1_DISP_FUNC_MENU_FONT_N` + API | Function menu monospace (Medium) | 8 | Yes |
| `u8g2_font_spleen5x8_mf` | API only | Legacy medium font (kept for script compat) | 6 | Yes |
| `u8g2_font_nine_by_five_nbp_tf` | `M1_DISP_FUNC_MENU_FONT_N2` + API | Function menu larger | 9 | Yes |
| `u8g2_font_courB08_tf` | `M1_DISP_RUN_MENU_FONT_B` + API | Running menu bold | 6 | Yes |
| `u8g2_font_Terminal_tr` | `M1_DISP_RUN_ERROR_FONT_1B` + API | Error messages | 9 | Yes |
| `u8g2_font_lubB08_tf` | `M1_DISP_RUN_ERROR_FONT_2B` + API | Error messages bold | 9 | Yes |
| `u8g2_font_victoriabold8_8r` | `M1_DISP_RUN_WARNING_FONT_1B` | Power warnings (wide bold) | 8 | Yes |
| `u8g2_font_pcsenior_8f` | `M1_DISP_RUN_WARNING_FONT_2B` + API | Warning messages | 7 | Yes |
| `u8g2_font_samim_10_t_all` | `M1_DISP_RUN_WARNING_FONT_3` | **Defined but unused** | — | Yes |
| `u8g2_font_profont17_tr` | `M1_DISP_LARGE_FONT_1B` + API | Large readable text | 11 | Yes |
| `u8g2_font_VCR_OSD_tu` | `M1_DISP_LARGE_FONT_2B` + API | Large uppercase display | 12 | **No** |
| `u8g2_font_spleen8x16_mf` | `M1_DISP_LARGE_FONT_3B` + API | Large monospace | 11 | Yes |
| `u8g2_font_Pixellari_tu` | `M1_MAIN_LOGO_FONT_1B` + API | Main logo / heading | 10 | **No** |
| `u8g2_font_4x6_tr` | API only | BadUSB tiny text | 5 | Yes |
| `u8g2_font_5x8_tr` | API only | BadUSB small text | 6 | Yes |
| `u8g2_font_6x10_tr` | API only | BadUSB small readable | 7 | Yes |
| `u8g2_font_10x20_mr` | API only | BadUSB large monospace | 14 | Yes |
| `u8g2_font_finderskeepers_tf` | API only | BadUSB game-style | 7 | Yes |

**Where the font is bound determines which code files must be updated when
changing it:**

- **`m1_display.h` macro** → changing the macro automatically updates all callers.
- **`m1_app_api.c` table** → the BadUSB scripting engine looks up fonts by string
  name at runtime.  If a font is removed from the table, existing `.m1app` scripts
  referencing it by name will fail.

### Font Maintenance Rules

1. **When adding a new font**: add it to the table above, add it to the
   `m1_app_api.c` font table if BadUSB scripts should be able to use it, and
   verify it is declared in `u8g2_fonts.c` (all 1987 fonts are already compiled;
   you only need to add a new entry if you bring in a font from an external
   source).
2. **When changing `M1_POWERUP_LOGO_FONT`**: the replacement font's `1` glyph
   must be visually distinct from its `I` glyph — otherwise "M1" reads as "MI"
   on the splash screen.  Prefer fonts whose `1` has a top hook or base serif.
3. **When removing a font**: remove it from this table, from the `m1_app_api.c`
   table, and from the `m1_display.h` macro (if applicable).  Note that removing
   a font from `m1_app_api.c` breaks any `.m1app` scripts that reference it.
4. **Keep this table current**: any agent session that modifies `m1_display.h`
   font macros, `m1_app_api.c` font table entries, or introduces a new
   `u8g2_SetFont()` call with a font not already listed here **MUST** update
   this table before the session ends.
5. **Suffix awareness**: when choosing a font for a context that displays
   lowercase text (menus, filenames, version strings), **never** use a `_tu` or
   `_tn` suffix font — those lack lowercase glyphs.  Use `_tr`, `_tf`, `_mr`,
   or `_mf` instead.

### User-Configurable Font Size — `m1_menu_style`

The user can change the menu text size in **Settings → LCD & Notifications → Text Size**.
The setting is stored as `m1_menu_style` (declared in `m1_system.h`, defined in
`m1_system.c`, persisted via `menu_style=N` in `settings.cfg`).

| `m1_menu_style` | Label  | Row Height | Visible Items | Font |
|-----------------|--------|------------|---------------|------|
| 0 (default)     | Small  | 8 px       | 6             | `M1_DISP_SUB_MENU_FONT_N` (`NokiaSmallPlain`) |
| 1               | Medium | 10 px      | 5             | `M1_DISP_FUNC_MENU_FONT_N` (`profont12`) |
| 2               | Large  | 13 px      | 4             | `M1_DISP_FUNC_MENU_FONT_N2` (`nine_by_five_nbp`) |

**Helpers** (declared in `m1_scene.h`, defined in `m1_scene.c`):

| Helper | Returns |
|--------|---------|
| `m1_menu_item_h()` | Row height in pixels (8, 10, or 13) |
| `m1_menu_max_visible()` | Max items that fit in the 52px menu area (6, 5, or 4) |
| `m1_menu_font()` | Pointer to the u8g2 font for the current style |
| `M1_MENU_VIS(count)` | `min(count, m1_menu_max_visible())` — convenience macro |

**Layout constants** (in `m1_scene.h`):

| Constant | Value | Purpose |
|----------|-------|---------|
| `M1_MENU_AREA_TOP` | 12 | Y coordinate below title + separator |
| `M1_MENU_AREA_H` | 52 | Available vertical space (64 − 12) |
| `M1_MENU_TEXT_W` | 124 | Highlight/text area width (leaves 4px for scrollbar) |
| `M1_MENU_SCROLLBAR_X` | 125 | Scrollbar left edge |
| `M1_MENU_SCROLLBAR_W` | 3 | Scrollbar track width |

#### Rules for all scrollable lists

1. **NEVER hardcode visible item counts or row heights.**  Use `m1_menu_max_visible()`
   and `m1_menu_item_h()` instead of `#define LIST_VISIBLE_ITEMS 4` or
   `#define LIST_ITEM_HEIGHT 9`.
2. **ALWAYS use `m1_menu_font()`** for list item text.  Do not hardcode
   `M1_DISP_FUNC_MENU_FONT_N` or `M1_DISP_SUB_MENU_FONT_N` — the correct font
   depends on the user's text size setting.
3. **Use `M1_MENU_VIS(count)`** when computing the visible slice of a list.
4. **The text baseline offset is `m1_menu_item_h() − 1`** — this places text
   1px above the bottom of its row.
5. **Highlight box width must be `M1_MENU_TEXT_W` (124px)**, not 128px, to leave
   room for the scrollbar.
6. **Include `m1_scene.h`** in any `.c` file that draws a scrollable list.
7. **When importing features from other repositories** (Flipper, C3, SiN360, or any
   fork), the imported code will almost certainly hardcode fonts and row heights.
   These **must** be converted to the Hapax font-aware helpers before merging.
   Neither Flipper nor upstream Monstatek have a user-configurable text size
   feature — every imported scrollable list needs adaptation.  This is a
   **blocking merge requirement**, not a follow-up task.

#### Exceptions (justified hardcoded values)

Some specialized displays intentionally use fixed row heights because they are
**not** standard selectable menus — they are compact data displays with
constrained vertical space:

| File | Define | Justification |
|------|--------|---------------|
| `m1_subghz_scene_read.c` | `HISTORY_ROW_H 8`, `HISTORY_VISIBLE 3` | Compact RX history in split-screen (RSSI bar + history + bottom bar) |
| `m1_subghz_scene_saved.c` | `DECODE_ROW_H 8`, `DECODE_VISIBLE 3` | Offline decode results in dual-view layout |
| `m1_sub_ghz.c` | `FREQ_SCANNER_VISIBLE_ROWS 5` | Frequency scanner data display, not a selectable menu |
| `m1_sub_ghz.c` | `SUBGHZ_HISTORY_ROW_HEIGHT 6`, `SUBGHZ_HISTORY_VISIBLE_ITEMS 5` | Legacy history (tiny rows for maximum density) |

### SI4463 Radio State Management — `menu_sub_ghz_init()` / `menu_sub_ghz_exit()`

> **Every caller of a function that powers off the SI4463 MUST restore radio state
> before using the radio again.**  This is the single most common source of
> "radio works in tool X but not in tool Y" bugs.

**Background.** `menu_sub_ghz_exit()` deasserts the SI4463 ENA pin, completely
powering off the radio.  After this, the chip is unresponsive — SPI commands
return garbage, RX captures zero edges, TX produces no output.
`menu_sub_ghz_init()` calls `radio_init_rx_tx()` with a full PowerUp + patch
load + config reset, restoring the radio to a known good state.

**The pattern:**

1. **Blocking delegates** (Spectrum Analyzer, Freq Scanner, RSSI Meter,
   Weather Station, Brute Force, Add Manually, Frequency Reader) each call
   `menu_sub_ghz_init()` at their top and `menu_sub_ghz_exit()` at their
   bottom.  They own their own radio lifecycle — this is correct.

2. **`sub_ghz_replay_flipper_file()`** calls `menu_sub_ghz_exit()` before
   returning.  **Every call site must call `menu_sub_ghz_init()` afterwards**:
   - `m1_subghz_scene_saved.c` — emulate action handler (line ~74)
   - `m1_subghz_scene_playlist.c` — `playlist_transmit_next()` after each file

3. **Scene-native RX starters** (`start_rx()` in Read, `start_raw_rx()` in
   Read Raw) call `menu_sub_ghz_init()` before configuring RX, so they
   recover from any prior powered-off state regardless of which scene
   the user was in before.

**Rules for new code:**
- If you call `sub_ghz_replay_flipper_file()`, add `menu_sub_ghz_init()`
  immediately after it returns.
- If you write a new blocking delegate, call `menu_sub_ghz_init()` at the
  top and `menu_sub_ghz_exit()` at the bottom.
- If you write a new RX scene, call `menu_sub_ghz_init()` before starting
  RX — never assume the radio is already powered on.
- **Never assume radio state is preserved across scene transitions** — a
  blocking delegate may have run and powered off the radio between the
  user's last scene and yours.

### ESP32 Coprocessor State Management — `m1_esp32_init()` / `m1_esp32_deinit()`

> **Every blocking delegate that uses the ESP32 MUST call `m1_esp32_deinit()`
> on ALL exit paths — including early returns, error paths, and normal
> completion.**  Failing to deinit leaves the SPI transport and GPIO
> interrupts active, wasting power and potentially interfering with
> subsequent ESP32 operations from other modules.

**Background.** The ESP32-C6 coprocessor communicates with the STM32 via SPI.
Two initialization layers exist:

1. `m1_esp32_init()` — HAL-level SPI peripheral + GPIO + interrupt setup
   (in `m1_esp32_hal.c`).  Sets `esp32_init_done = TRUE`.
2. `esp32_main_init()` — Creates the `spi_trans_control_task` RTOS task
   and sets `esp32_main_init_done = true` (in `esp_app_main.c`).  The task
   persists across init/deinit cycles — the flag prevents duplicate creation.

`m1_esp32_deinit()` tears down the SPI peripheral, disables interrupts, and
resets `esp32_init_done`.  The SPI control task remains alive but dormant
(blocked on a queue with no interrupts firing).

**The pattern for ESP32-using blocking delegates:**

```c
void my_esp32_operation(void)
{
    /* Init ESP32 if needed */
    if (!m1_esp32_get_init_status())
        m1_esp32_init();
    if (!get_esp32_main_init_status())
        esp32_main_init();

    if (!get_esp32_main_init_status())
    {
        show_error("ESP32 not ready!");
        m1_esp32_deinit();   /* ← REQUIRED even on failure */
        return;
    }

    /* ... do work ... */

    m1_esp32_deinit();       /* ← REQUIRED on every exit path */
}
```

**Modules that follow this pattern:**
- WiFi: `wifi_scan_ap()`, `wifi_show_status()`, `wifi_disconnect()`,
  `wifi_ntp_sync()`
- Bluetooth: `bluetooth_scan()`, `bluetooth_advertise()`
- 802.15.4: `ieee802154_scan()` (Zigbee/Thread)
- BLE Spam: `ble_spam_run()`
- Bad-BT: `badbt_run()` — also calls `ble_hid_deinit()` before ESP32 deinit

**Rules for new code:**
- If you call `m1_esp32_init()` or `esp32_main_init()`, you MUST call
  `m1_esp32_deinit()` on every return path — including error early returns.
- `m1_esp32_deinit()` is safe to call even if init was not fully successful
  (it checks `esp32_init_done` internally).
- **Never leave the ESP32 SPI transport initialized after a blocking
  delegate returns** — the caller's scene does not know which ESP32 mode
  was active and cannot clean up for you.

### NFC Worker State Management — `Q_EVENT_NFC_*` Events

> **The NFC worker task's state machine must handle ALL stop events sent
> to it.**  An unhandled event leaves the worker in `NFC_STATE_PROCESS`
> indefinitely, preventing `nfc_deinit_func()` from running and leaving
> `EN_EXT_5V` asserted.

**Background.** The NFC worker runs in a dedicated RTOS task
(`nfc_worker_task` in `nfc_driver.c`) with a 4-state machine:
`WAIT → INITIALIZE → PROCESS → DONE → WAIT`.  Transitions from PROCESS
to DONE are driven by events sent to `nfc_worker_q_hdl`.

**Handled stop events (trigger transition to `NFC_STATE_DONE`):**
- `Q_EVENT_NFC_READ_COMPLETE` — read operation finished
- `Q_EVENT_NFC_EMULATE_STOP` — emulation cancelled by user
- `Q_EVENT_NFC_STOP` — generic stop (cancel any operation in progress)

**The `NFC_STATE_DONE` handler calls `nfc_deinit_func()` and deasserts
`EN_EXT_5V`, so it is critical that every stop path reaches it.**

**Rules for new code:**
- If you add a new NFC stop event type, add a handler for it in the
  `NFC_STATE_PROCESS` case of `nfc_worker_task`.
- Always send a stop event to the worker before exiting an NFC blocking
  delegate — never rely on `menu_nfc_deinit()`'s `vTaskDelete()` to clean
  up, as that skips `nfc_deinit_func()`.

### IR Hardware State Management — `infrared_encode_sys_init()` / `_deinit()`

> **Every call to `infrared_encode_sys_init()` MUST be followed by
> `infrared_encode_sys_deinit()` — even if the TX complete event is not
> received.**  Skipping deinit on timeout leaves timer/DMA resources
> allocated, and the next `init()` call may fail or behave unpredictably.

**Background.** `transmit_command()` and `transmit_raw_command()` call
`infrared_encode_sys_init()` internally but do NOT call deinit — the
caller is responsible for deiniting after the `Q_EVENT_IRRED_TX` event.

**The pattern for IR TX loops (Send All, single-shot, raw):**
```c
transmit_command(&cmd);
/* Wait specifically for Q_EVENT_IRRED_TX, ignoring unrelated events.
 * BACK cancels.  Always deinit regardless of outcome. */
S_M1_Main_Q_t tx_q;
uint32_t deadline = HAL_GetTick() + 3000;
bool tx_done = false;
while (!tx_done) {
    uint32_t remaining = (HAL_GetTick() < deadline) ? (deadline - HAL_GetTick()) : 0;
    if (remaining == 0) break;
    if (xQueueReceive(main_q_hdl, &tx_q, pdMS_TO_TICKS(remaining)) != pdTRUE) break;
    if (tx_q.q_evt_type == Q_EVENT_IRRED_TX)
        tx_done = true;
    else if (tx_q.q_evt_type == Q_EVENT_KEYPAD)
        xQueueReceive(button_events_q_hdl, &bs, 0); /* drain to keep in sync */
}
infrared_encode_sys_deinit();  /* ← ALWAYS, even on timeout */
```

**Rules for new code:**
- Never skip `infrared_encode_sys_deinit()` — it must be called on every
  path after `transmit_command()` / `transmit_raw_command()`, regardless
  of whether `Q_EVENT_IRRED_TX` was received.
- When waiting for `Q_EVENT_IRRED_TX`, loop and ignore unrelated events
  (especially `Q_EVENT_KEYPAD`) — a single `xQueueReceive` may dequeue a
  keypad event instead.  Always drain `button_events_q_hdl` when consuming
  a keypad event to keep the two queues in sync.
- The event-loop single-shot TX handler already follows this pattern
  (deinit in the `Q_EVENT_IRRED_TX` handler).  The Send All loop must
  do the same for each iteration.

### Read Raw Scene State Management — TIM1 ISR / `start_passive_rx()` / `start_raw_rx()`

> **TIM1 input-capture MUST NOT be started during passive listen mode.**
> Starting it while `subghz_record_mode_flag == 0` causes the ISR to enqueue
> a `Q_EVENT_SUBGHZ_RX` for every noise edge (thousands per second at 433 MHz),
> flooding the main queue and preventing the 200 ms timeout that drives RSSI
> refreshes and the sine-wave animation.

**Background.** The Read Raw scene uses two radio states:

- **Passive listen** (Start / Idle) — SI4463 in RX for live RSSI; TIM1 ISR is
  NOT started.  The 200 ms queue timeout fires reliably and drives animations /
  RSSI updates.
- **Recording** — TIM1 ISR armed so captured edges flow into the ring buffer.

**The canonical timer lifecycle:**
```
scene_on_enter
  └─ start_passive_rx()          ← sub_ghz_rx_init_ext() only, NO rx_start
OK pressed (Start → Recording)
  └─ start_raw_rx()              ← sub_ghz_rx_start_ext() THEN record_mode_flag = 1
OK / BACK pressed (Recording → Idle)
  └─ stop_raw_rx()               ← sub_ghz_rx_pause_ext() + flush; NO rx_start at end
scene_on_exit
  └─ (recording) stop_raw_rx()   ← same cleanup including LED blink-off
  └─ (always)   sub_ghz_rx_deinit_ext() + opmode ISOLATED
```

**Rules for new code in or around the Read Raw scene:**

1. **Never call `sub_ghz_rx_start_ext()` inside `start_passive_rx()`.**  The
   timer must only be started when recording actually begins.
2. **In `start_raw_rx()`, set `subghz_decenc_ctl.pulse_det_stat = PULSE_DET_ACTIVE`
   and call `sub_ghz_rx_start_ext()` *before* setting `subghz_record_mode_flag = 1`.**
   This ensures the ISR state machine is initialised before any edge is captured.
3. **`stop_raw_rx()` pauses the timer but must NOT restart it.**  Passive Idle
   mode does not need TIM1 running.
4. **Never call `sub_ghz_rx_pause_ext()` in `scene_on_exit()` for Start/Idle.**
   The timer was never started in those states; calling pause on a stopped timer
   can trigger `Error_Handler()` via `HAL_TIM_IC_Stop_IT()`.  Call
   `sub_ghz_rx_deinit_ext()` directly — it checks `timerhdl_subghz_rx.State`
   before de-initialising and is safe regardless of run state.
5. **Use `stop_raw_rx()` on ALL exit paths when recording is active** (including
   scene exit), not inline teardown code.  `stop_raw_rx()` turns off the LED
   fast-blink — inline code that omits `m1_led_fast_blink(... OFF)` leaves the
   RGB LED blinking after leaving the scene.
6. **After `sub_ghz_replay_flipper_file()`, always call `start_passive_rx(app)`**
   (not an inline restart block).  Replay can mutate `subghz_scan_config.band`
   for CUSTOM-band files; `start_passive_rx()` re-applies `app->freq_idx/mod_idx`
   via `subghz_apply_config_ext()` to restore the user-selected config.  An
   inline restart that reads `subghz_scan_config.band` directly may resume on
   the wrong band after replay.

---

## Saved Item Actions Pattern

> **This is the canonical UX standard for all M1 modules with saved files.**
> It takes precedence over any previously defined UX preferences when they conflict.
> The pattern aligns with Flipper Zero's `*_scene_saved_menu.c` architecture —
> specifically, the action-menu verb set and navigation flow are modelled on Flipper
> and Momentum firmware rather than Monstatek stock.  Monstatek UX conventions
> (display rendering, keypad mapping, UIView framework) still apply when not
> superseded by this pattern.

Every module that loads files from SD card **MUST** provide a standard set of
saved-item actions.  This ensures consistency across the device and makes Flipper
port alignment straightforward.

### Core Verbs (required for ALL modules with saved files)

| Verb | Description | Implementation |
|------|-------------|----------------|
| **Emulate / Send** | Transmit or replay the saved item | Module-specific TX function |
| **Info** | Display file metadata (protocol, frequency, key/data) | Read-only info screen; BACK dismisses |
| **Rename** | Rename file on SD via virtual keyboard | `m1_vkb_get_filename()` → `f_rename()` |
| **Delete** | Delete file with confirmation dialog | `m1_message_box_choice()` → `m1_fb_delete_file()` or `f_unlink()` |

### Optional Verbs (module-specific)

| Verb | Modules | Description |
|------|---------|-------------|
| **Save** | Read scenes only (live decode, not loaded files) | Save captured signal to SD |
| **Write** | NFC, RFID | Write data to a physical tag/card |
| **Edit** | NFC (Edit UID), RFID (Edit data) | Modify item fields in-place |
| **Unlock** | NFC only | MF Classic key recovery / unlock |
| **Send All** | IR only | Transmit all commands in an .ir file sequentially |
| **Card/Tag Actions** | NFC, RFID | Context-specific tool submenu for loaded item |

### Implementation Patterns

**Sub-GHz** (`m1_subghz_scene_saved.c`):
- Inline action menu with `in_action_menu` / `in_info_screen` state flags
- Actions: Emulate, Info, Rename, Delete
- Info loads `.sub` metadata via `flipper_subghz_load()`

**Infrared** (`m1_ir_universal.c`):
- `ir_file_action_menu()` called from `show_commands()` via LEFT button
- Actions: Send All, Info, Rename, Delete
- Returns `false` if file was renamed/deleted (caller exits to browse)

**NFC** (`m1_nfc.c`):
- `m1_nfc_more_options_file[]` array with view mode dispatch
- Actions: Emulate UID, Unlock, Edit UID, Card Actions, Info, Rename, Delete

**RFID** (`m1_rfid.c`):
- `m1_rfid_save_mode_options[]` array with view mode dispatch
- Actions: Emulate, Write, Edit, Rename, Delete, Info

### Rules for New Modules

1. **Always include all four core verbs** (Emulate/Send, Info, Rename, Delete)
2. **Never add "Back" as a menu item** — the hardware BACK button handles navigation
3. **Rename must preserve the file extension** — strip ext before VKB, re-append after
4. **Delete must confirm** — use `m1_message_box_choice()` with "OK / Cancel"
5. **Info screen is read-only** — dismiss on any button press (typically BACK)
6. **After Rename or Delete, return to the file browser** — the old path is invalid

---

## UI / Button Bar Rules

> **Note:** The [Saved Item Actions Pattern](#saved-item-actions-pattern) above is the
> highest-priority UX standard.  The rules below still apply but are subordinate —
> if a saved-item action menu needs a specific layout, the Saved Item Actions pattern
> wins over generic button bar guidelines.

### Button-to-Column Mapping (3-Column Bar)

The `subghz_button_bar_draw()` API provides three columns: LEFT, CENTER,
RIGHT.  **Each column MUST correspond to its physical button:**

| Column | Physical button | Typical labels |
|--------|----------------|----------------|
| LEFT | LEFT button | Config, Erase, Stop |
| CENTER | OK button | REC, Stop, Send |
| RIGHT | RIGHT button | Save |

**NEVER** put a DOWN action label in the CENTER column or an OK action in
the RIGHT column.  This creates a confusing mismatch between what the user
sees on screen and what happens when they press a button.

If an action is triggered by UP or DOWN (which have no dedicated column),
either omit it from the button bar or use the LEFT column with a ↓ icon as
a visual hint.

#### Known violations (migration backlog)

All known button-to-column mapping violations have been fixed.

### General Button Bar Rules

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
  use `m1_menu_item_h()` and `m1_menu_max_visible()` — do NOT manually recompute row
  heights.  See [User-Configurable Font Size](#user-configurable-font-size--m1_menu_style).

---

## Scene-Based Application Architecture

**The scene manager is the target architecture for ALL M1 application modules.**
Every module that has more than one screen or any non-trivial navigation must use
scene-based architecture.  The `SubGhzSceneHandlers` pattern
(`on_enter` / `on_event` / `on_exit` / `draw` callbacks with stack-based push/pop
navigation) keeps state management clean, testable, and consistent across the device.

### Migration status

**Migration is complete.**  All modules with submenus now use the scene-based
architecture.  Sub-GHz has its own dedicated scene manager (`m1_subghz_scene.h/c`,
17 scenes) with radio-specific event handling.  All other modules use the shared
generic scene framework (`m1_scene.h/c`) with blocking delegates wrapping legacy
functions.  BadUSB and Apps are single-function modules (no submenus) and do not
need scene managers.

| Module | Current | Target | Entry point |
|--------|---------|--------|-------------|
| **Sub-GHz** | ✅ Scene manager | Done | `sub_ghz_scene_entry()` → `m1_subghz_scene.c` |
| **125KHz RFID** | ✅ Scene manager | Done | `rfid_scene_entry()` → `m1_rfid_scene.c` |
| **NFC** | ✅ Scene manager | Done | `nfc_scene_entry()` → `m1_nfc_scene.c` |
| **Infrared** | ✅ Scene manager | Done | `infrared_scene_entry()` → `m1_infrared_scene.c` |
| **GPIO** | ✅ Scene manager | Done | `gpio_scene_entry()` → `m1_gpio_scene.c` |
| **WiFi** | ✅ Scene manager | Done | `wifi_scene_entry()` → `m1_wifi_scene.c` |
| **Bluetooth** | ✅ Scene manager | Done | `bt_scene_entry()` → `m1_bt_scene.c` |
| **BadUSB** | — Single function | N/A | `badusb_run()` direct call (no submenus) |
| **Games** | ✅ Scene manager | Done | `games_scene_entry()` → `m1_games_scene.c` |
| **Settings** | ✅ Scene manager | Done | `settings_scene_entry()` → `m1_settings_scene.c` |

### Agent instructions for scene migration

Scene migration is complete for all modules.  There are no more legacy-menu
modules to migrate.  When working on **any** module, all new screens and features
**MUST** be implemented as scenes, not as standalone event-loop functions.

### Legacy functions still in m1_sub_ghz.c

After the dead-code cleanup, the only legacy functions remaining in `m1_sub_ghz.c`
are **actively called** as blocking delegates from their scene wrappers:

| Function | Called by | Notes |
|----------|-----------|-------|
| `sub_ghz_frequency_reader()` | `m1_subghz_scene_freq_analyzer.c` | Blocking delegate |
| `sub_ghz_spectrum_analyzer()` | `m1_subghz_scene_spectrum_analyzer.c` | Blocking delegate |
| `sub_ghz_rssi_meter()` | `m1_subghz_scene_rssi_meter.c` | Blocking delegate |
| `sub_ghz_freq_scanner()` | `m1_subghz_scene_freq_scanner.c` | Blocking delegate |
| `sub_ghz_weather_station()` | `m1_subghz_scene_weather_station.c` | Blocking delegate |
| `sub_ghz_brute_force()` | `m1_subghz_scene_brute_force.c` | Blocking delegate |
| `sub_ghz_add_manually()` | `m1_subghz_scene_add_manually.c` | Blocking delegate |
| `sub_ghz_replay_flipper_file()` | `m1_subghz_scene_saved.c` | Direct call for TX |

These are **not dead code** — they are the implementation behind scene wrappers.
Converting them to scene-native implementations is optional and can be done
incrementally.  Do **not** delete them.

### Rules for all scene menus

- Scene menus use the scrollbar pattern (no "OK" button bar).
- Never add "Back" as a menu item or button bar label.
- When item count changes, verify `MENU_VISIBLE` and scrolling math.
- Every scene file must be added to `cmake/m1_01/CMakeLists.txt`.
- **All scrollable lists MUST use the font-aware helpers** (`m1_menu_item_h()`,
  `m1_menu_max_visible()`, `m1_menu_font()`) — see the
  [User-Configurable Font Size](#user-configurable-font-size--m1_menu_style) section.

---

## Rolling Code Protocol Replay Philosophy

> **The standing preference for this project is: if a rolling code protocol can be
> decrypted or replayed using publicly documented algorithms or known parameters,
> it MUST be implemented.**  Every agent session that adds, modifies, or reviews
> a rolling code protocol MUST research its replay feasibility before concluding
> that replay is impossible.  "Search high and low for sources."

### Replay Feasibility Registry (researched 2026-04-17)

The `SubGhzProtocolFlag_PwmKeyReplay` bit in `subghz_protocol_registry.h` is the
authoritative, flag-based gate for which Dynamic (rolling-code) protocols the standard
OOK PWM key encoder can replay.  **Never gate on protocol name strings — use the flag.**

| Protocol | Replay? | Reason | Flag set? |
|----------|---------|--------|-----------|
| **CAME Atomo** | ✅ Yes | OOK PWM, no crypto | ✅ |
| **CAME TWEE** | ✅ Yes | OOK PWM, no crypto | ✅ |
| **Nice FloR-S** | ✅ Yes | OOK PWM, no crypto | ✅ |
| **Alutech AT-4N** | ✅ Yes | OOK PWM, no crypto | ✅ |
| **KingGates Stylo4k** | ✅ Yes | OOK PWM, no crypto | ✅ |
| **Scher-Khan Magicar** | ✅ Yes | OOK PWM, no crypto | ✅ |
| **Scher-Khan Logicar** | ✅ Yes | OOK PWM, no crypto | ✅ |
| **Toyota** | ✅ Yes | OOK PWM, no crypto | ✅ |
| **DITEC_GOL4** | ✅ Yes | OOK PWM, no crypto | ✅ |
| **Security+ 1.0** | ✅ Special | Ternary; brute-force counter mode in `m1_sub_ghz.c` | ❌ (custom path) |
| **KeeLoq** | ⚠️ Possible | OOK PWM; KeeLoq cipher with public algorithm (see note¹) | ❌ |
| **Jarolift** | ⚠️ Possible | KeeLoq-based OOK PWM; brand-specific MK partially researched (see note¹) | ❌ |
| **Star Line** | ⚠️ Possible | KeeLoq-based OOK PWM; MK partially reverse-engineered (see note¹) | ❌ |
| **FAAC SLH** | ❌ No | Manchester-encoded; algorithm not fully public (2026-04-17) | ❌ |
| **Somfy Telis** | ❌ No | Manchester + proprietary XOR; no feasible OTA replay method | ❌ |
| **Somfy Keytis** | ❌ No | Same RTS protocol as Telis | ❌ |
| **Revers_RB2** | ❌ No | Manchester-encoded; no public decryption method | ❌ |
| **KIA Seed** | ❌ No | Special preamble/start-bit encoding; not standard OOK PWM | ❌ |
| **Security+ 2.0** | ❌ No | FSK + bidirectional challenge-response; replay not feasible | ❌ |
| **Hormann BiSecur** | ❌ No | Manchester + AES-128; no known feasible attack | ❌ |
| **Beninca ARC** | ❌ No | AES-128 rolling code; no known feasible attack | ❌ |

> **¹ KeeLoq note:** The KeeLoq cipher is fully documented (Microchip AN66903 / 3NLF-NLFSR).
> The attack path is: `device_key = keeloq_learn(serial, manufacturer_key)` then
> `counter = keeloq_decrypt(encrypted_hop, device_key)` → increment → re-encrypt → transmit.
> Some manufacturer keys have been extracted via power-analysis attacks and discussed in the
> security research community (DarkFlippers/unleashed-firmware, academic papers).
> **This is the highest-priority next feature.**  Research brand-specific manufacturer keys
> for KeeLoq, Jarolift, and Star Line and implement a MK-lookup + counter mode.

### Implementation Rules

1. **When adding a new Dynamic protocol**, research its replay feasibility immediately.
   Set `SubGhzProtocolFlag_PwmKeyReplay` only if the protocol uses plain OOK PWM with
   no cipher (just a counter increment).  If it uses a known reversible cipher with a
   public or partially-published key, add it to the ⚠️ Possible list and open a task.

2. **KeeLoq counter mode is the next high-value target.**  Implement:
   - A manufacturer-key lookup table (brand → 64-bit MK)
   - KeeLoq decrypt: `counter = keeloq_decrypt(encrypted_hop, device_key)` where
     `device_key = keeloq_learn(serial, manufacturer_key)`
   - Counter-increment + re-encrypt → retransmit as OOK PWM

3. **Security+ 1.0 brute-force counter mode** is already implemented in `m1_sub_ghz.c`.
   The dedicated ternary encoder lives in `Sub_Ghz/subghz_secplus_v1_encoder.c`.

4. **Never gate replay on name strings** — always use `SubGhzProtocolFlag_PwmKeyReplay`
   or a dedicated custom encoder path.  The fragile `strcasecmp(proto->name, ...)` guard
   has been replaced by the flag; do not reintroduce it.

5. **Manchester-encoded protocols** (FAAC SLH, Somfy Telis/Keytis, Revers_RB2) must
   NOT have `SubGhzProtocolFlag_PwmKeyReplay`.  A Manchester encoder would be needed
   before replay could be attempted, and the receiver algorithm must also be known.

6. **Research sources to check** when evaluating a new rolling code protocol:
   - Flipper Zero firmware and Unleashed/DarkFlippers forks (Sub-GHz protocol handlers)
   - argilo/secplus (Security+ 1.0/2.0)
   - Academic papers: IEEE, USENIX, Black Hat EU proceedings on RFID/RF security
   - Hackaday, RTL-SDR Blog, forum.flipper.net
   - GitHub topics: `keeloq`, `rolling-code`, `subghz`, `rfhacking`
   - Signal identification wiki: sigidwiki.com
   - OpenSesame, RFCrack, GNU Radio out-of-tree modules

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
| [sincere360/M1_SiN360](https://github.com/sincere360/M1_SiN360) | sincere360 | **Active** | `9fdf2ff9e2` | 2026-04-04 14:57 | 2026-04-08 04:19 | v0.9 lineage — LCD settings, IR remote, screen orientation. Hapax version scheme derived from SiN360. v0.9.0.5 NFC Amiibo/Switch HALT fix cherry-picked. |
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
