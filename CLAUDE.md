# CLAUDE.md — Standing Instructions for AI Assistants

## Project: Monstatek M1 Firmware — Hapax Fork
## Owner: hapaxx11

---

## How to Use These Instructions

This file is the **always-loaded core**: universal rules that apply to *every*
session.  Task-specific reference material has been moved into **modular skills**
under `.github/skills/`.  **Before starting work, consult the Skill Index below
and read the SKILL.md for any skill whose trigger matches your task.**  Skipping
the relevant skill is how rules get "overlooked" — the whole point of this
structure is that each skill is short and focused, so read it in full.

Deep prose references remain in [`documentation/`](documentation/) and are linked
from the relevant skills.

### Skill Index (routing table)

| If the task touches… | Read skill |
|----------------------|------------|
| Sub-GHz protocols, replay, `.sub`/`.sgh`, emulation, bind wizard, freq presets, the Sub-GHz menu | [`subghz-protocols`](.github/skills/subghz-protocols/SKILL.md) |
| Importing Flipper files (Sub-GHz / LF-RFID / NFC / IR) | [`flipper-import`](.github/skills/flipper-import/SKILL.md) |
| ESP32-C6, WiFi, Bluetooth, BLE, 802.15.4, AT commands, SPI transport | [`esp32-coprocessor`](.github/skills/esp32-coprocessor/SKILL.md) |
| Any scene, menu, button bar, font, saved-item UI, post-connection navigation | [`ui-scene-architecture`](.github/skills/ui-scene-architecture/SKILL.md) |
| Adding tests, extracting pure logic, refactors, the phase-checklist workflow | [`firmware-testing`](.github/skills/firmware-testing/SKILL.md) |
| Radio / ESP32 / NFC / IR / Read Raw / backlight lifecycle & async RTOS flows | [`hardware-state-mgmt`](.github/skills/hardware-state-mgmt/SKILL.md) |
| `malloc`/heap/FreeRTOS/ISR allocation, heap-redirect checklist | [`memory-heap`](.github/skills/memory-heap/SKILL.md) |
| Updating vendored libs (u8g2, FreeRTOS, FatFs, IRMP) | [`vendored-deps`](.github/skills/vendored-deps/SKILL.md) |
| Any change needing a changelog entry or documentation update | [`docs-changelog`](.github/skills/docs-changelog/SKILL.md) |
| Fork auditing, upstream comparison, cherry-pick decisions | [`forks-tracker`](.github/skills/forks-tracker/SKILL.md) |

> The rules in this core file always apply regardless of which skill is loaded.
> When a skill and this core file appear to conflict, the ABSOLUTE RULES below
> always win.

---

## Glossary

Terminology used when communicating with the agent about the M1 UI.

| Term | Definition |
|------|------------|
| **Main Menu** | The top-level navigation screen that lists the major modules — Sub-GHz, RFID, NFC, Infrared, WiFi, Bluetooth, GPIO, Games, Settings, etc. |
| **Splash Screen** | The boot animation / logo screen shown exactly once at startup before the device is fully initialized. |
| **Home Screen** | The screen displayed when the user presses BACK from the Main Menu. Visually similar to the Splash Screen but may show additional status indicators (battery, SD card, radio state, etc.) because the hardware is now initialized. |

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

> **More on testing, modularization, and the phase checklist:** read the
> [`firmware-testing`](.github/skills/firmware-testing/SKILL.md) skill.

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

## Core Architecture Invariants

These invariants apply codebase-wide. Deeper, subsystem-specific architecture
rules live in the skills (see the Skill Index).

- **S_M1_FW_CONFIG_t** struct is EXACTLY 20 bytes — NEVER modify it
- CRC extension data lives at fixed offsets AFTER the struct (offset 20+)
- All Flipper file parsers use stack allocation (avoid heap where possible)
- FreeRTOS headers must be included before stream_buffer.h / queue.h
- Flipper parser API: functions are named `flipper_*_load()` / `flipper_*_save()`, return `bool`

---

## Versioning Scheme (quick reference)

Full rationale: [`documentation/agent/versioning.md`](documentation/agent/versioning.md).

- **Display format**: `v{major}.{minor}.{build}.{rc}-Hapax.{hapax_revision}` — e.g. `v0.9.0.1-Hapax.1`.
- **File/tag format**: `M1_Hapax_v{major}.{minor}.{build}.{rc}` — no `-Hapax.X` suffix in filenames/tags.
- **`FW_VERSION_MINOR`** = `9` (Hapax generation, owned by this fork).  **`FW_VERSION_MAJOR`/`FW_VERSION_BUILD`** stay `0` until Monstatek ships a breaking change.
- **`FW_VERSION_RC`** maps 1:1 to **`M1_HAPAX_REVISION`** (`m1_fw_update_bl.h`); CI auto-increments both.
- **Manual bump**: edit only `FW_VERSION_RC` and `M1_HAPAX_REVISION` — `CMakeLists.txt` is never patched (project name is derived from the four `FW_VERSION_*` macros at configure time).
- **`Hapax` is the project codename**, not a version number.

For Flipper protocol import procedures, see the [`flipper-import`](.github/skills/flipper-import/SKILL.md) skill.
For hardware capability assessment, see [`documentation/hardware_schematics.md`](documentation/hardware_schematics.md) (*secondary* — source code and build config are primary truth).

---

## Remote Configuration

- `origin` = hapaxx11/M1 (this fork — push here when explicitly told)
- `bedge117` = bedge117/M1 (upstream C3 reference, DO NOT PUSH)
- `sincere360` = sincere360/M1_SiN360 (upstream SiN360 reference, DO NOT PUSH)
- `monstatek` = Monstatek/M1 (original upstream, DO NOT PUSH)
- "Stock" firmware means Monstatek, NOT hapaxx11

---

> **Upstream merge policy** (why we don't merge Monstatek) and the full public
> forks tracker live in the [`forks-tracker`](.github/skills/forks-tracker/SKILL.md) skill.
