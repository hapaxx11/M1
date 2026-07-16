---
name: esp32-coprocessor
description: ESP32-C6 coprocessor reference: AT vs binary-SPI firmware variants, SPI transport, capability probing, memory footprint, and how to build/flash the ESP32 firmware. Load for any WiFi, Bluetooth, BLE, 802.15.4, or ESP32 AT-command work. Deep firmware detail lives in documentation/esp32_firmware.md.
---

# ESP32-C6 Coprocessor

> Extracted from CLAUDE.md. See also documentation/esp32_firmware.md for the
> full AT-command and firmware reference.

## ESP32-C6 Coprocessor

### Firmware Source

> **Two distinct ESP32 firmware variants are supported.  Choose based on which features you need.**

#### AT-command firmware (legacy — bedge117 / neddy299 / dag)
- **Source repo**: [`bedge117/esp32-at-monstatek-m1`](https://github.com/bedge117/esp32-at-monstatek-m1) (C3 custom AT firmware)
- **Deauth fork**: [`neddy299/esp32-at-monstatek-m1`](https://github.com/neddy299/esp32-at-monstatek-m1) (adds `AT+DEAUTH` + `AT+STASCAN`)
- **dag fork**: [`dagnazty/esp32-at-monstatek-m1`](https://github.com/dagnazty/esp32-at-monstatek-m1) (additional AT command extensions)
- All are forks of Espressif's official `esp-at`, customised for M1's SPI transport
- Pre-built binaries available on the GitHub Releases pages of each repo
- Enables: Bad-BT / BLE HID, 802.15.4 (Zigbee/Thread)
- Does NOT support (current firmware): WiFi sniffing, BLE wardrive, deauth attacks, evil portal, network scanners, WiFi connect/NTP sync (binary SPI; AT implementations pending), BT device management (binary SPI)
- See [`documentation/esp32_firmware.md`](documentation/esp32_firmware.md) for the full reference

#### Binary SPI firmware (recommended — sincere360 / SiN360)
- **Source repo**: [`sincere360/M1_SiN360_ESP32`](https://github.com/sincere360/M1_SiN360_ESP32) (NimBLE binary SPI slave)
- Pre-built binaries on the Releases page (`factory_ESP32C6-SPI-XIAO.bin` + `.md5`)
- **Enables** (vs AT firmware): WiFi packet sniffers (All/Beacon/Probe/Deauth/EAPOL/SAE/Pwnagotchi), signal monitor, station scan, MAC tracker, wardrive, BLE wardrive (regular/continuous/Flock), BLE sniffers (Analyzer/Generic/Flipper/AirTag/Flock), BLE Spam (SourApple/SwiftPair/Samsung/Flipper/All/AirTag Spoof), BLE Detectors (Skimmers/Flock/Meta), WiFi attacks (Deauth/Beacon/Clone/Rickroll/Evil Portal/Probe Flood/Karma/Karma+Portal), network scanners (Ping/ARP/SSH/Telnet/Port scan), **Bad-BT/BLE HID** (v0.9.1.0+, via CMD_BLE_HID_START/STOP/STATUS/REPORT)
- **Disables** (vs AT firmware): WiFi connect/NTP sync (stub returns "not available"), 802.15.4 scan — AT-layer modules still compile and these features will be restored once the SiN360 ESP32 firmware gains equivalent support
- Uses a 64-byte binary SPI packet protocol (`m1_esp32_cmd.c/h`) — NOT AT text commands
- Handshake via HANDSHAKE pin (PD7) after each TX; CS on PB10 (ESP32_SPI3_NSS)
- Same SPI3 Mode 1 hardware as AT firmware — no hardware changes required to switch between variants

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

### Memory Footprint Estimates — `M1_ESP32_FALLBACK_*`

> **For firmware developers and debugging agents only** — these values are not
> transmitted over the SPI wire protocol and are not visible to users.

`bss_bytes` and `free_heap_bytes` are not part of the `CMD_GET_STATUS` response
payload.  The M1 derives them from compile-time constants in
`m1_csrc/m1_esp32_caps.h` (`M1_ESP32_FALLBACK_*`) for use in OOM diagnostics
and buffer-sizing decisions when investigating SubGhz Read Raw, BLE sniff, or
other memory-intensive features.

#### How the estimate is selected

After `m1_esp32_caps_init()` resolves the capability bitmap (either from a live
`CMD_GET_STATUS` parse or the compile-flag fallback), it selects the profile:
- **`M1_ESP32_CAP_WIFI_JOIN` present** → AT profile (bedge117/C3.12)
- **`M1_ESP32_CAP_WIFI_JOIN` absent**  → SiN360 profile

The selected constants are written to `s_bss_bytes` and `s_free_heap_bytes` and
returned by `m1_esp32_caps_bss_bytes()` / `m1_esp32_caps_free_heap()`.

#### Current estimates and their derivation

| Profile | Constant | Value | Source |
|---------|----------|-------|--------|
| SiN360 | `M1_ESP32_FALLBACK_BSS_SIN360` | 200 KB | sincere360/M1_SiN360_ESP32 v0.9.0.8, ESP-IDF 5.5.4 |
| SiN360 | `M1_ESP32_FALLBACK_HEAP_SIN360` | 160 KB | NimBLE with `MSYS_BUF_FROM_HEAP=y`; 10×1600 B static WiFi RX; `ap_records[64]` ≈ 14 KB |
| AT/C3 | `M1_ESP32_FALLBACK_BSS_AT` | 284 KB | bedge117/esp32-at-monstatek-m1 v2.0.2, ESP-AT v4.0.0.0 |
| AT/C3 | `M1_ESP32_FALLBACK_HEAP_AT` | 112 KB | Full AT infrastructure + SPI ring buffers + BLE HID + 802.15.4 |

#### How to update estimates when a new firmware release appears

1. **Fetch the firmware repository** (sincere360/M1_SiN360_ESP32 or
   bedge117/esp32-at-monstatek-m1) and check out the new release tag.

2. **Measure BSS from the map file**: after a build, open the `.map` file
   and sum all BSS-section contributions, or use the linker symbols:
   ```python
   # ESP-IDF: from the elf file
   $ xtensa-esp32c6-elf-size -A build/<name>.elf | grep -E '\.bss|\.noinit'
   ```
   Alternatively, subtract `(_ebss - _sbss)` from the IDA/Ghidra BSS view.

3. **Measure free heap from a running device**:
   - Flash the new firmware to the XIAO test bench.
   - After boot, send `AT+GMR\r\n` (for AT builds) or a known status command to
     let the firmware settle.
   - Read `esp_get_free_heap_size()` — for AT builds this is reported by
     `AT+SYSRAMINFO`; for SiN360 it appears in the boot log on the UART console.

4. **Update the four constants** in `m1_csrc/m1_esp32_caps.h` with the new
   measured values (round down to the nearest KB for conservatism):
   ```c
   #define M1_ESP32_FALLBACK_BSS_SIN360   (NNN * 1024u)
   #define M1_ESP32_FALLBACK_HEAP_SIN360  (NNN * 1024u)
   ```

5. **Update the invariant tests** in `tests/test_esp32_caps.c`:
   - `test_fallback_at_bss_exceeds_sin360` — must still pass (AT BSS > SiN360 BSS)
   - `test_fallback_sin360_heap_exceeds_at` — must still pass (SiN360 heap > AT heap)
   If a new firmware reverses either invariant, update the test and the comment.

6. **Update this table** and the table in `documentation/esp32_firmware.md`
   with the new values and their source (firmware version + analysis method).

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
