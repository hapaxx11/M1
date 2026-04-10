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

# Build
cmake --build build

# Post-build: inject CRC and Hapax metadata (required for SD card flashing)
python tools/append_crc32.py build/M1_Hapax_v0.9.0.1.bin \
    --output build/M1_Hapax_v0.9.0.1_wCRC.bin \
    --hapax-revision 1 --verbose
```

The `--hapax-revision` flag is **mandatory** — without it, the dual-boot bank screen will
not show the Hapax revision or build date. CI auto-increments the revision on each merge.

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
