# CLAUDE.md — Standing Instructions for AI Assistants

## Project: Monstatek M1 Firmware
## Owner: bedge117

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
- When pushing is approved, push ONLY to `myfork` (bedge117/M1), never to `monstatek` (upstream)

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
- **Post-build CRC + C3 metadata**: The CMake post-build step uses `srec_cat` which is NOT installed and will fail. This is expected — the .bin/.elf/.hex files are already generated before that step. After `cmake --build` completes, run the CRC/metadata injection script. The canonical command is in `do_build.ps1` — always use it as the reference. Currently:
  ```
  python tools/append_crc32.py build/M1_v0800_C3.4.bin --output build/M1_v0800_C3.4_wCRC.bin --c3-revision 4 --verbose
  ```
- **CRITICAL: `--c3-revision 4` is MANDATORY** — without it, the C3 metadata (revision number + build date) will NOT be injected into the binary, and the dual boot bank screen will show only `v0.8.0.0` with no `-C3.1` suffix or build date. This flag must ALWAYS be included. The binary name must also match the CMake project name (`M1_v0800_C3.4`).

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

### Remotes (Monstatek vs ChrisUFO)
- `monstatek` remote = Monstatek/M1 (original upstream, still at v0.8.0.0)
- `origin` remote = ChrisUFO/M1 (fork with additional features up to v0.8.11)
- "Stock" firmware means Monstatek, NOT ChrisUFO

---

## Versioning Scheme

- **Monstatek's 4-field version is LOCKED** — `FW_VERSION_MAJOR`, `FW_VERSION_MINOR`, `FW_VERSION_BUILD`, and `FW_VERSION_RC` in `m1_fw_update_bl.h` all belong to Monstatek. NEVER change them. Currently `0.8.0.0`.
- **`C3` is the project codename**, NOT a version number. It does NOT mean "version 3"
- **`M1_C3_REVISION`** in `m1_fw_update_bl.h` = the C3 fork revision (currently 4). This is OUR version, completely separate from Monstatek's fields.
- **Display format**: `v{major}.{minor}.{build}.{rc}-C3.{c3_revision}` — e.g. `v0.8.0.0-C3.1`. Monstatek's 4 digits are displayed verbatim, the C3 suffix is appended.
- **When Monstatek updates**: bump their 4 fields to match upstream, `M1_C3_REVISION` stays as-is. No collision.
- **CMake project name** in `CMakeLists.txt:24`: `M1_v0800_C3.4` — must match the C3 revision
- **When bumping C3 revision**: update BOTH `M1_C3_REVISION` in `m1_fw_update_bl.h` AND `CMAKE_PROJECT_NAME` in `CMakeLists.txt`
- **RPC protocol**: `c3_revision` is sent as a separate byte in `S_RPC_DeviceInfo`. qMonstatek conditionally appends the `-C3.X` suffix only when `c3_revision > 0`, so stock Monstatek firmware displays without it.

---

## Architecture Rules

- **S_M1_FW_CONFIG_t** struct is EXACTLY 20 bytes — NEVER modify it
- CRC extension data lives at fixed offsets AFTER the struct (offset 20+)
- All Flipper file parsers use stack allocation (no heap/malloc)
- FreeRTOS headers must be included before stream_buffer.h / queue.h
- Flipper parser API: functions are named `flipper_*_load()` / `flipper_*_save()`, return `bool`

---

## Remote Configuration

- `origin` = ChrisUFO/M1 (DO NOT PUSH)
- `myfork` = bedge117/M1 (push here ONLY when explicitly told)
- `monstatek` = Monstatek/M1 (upstream reference, DO NOT PUSH)
- Feature branch: `feature/enhanced-firmware`
