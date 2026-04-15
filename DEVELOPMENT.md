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

Hapax is the **only M1 firmware fork with a GitHub-first CI/CD pipeline**.  Other
forks rely on manual local builds and ad-hoc binary distribution.  Hapax treats
GitHub Actions as the authoritative build system:

- Every push/PR to `main` triggers a **firmware build check** (`ci.yml`) and
  **unit test run** (`tests.yml`).
- Every merge to `main` **auto-creates a GitHub Release** with firmware artifacts
  (`build-release.yml`) — no manual compilation or upload required.
- **Doxygen API documentation** is auto-deployed to GitHub Pages (`docs.yml`).
- **Static analysis** (cppcheck, MISRA-C) is available on-demand via
  `static-analysis.yml`.
- The **[Web Updater](https://hapaxx11.github.io/M1/)** is hosted on GitHub Pages
  and pulls firmware directly from GitHub Releases — users can flash their M1
  from a browser with zero software installation.
- The M1 device itself can **download firmware over WiFi** directly from GitHub
  Releases (Settings → FW Update → Download).
- Builds that only touch docs, databases, IDE configs, or CI workflow files are
  automatically skipped (see `paths-ignore` in `ci.yml`).

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
