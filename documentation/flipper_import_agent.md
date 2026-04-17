<!-- See COPYING.txt for license details. -->

# Flipper Zero Import Agent — Skill Documentation

This document describes how an AI coding agent (GitHub Copilot, Claude, or similar) should
import the latest protocol code from the Flipper Zero firmware repository into the M1 project.

---

## Overview

The M1 firmware shares file-format and protocol compatibility with
[Flipper Zero](https://github.com/flipperdevices/flipperzero-firmware).
Flipper continuously adds new Sub-GHz, LF-RFID, NFC, and IR protocols.
This skill keeps M1 in sync with those additions by:

1. Checking whether the Monstatek/M1 upstream firmware has already added the
   protocol — if so, pull the work from there instead of re-porting from Flipper.
2. Identifying new or updated Flipper protocols since the last M1 sync (only if
   not already present in Monstatek upstream).
3. Porting each protocol to M1's architecture.
4. Registering the protocol in the M1 dispatch tables.
5. Ensuring protocol names match Flipper's constants for `.sub` / `.rfid` / `.nfc` / `.ir` file
   interoperability.
6. Respecting license obligations (Flipper firmware is GPLv3).

---

## Pattern Adoption Policy

When porting protocols or implementing new features, agents must decide which coding
patterns and architectural conventions to follow.  Two primary references exist:

1. **Monstatek/M1** — the upstream stock firmware from which this fork descends.
2. **Flipper Zero / Flipper One and community forks** (Momentum, Unleashed, etc.) — the
   ecosystem whose file formats, protocol names, and decoder algorithms M1 tracks.

### Current weighting: Flipper-first (until Monstatek > 1.0.0.0)

As of this writing, the Monstatek stock firmware has not yet released a version greater
than **1.0.0.0**.  Until that milestone is reached, the following weighting applies:

| Decision area | Preferred source | Rationale |
|---------------|-----------------|-----------|
| Protocol decoder algorithm & timing | **Flipper / forks** | Flipper's protocol implementations are battle-tested across millions of devices and continuously refined by a large community. |
| Struct layout, field names, enums | **Flipper / forks** | Maximises `.sub` / `.rfid` / `.nfc` / `.ir` interoperability and simplifies future protocol merges. |
| Naming conventions (`SUBGHZ_PROTOCOL_*_NAME`, file names) | **Flipper / forks** | File-format compatibility is the top priority. |
| HAL / driver patterns (SPI, UART, GPIO, power) | **Flipper / forks** (consult `documentation/furi_hal_reference/`) | The Furi HAL is a proven abstraction for the same peripherals M1 uses. |
| Build system, CMake structure, FreeRTOS integration | **Monstatek/M1** | M1's build system is already established and functional. |
| Flash layout, boot bank, CRC metadata | **Monstatek/M1** | Hardware-specific; Monstatek's implementation is validated on real M1 boards. |
| UI framework, display rendering, keypad handling | **Monstatek/M1** (low-level) / **Flipper** (scene UX) | M1's UIView framework differs from Flipper's ViewPort model, so display rendering and keypad mapping remain Monstatek. However, **scene-level UX patterns** (saved-item action menus, verb sets, navigation flow) follow the Flipper `*_scene_saved_menu.c` pattern. See CLAUDE.md "Saved Item Actions Pattern". |
| Scrollable list / menu text size | **Hapax** | Hapax has a user-configurable text size setting (`m1_menu_style`). All scrollable lists **must** use `m1_menu_font()`, `m1_menu_item_h()`, `m1_menu_max_visible()`, and `M1_MENU_VIS()` instead of hardcoding fonts, row heights, or visible item counts. Neither Flipper nor upstream Monstatek have this feature — imported code must be adapted. See CLAUDE.md [User-Configurable Font Size](#user-configurable-font-size). |

When a Flipper pattern conflicts with a Monstatek pattern in the protocol/decoder/HAL
space, **adopt the Flipper pattern** and log the deviation in the
[Monstatek Pattern Deviation Log](#monstatek-pattern-deviation-log) below.

### When Monstatek exceeds 1.0.0.0

Once Monstatek ships a release with a version number **greater than 1.0.0.0**, re-evaluate
the weighting.  At that point Monstatek will have made substantial architectural progress
and its patterns should carry equal or greater weight.  An agent encountering this
condition should:

1. Check the latest Monstatek release tag.
2. If the version is > 1.0.0.0, treat Monstatek and Flipper patterns as **equally weighted**,
   preferring whichever produces cleaner integration with the existing M1 codebase.
3. Review the deviation log below and resolve any entries where Monstatek's newer patterns
   are now preferable.

---

## Repository Locations

### Monstatek/M1 (firmware origin — do NOT push)

This M1 fork descends from the official Monstatek firmware.
**Always check here first** before porting anything from Flipper.

| Remote | URL |
|--------|-----|
| `monstatek` | `https://github.com/Monstatek/M1` |

Relevant branches: `main` (or the latest release tag).

### Flipper Zero (upstream reference — do NOT push)

| Area | Flipper path |
|------|-------------|
| Sub-GHz protocols | `lib/subghz/protocols/` |
| Sub-GHz protocol names | `lib/subghz/protocols/<proto>.h` → `#define SUBGHZ_PROTOCOL_*_NAME` |
| LF-RFID protocols | `lib/lfrfid/protocols/` |
| IR library | `lib/infrared/encoder_decoder/` |
| NFC library | `lib/nfc/protocols/` |

Flipper repository: `https://github.com/flipperdevices/flipperzero-firmware`
Use the `dev` branch for the latest code.

### Momentum Firmware (community fork — preferred protocol source)

The [Momentum Firmware](https://github.com/Next-Flip/Momentum-Firmware) (formerly
Xtreme) is a community fork of Flipper Zero firmware that includes additional
Sub-GHz and LF-RFID protocols beyond the official firmware.  Same directory
layout as Flipper Zero.

| Area | Path (same as Flipper) |
|------|------------------------|
| Sub-GHz protocols | `lib/subghz/protocols/` |
| LF-RFID protocols | `lib/lfrfid/protocols/` |

Momentum repository: `https://github.com/Next-Flip/Momentum-Firmware`
Use the `dev` branch or latest release tag (`mntm-XXX`).

> **Prefer Momentum** when both have the protocol — Momentum includes bug fixes
> and additional protocols not yet in official Flipper firmware.

### Flipper One (fbtng / flipperone-mcu-firmware — do NOT push)

The **Flipper One** is a new Flipper product based on a dual-CPU architecture:
a main application processor (running Linux) and an RP2350 (Cortex-M33) MCU
coprocessor that runs FreeRTOS and provides Sub-GHz, NFC, RFID, IR, GPIO, and
USB services — the exact same peripherals M1 manages.

This makes the Flipper One MCU firmware the **single most relevant reference**
for a complete, production-quality Furi HAL implementation on a Cortex-M33 +
FreeRTOS architecture.

#### Key repositories

| Name | URL | Purpose |
|------|-----|---------|
| `flipperone-mcu-firmware` | `https://github.com/flipperdevices/flipperone-mcu-firmware` | RP2350 MCU firmware — **furi_hal lives here** |
| `fbtng` | `https://github.com/flipperdevices/fbtng` | SCons-based build system; defines HAL contract headers and target manifests |
| `fbtng-corelibs` | `https://github.com/flipperdevices/fbtng-corelibs` | Packages `furi` core, FatFS, `bit_lib`, `mlib`, and other libraries |

#### f100 target — the RP2350 HAL reference

The target we are interested in is `targets/f100/` inside `flipperone-mcu-firmware`.
It implements the Furi HAL for RP2350 (dual Cortex-M33 + RISC-V, same ISA as STM32H573).

Reference commit: `29ada14951a34902bafaaad2be00e7b05774a414`  
Direct path: `targets/f100/furi_hal/`

> **All 45 source files from this path have been imported into this repository** at  
> `documentation/furi_hal_reference/`. See the README in that directory for complete
> porting notes mapping each RP2350 HAL module to STM32H573 equivalents.

#### Sub-GHz / LF-RFID / NFC / IR in Flipper One

Protocol implementations for the Flipper One follow the same pattern as Flipper Zero but
live under a different path. When the Flipper One gains a new protocol, check here:

| Area | Path in `flipperone-mcu-firmware` |
|------|-----------------------------------|
| Sub-GHz protocols | `lib/subghz/protocols/` (same lib as Flipper Zero, shared via `fbtng-corelibs`) |
| LF-RFID protocols | `lib/lfrfid/protocols/` |
| IR encoder/decoder | `lib/infrared/encoder_decoder/` |
| NFC protocols | `lib/nfc/protocols/` |

These protocol libraries are **shared between Flipper Zero and Flipper One** via the
`fbtng-corelibs` package — protocol port names and `SUBGHZ_PROTOCOL_*_NAME` constants
are identical between the two platforms.

### M1 (this repository)

| Area | M1 path |
|------|---------|
| Sub-GHz decoder files | `Sub_Ghz/protocols/m1_<proto>_decode.c` and `.h` |
| Sub-GHz protocol registry | `Sub_Ghz/subghz_protocol_registry.c` — master registry array (names, timing, decode fn ptr) |
| Sub-GHz registry header | `Sub_Ghz/subghz_protocol_registry.h` — types, includes all block headers below |
| Sub-GHz block decoder | `Sub_Ghz/subghz_block_decoder.h` — `SubGhzBlockDecoder` state machine (mirrors `lib/subghz/blocks/decoder.h`) |
| Sub-GHz block generic | `Sub_Ghz/subghz_block_generic.{h,c}` — `SubGhzBlockGeneric` decoded output (mirrors `lib/subghz/blocks/generic.h`) |
| Sub-GHz blocks math | `Sub_Ghz/subghz_blocks_math.h` — `DURATION_DIFF`, `bit_read/set/clear`, CRC, parity, LFSR (mirrors `lib/subghz/blocks/math.h`) |
| Sub-GHz legacy dispatch | `Sub_Ghz/m1_sub_ghz_decenc.c` / `.h` — enum kept for index constants; legacy arrays populated from registry at init |
| Existing bit/CRC utilities | `m1_csrc/bit_util.{h,c}` — underlying CRC/parity/LFSR implementations (wrapped by `subghz_blocks_math.h`) |
| LF-RFID protocol files | `lfrfid/lfrfid_protocol_<proto>.c` and `.h` |
| LF-RFID registry | `lfrfid/lfrfid_protocol.c` — `lfrfid_protocols[]` table |
| LF-RFID bit library | `lfrfid/lfrfid_bit_lib.h` — bit manipulation for LF-RFID (same pattern as Sub-GHz blocks) |
| Flipper file parsers | `m1_csrc/flipper_*.c` and `.h` |
| Flipper integration API | `m1_csrc/m1_flipper_integration.c` / `.h` |
| Build list | `cmake/m1_01/CMakeLists.txt` — `target_sources(...)` section |

---

---

## Furi HAL Porting Reference

A complete set of Furi HAL source files from the Flipper One f100 target has been imported
at `documentation/furi_hal_reference/`.  These are **not compiled** in M1 — they are there
so an agent can read them for API signatures, business logic, and porting guidance.

### When to consult the Furi HAL reference

- Adding or replacing a low-level M1 driver (UART, SPI, I2C, GPIO interrupts, power)
  and you want the authoritative Furi-compatible API shape.
- Implementing `furi_hal_serial_control` (acquire/release arbiter) on STM32H573.
- Implementing tickless deep sleep (`vPortSuppressTicksAndSleep`).
- Understanding the `FuriHalI2cBus` / `FuriHalI2cBusHandle` multi-bus abstraction.
- Referencing GPIO pin numbers for the F100 board
  (`furi_hal_reference/furi_hal_resources_pins.c`).

### Key differences from M1's current architecture

| Furi HAL concept | M1 current equivalent |
|---|---|
| `furi_hal_gpio_init_ex()` | Direct STM32 `HAL_GPIO_Init()` calls scattered in drivers |
| `furi_hal_interrupt_set_isr()` | Direct `HAL_NVIC_EnableIRQ()` + STM32 IRQ handlers |
| `furi_hal_serial_control` arbiter thread | `m1_esp32_hal.c` ad-hoc locking |
| `furi_hal_power_insomnia_enter/exit()` | No equivalent; `__WFI()` called directly |
| `FuriHalI2cBus` with mutex + event callbacks | `HAL_I2C_*` called directly |
| `furi_hal_nvm` key-value storage | `m1_fw_config` direct struct in flash |

The porting notes in `documentation/furi_hal_reference/README.md` map every RP2350-specific
API call to its STM32H573 equivalent.

---

---

## Copied Flipper Code Inventory

This section documents every file in this repository that was **directly copied or closely
derived** from Flipper Zero or Flipper One firmware source.  It exists so that:

- The total volume of copied code is visible at a glance.
- Agents can assess whether the submodule threshold has been reached (see below).
- License obligations can be audited without reading every file header.

> **Agent instruction:** Whenever you copy, port, or directly derive a new file from Flipper
> firmware source, add it to the appropriate table below **in the same commit**.  If you port
> a protocol decoder that is independently re-written (same algorithm, new code), note it as
> "independent port" in the Notes column.  Keep the running totals accurate.

---

### Submodule / Mirroring Threshold

We intentionally do **not** use git submodules or sparse-checkout mirrors for the Flipper
repos.  The reasons:

1. We are **porting** (transforming) Flipper code to M1's architecture, not vendoring it.
   Submodules are appropriate for build-time dependencies, not for read-time references.
2. AI agents working in sandboxed environments can use the GitHub MCP tool
   (`get_file_contents`) to fetch any Flipper source on demand without a submodule.
3. A full `flipperzero-firmware` clone is ~1 GB; even a sparse checkout of just the
   protocol dirs adds unnecessary repository weight.

**Reconsider submodules if any of the following become true:**

- The number of directly-copied (not independently-ported) Flipper source files in
  `lfrfid/` or `Sub_Ghz/protocols/` exceeds **80 files** — at that point keeping up with
  upstream bug-fixes by hand becomes impractical.
- A new protocol area requires copying Flipper **header-only libraries** (e.g. `bit_lib`,
  `mlib`) verbatim as build dependencies (not just reference docs).
- Upstream Flipper makes a breaking API change that affects more than **10 already-copied
  files** simultaneously — a sparse subtree pull would be safer than manual patching.

Current count of directly-copied files: **~58** (lfrfid area + 1 Sub-GHz) — well below the threshold.

---

### Category 1 — LF-RFID Protocol Implementations

Source: `lib/lfrfid/protocols/` in `flipperzero-firmware` (GPLv3)

These files were directly ported from Flipper Zero and adapted to M1's hardware abstraction
layer.  Each `.c` file carries a GPLv3 attribution header referencing the original project.
Framework files (`lfrfid.c`, `lfrfid_hal.c`, `lfrfid_file.c`, `lfrfid_manchester.c`) are
M1-specific wrappers/adapters derived from the Flipper HAL contract.

| M1 file | Flipper origin | Notes |
|---------|---------------|-------|
| `lfrfid/lfrfid_protocol.c` | `lib/lfrfid/lfrfid_protocol.c` | Protocol registry — adapted |
| `lfrfid/lfrfid_protocol.h` | `lib/lfrfid/lfrfid_protocol.h` | |
| `lfrfid/lfrfid.c` | `lib/lfrfid/lfrfid.c` | M1 HAL adapter |
| `lfrfid/lfrfid.h` | `lib/lfrfid/lfrfid.h` | |
| `lfrfid/lfrfid_hal.c` | — | M1-specific HAL glue |
| `lfrfid/lfrfid_hal.h` | — | |
| `lfrfid/lfrfid_file.c` | — | SD card file I/O |
| `lfrfid/lfrfid_file.h` | — | |
| `lfrfid/t5577.c` | `lib/lfrfid/t5577.c` | Derived — HAL replaced |
| `lfrfid/t5577.h` | `lib/lfrfid/t5577.h` | |
| `lfrfid/lfrfid_bit_lib.h` | `lib/toolbox/bit_lib.h` | Header adapted |
| `lfrfid/lfrfid_manchester.c` | `lib/toolbox/manchester.c` | Adapted |
| `lfrfid/lfrfid_manchester.h` | `lib/toolbox/manchester.h` | Header adapted |
| `lfrfid/lfrfid_protocol_awid.c` | `lib/lfrfid/protocols/protocol_awid.c` | |
| `lfrfid/lfrfid_protocol_awid.h` | `lib/lfrfid/protocols/protocol_awid.h` | |
| `lfrfid/lfrfid_protocol_electra.c` | `lib/lfrfid/protocols/protocol_electra.c` | |
| `lfrfid/lfrfid_protocol_electra.h` | `lib/lfrfid/protocols/protocol_electra.h` | |
| `lfrfid/lfrfid_protocol_em4100.c` | `lib/lfrfid/protocols/protocol_em4100.c` | |
| `lfrfid/lfrfid_protocol_em4100.h` | `lib/lfrfid/protocols/protocol_em4100.h` | |
| `lfrfid/lfrfid_protocol_fdx_a.c` | `lib/lfrfid/protocols/protocol_fdx_a.c` | |
| `lfrfid/lfrfid_protocol_fdx_a.h` | `lib/lfrfid/protocols/protocol_fdx_a.h` | |
| `lfrfid/lfrfid_protocol_fdx_b.c` | `lib/lfrfid/protocols/protocol_fdx_b.c` | |
| `lfrfid/lfrfid_protocol_fdx_b.h` | `lib/lfrfid/protocols/protocol_fdx_b.h` | |
| `lfrfid/lfrfid_protocol_gallagher.c` | `lib/lfrfid/protocols/protocol_gallagher.c` | |
| `lfrfid/lfrfid_protocol_gallagher.h` | `lib/lfrfid/protocols/protocol_gallagher.h` | |
| `lfrfid/lfrfid_protocol_gproxii.c` | `lib/lfrfid/protocols/protocol_gproxii.c` | |
| `lfrfid/lfrfid_protocol_gproxii.h` | `lib/lfrfid/protocols/protocol_gproxii.h` | |
| `lfrfid/lfrfid_protocol_h10301.c` | `lib/lfrfid/protocols/protocol_h10301.c` | |
| `lfrfid/lfrfid_protocol_h10301.h` | `lib/lfrfid/protocols/protocol_h10301.h` | |
| `lfrfid/lfrfid_protocol_hid_ex.c` | `lib/lfrfid/protocols/protocol_hid_ex.c` | |
| `lfrfid/lfrfid_protocol_hid_ex.h` | `lib/lfrfid/protocols/protocol_hid_ex.h` | |
| `lfrfid/lfrfid_protocol_hid_generic.c` | `lib/lfrfid/protocols/protocol_hid_generic.c` | |
| `lfrfid/lfrfid_protocol_hid_generic.h` | `lib/lfrfid/protocols/protocol_hid_generic.h` | |
| `lfrfid/lfrfid_protocol_idteck.c` | `lib/lfrfid/protocols/protocol_idteck.c` | |
| `lfrfid/lfrfid_protocol_idteck.h` | `lib/lfrfid/protocols/protocol_idteck.h` | |
| `lfrfid/lfrfid_protocol_indala26.c` | `lib/lfrfid/protocols/protocol_indala26.c` | |
| `lfrfid/lfrfid_protocol_indala26.h` | `lib/lfrfid/protocols/protocol_indala26.h` | |
| `lfrfid/lfrfid_protocol_ioprox.c` | `lib/lfrfid/protocols/protocol_ioprox.c` | |
| `lfrfid/lfrfid_protocol_ioprox.h` | `lib/lfrfid/protocols/protocol_ioprox.h` | |
| `lfrfid/lfrfid_protocol_jablotron.c` | `lib/lfrfid/protocols/protocol_jablotron.c` | |
| `lfrfid/lfrfid_protocol_jablotron.h` | `lib/lfrfid/protocols/protocol_jablotron.h` | |
| `lfrfid/lfrfid_protocol_keri.c` | `lib/lfrfid/protocols/protocol_keri.c` | |
| `lfrfid/lfrfid_protocol_keri.h` | `lib/lfrfid/protocols/protocol_keri.h` | |
| `lfrfid/lfrfid_protocol_nexwatch.c` | `lib/lfrfid/protocols/protocol_nexwatch.c` | |
| `lfrfid/lfrfid_protocol_nexwatch.h` | `lib/lfrfid/protocols/protocol_nexwatch.h` | |
| `lfrfid/lfrfid_protocol_noralsy.c` | `lib/lfrfid/protocols/protocol_noralsy.c` | |
| `lfrfid/lfrfid_protocol_noralsy.h` | `lib/lfrfid/protocols/protocol_noralsy.h` | |
| `lfrfid/lfrfid_protocol_pac_stanley.c` | `lib/lfrfid/protocols/protocol_pac_stanley.c` | |
| `lfrfid/lfrfid_protocol_pac_stanley.h` | `lib/lfrfid/protocols/protocol_pac_stanley.h` | |
| `lfrfid/lfrfid_protocol_paradox.c` | `lib/lfrfid/protocols/protocol_paradox.c` | |
| `lfrfid/lfrfid_protocol_paradox.h` | `lib/lfrfid/protocols/protocol_paradox.h` | |
| `lfrfid/lfrfid_protocol_pyramid.c` | `lib/lfrfid/protocols/protocol_pyramid.c` | |
| `lfrfid/lfrfid_protocol_pyramid.h` | `lib/lfrfid/protocols/protocol_pyramid.h` | |
| `lfrfid/lfrfid_protocol_securakey.c` | `lib/lfrfid/protocols/protocol_securakey.c` | |
| `lfrfid/lfrfid_protocol_securakey.h` | `lib/lfrfid/protocols/protocol_securakey.h` | |
| `lfrfid/lfrfid_protocol_viking.c` | `lib/lfrfid/protocols/protocol_viking.c` | |
| `lfrfid/lfrfid_protocol_viking.h` | `lib/lfrfid/protocols/protocol_viking.h` | |

**Subtotal:** 57 files

---

### Category 2 — Sub-GHz Protocol Decoders

Source: `lib/subghz/protocols/` in `flipperzero-firmware` (GPLv3)

The 96 decoder files in `Sub_Ghz/protocols/` were **independently re-implemented** based on
the same RF timing specifications as Flipper (so `.sub` files remain compatible), but the M1
code is not a copy of Flipper source.  The sole exception is noted below.  The total of 96
comprises 95 named protocol decoders plus one utility file (`m1_bin_raw_decode.c`).

| M1 file | Flipper origin | Notes |
|---------|---------------|-------|
| `Sub_Ghz/protocols/m1_chamberlain_decode.c` | `lib/subghz/protocols/secplus_v1.c` | Contains direct port of argilo/secplus ternary algorithm; GPLv3 attribution present |
| All other `Sub_Ghz/protocols/m1_*_decode.c` (90 files) | Various | **Independent ports** — same algorithm, original M1 implementation; no Flipper source copied |
| `Sub_Ghz/protocols/m1_acurite_592txr_decode.c` | `lib/subghz/protocols/acurite_592txr.c` | **Independent port** — 56-bit PWM, sum checksum + parity |
| `Sub_Ghz/protocols/m1_acurite_986_decode.c` | `lib/subghz/protocols/acurite_986.c` | **Independent port** — 40-bit PWM, CRC-8 poly 0x07, LSB bit reversal |
| `Sub_Ghz/protocols/m1_tx_8300_decode.c` | `lib/subghz/protocols/tx_8300.c` | **Independent port** — 72-bit OOK, Fletcher-8 checksum, 128-bit decoder |
| `Sub_Ghz/protocols/m1_oregon_v1_decode.c` | `lib/subghz/protocols/oregon_v1.c` | **Independent port** — Manchester, 32-bit, byte-sum checksum |
| `Sub_Ghz/protocols/m1_oregon3_decode.c` | `lib/subghz/protocols/oregon3.c` | **Independent port** — Manchester inverted, 32-bit, nibble checksum |

**Subtotal:** 1 directly-copied file; 94 independent ports (+ 1 bin_raw utility file)

---

### Category 3 — Furi HAL Reference (documentation only)

Source: `targets/f100/furi_hal/` in `flipperone-mcu-firmware` (GPLv3)
Commit: `29ada14951a34902bafaaad2be00e7b05774a414`

These 45 files live in `documentation/furi_hal_reference/` and are **not compiled** in M1.
They are a read-only porting reference for implementing M1's driver layer.  See
`documentation/furi_hal_reference/README.md` for the full RP2350 → STM32H573 mapping.

**Subtotal:** 45 files (documentation only, not shipped in firmware)

---

### Category 4 — Flipper File Format Parsers (m1_csrc/)

These files were **independently written** by the M1 team to parse Flipper's file formats.
They are not copied from Flipper source; they implement the format by reading Flipper's
documentation and `.sub`/`.rfid`/`.nfc`/`.ir` file samples.

| M1 file | Purpose |
|---------|---------|
| `m1_csrc/flipper_file.c` / `.h` | Generic Flipper key-value file parser |
| `m1_csrc/flipper_ir.c` / `.h` | `.ir` file parser — maps Flipper IR protocol names to IRMP IDs |
| `m1_csrc/flipper_nfc.c` / `.h` | `.nfc` file parser |
| `m1_csrc/flipper_rfid.c` / `.h` | `.rfid` file parser |
| `m1_csrc/flipper_subghz.c` / `.h` | `.sub` file parser |

**Subtotal:** 10 files (independent — no Flipper attribution required)

---

### Grand Total

| Category | Files | Directly copied | Independent ports/writes |
|----------|-------|-----------------|--------------------------|
| LF-RFID protocols | 57 | 57 | 0 |
| Sub-GHz decoders | 96 | 1 | 94 (+ 1 bin_raw) |
| Furi HAL reference (docs) | 45 | 45 | — |
| Flipper file parsers | 10 | 0 | 10 |
| **Total** | **208** | **58 compiled + 45 docs** | **104** |

The **directly-copied compiled files** total ~58, comfortably below the 80-file submodule
threshold.  Continue monitoring this count as new LF-RFID protocols are ported.

---

## Flipper-Compatible Building Blocks

M1 provides a set of headers that mirror Flipper's `lib/subghz/blocks/` directory.
These make porting a Flipper or Momentum decoder largely mechanical — ported code
can use the same struct names, field names, and function names as the original.

### Block Decoder (`subghz_block_decoder.h`)

Maps to Flipper's `lib/subghz/blocks/decoder.h`.

```c
#include "subghz_block_decoder.h"

// Identical to Flipper's struct — ported code compiles unchanged
SubGhzBlockDecoder decoder = {0};

// State machine steps (define your own enum per protocol)
decoder.parser_step = MyProtoStepReset;

// Save last pulse duration for two-pulse analysis
decoder.te_last = duration;

// Accumulate bits — exact same function name and semantics as Flipper
subghz_protocol_blocks_add_bit(&decoder, bit_value);

// Check completion
if (decoder.decode_count_bit >= MIN_COUNT_BIT) {
    uint64_t result = decoder.decode_data;
    // success!
}

// For protocols >64 bits (e.g., Oregon V2, Bresser):
uint64_t upper_bits = 0;
subghz_protocol_blocks_add_to_128_bit(&decoder, bit_value, &upper_bits);
```

### Block Generic (`subghz_block_generic.h`)

Maps to Flipper's `lib/subghz/blocks/generic.h`.

```c
#include "subghz_block_generic.h"

// Decoded output — same field names as Flipper
SubGhzBlockGeneric generic = {0};
generic.data           = decoder.decode_data;
generic.data_count_bit = decoder.decode_count_bit;
generic.serial         = (uint32_t)(generic.data >> 8);
generic.btn            = (uint8_t)(generic.data & 0xF);
generic.cnt            = rolling_code;

// Bridge to M1's global state (call at end of successful decode)
subghz_block_generic_commit_to_m1(&generic, PROTOCOL_INDEX);
```

### Blocks Math (`subghz_blocks_math.h`)

Maps to Flipper's `lib/subghz/blocks/math.h`.

```c
#include "subghz_blocks_math.h"

// Timing comparison — branchless absolute difference
if (DURATION_DIFF(duration, te_short) < te_delta) { ... }

// Bit manipulation macros
bit_read(value, 3);       // read bit 3
bit_set(value, 5);        // set bit 5
bit_clear(value, 5);      // clear bit 5

// Key reversal
uint64_t reversed = subghz_protocol_blocks_reverse_key(key, 24);

// Parity
uint8_t par = subghz_protocol_blocks_get_parity(key, 24);

// CRC (delegates to M1's bit_util.h implementations)
uint8_t crc = subghz_protocol_blocks_crc8(msg, len, 0x31, 0x00);
```

### Manchester Decoder (`subghz_manchester_decoder.h`)

Maps to Flipper's `lib/toolbox/manchester_decoder.{h,c}`.

Table-driven Manchester decoder state machine used by protocols like Somfy Telis,
Somfy Keytis, Marantec, FAAC SLH, and Revers RB2.  Feed it (short/long) ×
(high/low) events and it produces decoded bits.

```c
#include "subghz_manchester_decoder.h"

ManchesterState manchester_state = ManchesterStateMid1;
bool bit_value;

// Classify each pulse pair into a Manchester event:
ManchesterEvent event;
if (is_short && is_high) event = ManchesterEventShortHigh;
else if (is_short && !is_high) event = ManchesterEventShortLow;
else if (!is_short && is_high) event = ManchesterEventLongHigh;
else event = ManchesterEventLongLow;

// Advance the state machine:
if (manchester_advance(manchester_state, event, &manchester_state, &bit_value)) {
    // bit_value is valid — accumulate it
    subghz_protocol_blocks_add_bit(&decoder, bit_value);
}
```

### Manchester Encoder (`subghz_manchester_encoder.h`)

Maps to Flipper's `lib/toolbox/manchester_encoder.{h,c}`.

Converts data bits into Manchester-coded symbols for TX waveform generation.

```c
#include "subghz_manchester_encoder.h"

ManchesterEncoderState enc_state;
manchester_encoder_reset(&enc_state);

// Encode each data bit:
for (int i = 0; i < bit_count; i++) {
    ManchesterEncoderResult result;
    bool consumed = false;
    while (!consumed) {
        consumed = manchester_encoder_advance(&enc_state, data_bits[i], &result);
        // result is ShortLow/ShortHigh/LongLow/LongHigh
        // → map to LevelDuration and append to upload buffer
    }
}

// Emit trailing symbol:
ManchesterEncoderResult final = manchester_encoder_finish(&enc_state);
```

### Block Encoder (`subghz_block_encoder.h`)

Maps to Flipper's `lib/subghz/blocks/encoder.{h,c}`.

Provides `SubGhzProtocolBlockEncoder` for TX state tracking, bit-array
helpers, and an upload generator that converts bit arrays to `LevelDuration[]`.

```c
#include "subghz_block_encoder.h"

// Build a bit array (MSB-first, matching Flipper layout):
uint8_t data[4] = {0};
subghz_protocol_blocks_set_bit_array(true,  data, 0, sizeof(data));  // bit 0 = 1
subghz_protocol_blocks_set_bit_array(false, data, 1, sizeof(data));  // bit 1 = 0

// Convert to LevelDuration waveform:
LevelDuration upload[128];
size_t upload_size = subghz_protocol_blocks_get_upload_from_bit_array(
    data, 24, upload, 128, 500,  // 500μs per bit
    SubGhzProtocolBlockAlignBitLeft);

// Track TX playback:
SubGhzProtocolBlockEncoder encoder = {
    .upload      = upload,
    .size_upload = upload_size,
    .repeat      = 4,
    .is_running  = true,
};
```

### LevelDuration (`subghz_level_duration.h`)

Maps to Flipper's `lib/toolbox/level_duration.h`.

A compact (level, duration_μs) pair used by encoders and the TX path.

```c
#include "subghz_level_duration.h"

LevelDuration ld = level_duration_make(true, 500);   // HIGH for 500μs
bool is_high = level_duration_get_level(ld);          // true
uint32_t dur = level_duration_get_duration(ld);       // 500

LevelDuration rst = level_duration_reset();           // sentinel
bool is_rst = level_duration_is_reset(rst);           // true
```

### Mapping: Flipper → M1

| Flipper file | M1 equivalent | Notes |
|-------------|---------------|-------|
| `lib/subghz/blocks/decoder.h` | `Sub_Ghz/subghz_block_decoder.h` | All-inline header |
| `lib/subghz/blocks/decoder.c` | (not needed — all inline) | |
| `lib/subghz/blocks/generic.h` | `Sub_Ghz/subghz_block_generic.h` | + M1 bridge function |
| `lib/subghz/blocks/generic.c` | `Sub_Ghz/subghz_block_generic.c` | Only the commit bridge |
| `lib/subghz/blocks/math.h` | `Sub_Ghz/subghz_blocks_math.h` | Wraps `bit_util.h` |
| `lib/subghz/blocks/math.c` | `m1_csrc/bit_util.c` | Already exists |
| `lib/subghz/blocks/encoder.h` | `Sub_Ghz/subghz_block_encoder.h` | All-inline header |
| `lib/subghz/blocks/encoder.c` | (not needed — all inline) | |
| `lib/toolbox/level_duration.h` | `Sub_Ghz/subghz_level_duration.h` | LevelDuration type |
| `lib/toolbox/manchester_decoder.h` | `Sub_Ghz/subghz_manchester_decoder.h` | All-inline header |
| `lib/toolbox/manchester_decoder.c` | (not needed — all inline) | |
| `lib/toolbox/manchester_encoder.h` | `Sub_Ghz/subghz_manchester_encoder.h` | All-inline header |
| `lib/toolbox/manchester_encoder.c` | (not needed — all inline) | |
| `lib/toolbox/bit_lib.h` | `lfrfid/lfrfid_bit_lib.h` | Exists for LF-RFID |

---

## Step-by-Step Process

### Step 0 — Check Monstatek upstream and assess pattern source

Before doing any Flipper porting work, check whether Monstatek has already
shipped the protocol in a newer version of the official firmware:

1. Fetch the Monstatek/M1 `main` branch (or the latest tag):
   ```
   git fetch monstatek main
   ```
2. Compare `Sub_Ghz/protocols/` and `lfrfid/` in `monstatek/main` against the
   current working tree:
   ```
   git diff HEAD monstatek/main -- Sub_Ghz/protocols/ lfrfid/ m1_csrc/
   ```
3. If Monstatek already contains the protocol:
   - Review Monstatek's implementation **and** the corresponding Flipper source.
   - If the Monstatek implementation closely follows Flipper patterns, cherry-pick
     or merge the relevant commits from `monstatek/main`.
   - If the Monstatek implementation diverges from Flipper patterns (different struct
     layout, naming, algorithm structure), **prefer porting from Flipper directly**
     while Monstatek remains at version ≤ 1.0.0.0 (see
     [Pattern Adoption Policy](#pattern-adoption-policy)).  Log the divergence in
     the [Monstatek Pattern Deviation Log](#monstatek-pattern-deviation-log).
   - Adjust for any Hapax-specific additions (version fields, build-date injection,
     RPC extensions) before committing.
4. If Monstatek does **not** yet contain the protocol, continue with Step 1 and
   port from Flipper (or a community fork such as Momentum / Unleashed).

> **Why Flipper-first while Monstatek is pre-1.0:** Flipper's protocol
> implementations are maintained by a large community, continuously tested across
> millions of devices, and define the `.sub` / `.rfid` / `.nfc` / `.ir` file
> formats that M1 must stay compatible with.  Until Monstatek ships a release
> above 1.0.0.0, Flipper patterns carry more weight for protocol and decoder
> decisions.  Deviations from Monstatek patterns are still tracked so they can be
> reconciled once Monstatek matures.

### Step 1 — Identify new protocols

1. Fetch the Flipper `dev` branch.
2. List all files in `lib/subghz/protocols/` that have no counterpart in
   `Sub_Ghz/protocols/` (after stripping Flipper naming conventions and matching
   against `protocol_text[]` in `m1_sub_ghz_decenc.c`).
3. List all files in `lib/lfrfid/protocols/` that have no counterpart in
   `lfrfid/`.
4. Record the exact `#define SUBGHZ_PROTOCOL_*_NAME` string from each new
   Flipper header — this string **must** be used verbatim in M1's
   `protocol_text[]` array so that `.sub` files are cross-compatible.

### Step 2 — Port a Sub-GHz protocol decoder

#### 2a. Create the decoder source file

Create `Sub_Ghz/protocols/m1_<proto>_decode.c` following this template:

```c
/* See COPYING.txt for license details. */

/*
 *  m1_<proto>_decode.c
 *
 *  M1 sub-ghz <Protocol Name> decoding
 *
 *  Ported from Flipper Zero / Momentum firmware
 *  https://github.com/flipperdevices/flipperzero-firmware
 *  https://github.com/Next-Flip/Momentum-Firmware
 *  Original file: lib/subghz/protocols/<proto>.c
 *  Copyright (C) Flipper Devices Inc. — Licensed under GPLv3
 *
 * M1 Project
 */

#include <string.h>
#include <stdlib.h>
#include "stm32h5xx_hal.h"
#include "m1_sub_ghz_decenc.h"
#include "subghz_protocol_registry.h"   /* includes all block headers */
#include "m1_log_debug.h"

#define M1_LOGDB_TAG  "SUBGHZ_<PROTO>"

/* ---- Flipper-compatible decoder state (optional — for complex protocols) ---- */

/* Simple protocols can use direct bit-shifting on a local uint64_t.
 * Complex protocols should use SubGhzBlockDecoder for Flipper-compatible
 * state machine porting: */

/*
 * enum {
 *     <Proto>DecoderStepReset = 0,
 *     <Proto>DecoderStepSaveDuration,
 *     <Proto>DecoderStepCheckDuration,
 * };
 */

/* ---- decoder implementation ---- */
uint8_t subghz_decode_<proto>(uint16_t p, uint16_t pulsecount)
{
    const SubGhzProtocolDef *proto = &subghz_protocol_registry[p];
    const uint16_t te_short = proto->timing.te_short;
    const uint16_t te_long  = proto->timing.te_long;
    const uint16_t te_delta = proto->timing.te_delta
        ? proto->timing.te_delta
        : (te_short * proto->timing.te_tolerance_pct / 100);
    const uint16_t min_bits = proto->timing.min_count_bit_for_found;

    /* Option A: Simple protocol — direct bit accumulation */
    uint64_t code = 0;
    uint16_t bits_count = 0;

    for (uint16_t i = 0; i + 1 < pulsecount; i += 2) {
        uint16_t t_hi = subghz_decenc_ctl.pulse_times[i];
        uint16_t t_lo = subghz_decenc_ctl.pulse_times[i + 1];

        code <<= 1;
        if (DURATION_DIFF(t_hi, te_short) < te_delta &&
            DURATION_DIFF(t_lo, te_long) < te_delta) {
            /* bit 0 */
        } else if (DURATION_DIFF(t_hi, te_long) < te_delta &&
                   DURATION_DIFF(t_lo, te_short) < te_delta) {
            code |= 1;  /* bit 1 */
        } else {
            break;
        }
        bits_count++;
    }

    if (bits_count < min_bits)
        return 1;

    /* Option B: Bridge to M1 global state via SubGhzBlockGeneric */
    SubGhzBlockGeneric generic = {0};
    generic.data           = code;
    generic.data_count_bit = bits_count;
    generic.serial         = (uint32_t)(code >> 4);  /* protocol-specific extraction */
    generic.btn            = (uint8_t)(code & 0xF);
    subghz_block_generic_commit_to_m1(&generic, p);
    return 0;
}
```

No header file is required unless the decoder needs shared definitions.

#### 2b. Add the protocol to the registry

In `Sub_Ghz/subghz_protocol_registry.c`, append one entry to the
`subghz_protocol_registry[]` array:

```c
[NEW_PROTO_ENUM] = {
    .name   = "<Flipper SUBGHZ_PROTOCOL_*_NAME>",
    .type   = SubGhzProtocolTypeStatic,  /* or Dynamic/Weather/TPMS */
    .flags  = F_STATIC_433,
    .filter = SubGhzProtocolFilter_Auto,
    .timing = { .te_short=<val>, .te_long=<val>, .te_tolerance_pct=20,
                .min_count_bit_for_found=<val> },
    .decode = subghz_decode_<proto>,
},
```

And add the corresponding `extern` forward declaration at the top of the file.

#### 2c. Add enum entry in `m1_sub_ghz_decenc.h`

Add a new constant to the protocol enum. It must match the array index used
in the registry. Append at the end — never reorder existing entries.

#### 2d. No more switch-case registration needed

The old `subghz_decode_protocol()` switch-case has been replaced with a
table-driven dispatch from the registry. No manual case statement is required.

**That's it — 3 edits total** (decoder file + registry entry + enum constant)
plus adding the `.c` file to CMakeLists.txt.

### Step 3 — Port an LF-RFID protocol

1. Create `lfrfid/lfrfid_protocol_<proto>.c` and `.h`, following the pattern of
   `lfrfid/lfrfid_protocol_em4100.c`.
2. Add the GPLv3 header comment referencing the Flipper source file.
3. Register the protocol in the `lfrfid_protocols[]` table in
   `lfrfid/lfrfid_protocol.c`.

### Step 4 — Update the build system

Add each new source file to `cmake/m1_01/CMakeLists.txt` inside the `target_sources(M1 PRIVATE ...)` block:

```cmake
../../Sub_Ghz/protocols/m1_<proto>_decode.c
```

or for LF-RFID:

```cmake
../../lfrfid/lfrfid_protocol_<proto>.c
```

### Step 5 — Verify `.sub` / `.rfid` file interoperability

After adding the protocol:

- Confirm the string in `protocol_text[]` matches `SUBGHZ_PROTOCOL_*_NAME`
  from the Flipper header exactly (case-sensitive, spaces vs underscores matter).
- Check the existing name mappings in `m1_sub_ghz_decenc.c` (lines ~107-164)
  for precedent — some names differ from naive expectations (e.g.
  `"Holtek_HT12X"` not `"Holtek"`, `"GateTX"` not `"Gate TX"`).
- Consult `CLAUDE.md` → *Flipper compatibility* section for known name corrections.
- If the imported feature includes any file browser, loader, info screen, or
  playlist parser, verify it handles **both** `.sub` and `.sgh` files.  See
  *Dual `.sub` / `.sgh` File Format Support* in the File Interoperability
  Reference section below for the full rules.

---

## License Rules

The Flipper Zero firmware is **GPLv3**. Any file that directly adapts or ports
Flipper source code **must**:

1. Include the following comment near the top of the file:

   ```c
   /*
    * Ported from Flipper Zero firmware
    * https://github.com/flipperdevices/flipperzero-firmware
    * Original file: lib/subghz/protocols/<proto>.c  (or lfrfid/...)
    * Copyright (C) Flipper Devices Inc. — Licensed under GPLv3
    */
   ```

2. Be listed in `README_License.md` under the appropriate section.

3. Retain the `/* See COPYING.txt for license details. */` header.

If the M1 implementation is **independently written** (same algorithm, new code),
the Flipper attribution is optional but the protocol name must still match.

---

## File Interoperability Reference

### Sub-GHz `.sub` files

The `Protocol:` field in a `.sub` file must exactly match an entry in
`protocol_text[]` in `Sub_Ghz/m1_sub_ghz_decenc.c`.

Flipper name constants are defined as `SUBGHZ_PROTOCOL_*_NAME` macros in
`lib/subghz/protocols/<proto>.h` in the Flipper repository.

Known name corrections already applied in M1 (do not revert):

| M1 `protocol_text[]` | Flipper `SUBGHZ_PROTOCOL_*_NAME` |
|----------------------|----------------------------------|
| `"Holtek_HT12X"` | `SUBGHZ_PROTOCOL_HOLTEK_HT12X_NAME` |
| `"Faac SLH"` | `SUBGHZ_PROTOCOL_FAAC_SLH_NAME` |
| `"Hormann HSM"` | `SUBGHZ_PROTOCOL_HORMANN_HSM_NAME` |
| `"GateTX"` | `SUBGHZ_PROTOCOL_GATE_TX_NAME` |
| `"Cham_Code"` | `SUBGHZ_PROTOCOL_CHAMB_CODE_NAME` |
| `"iDo 117/111"` | `SUBGHZ_PROTOCOL_IDO_NAME` |
| `"Nice FloR-S"` | `SUBGHZ_PROTOCOL_NICE_FLOR_S_NAME` |
| `"KingGates Stylo4k"` | `SUBGHZ_PROTOCOL_KINGGATES_STYLO_4K_NAME` |
| `"MegaCode"` | `SUBGHZ_PROTOCOL_MEGACODE_NAME` |
| `"CAME TWEE"` | `SUBGHZ_PROTOCOL_CAME_TWEE_NAME` |
| `"Scher-Khan"` | `SUBGHZ_PROTOCOL_SCHER_KHAN_NAME` |

#### Sub-GHz protocols ported — March 2026

The following 17 protocols were added in this session. Each has a decoder file
in `Sub_Ghz/protocols/` and an entry in `subghz_protocol_registry[]` plus
the enum in `m1_sub_ghz_decenc.h`.

| Flipper source file | `SUBGHZ_PROTOCOL_*_NAME` | M1 decoder | Notes |
|---------------------|--------------------------|------------|-------|
| `dickert_mahs.c` | `"Dickert_MAHS"` | `m1_dickert_mahs_decode.c` | Custom inverted-pair decoder (LOW+HIGH, 36 bits) |
| `feron.c` | `"Feron"` | `m1_feron_decode.c` | Generic PWM (350/750, 32 bits) |
| `gangqi.c` | `"GangQi"` | `m1_gangqi_decode.c` | Generic PWM (500/1200, 34 bits) |
| `hay21.c` | `"Hay21"` | `m1_hay21_decode.c` | Generic PWM (300/700, 21 bits) |
| `hollarm.c` | `"Hollarm"` | `m1_hollarm_decode.c` | Custom PPM (200H + variable LOW, 42 bits) |
| `holtek.c` | `"Holtek"` | `m1_holtek_base_decode.c` | Custom inverted-pair after start bit (430/870, 40 bits); distinct from existing `"Holtek_HT12X"` |
| `intertechno_v3.c` | `"Intertechno_V3"` | `m1_intertechno_v3_decode.c` | Custom 4-pulse-per-bit (275/1375, 32 bits); coexists with `"Intertechno"` |
| `kia.c` | `"KIA Seed"` | `m1_kia_seed_decode.c` | Custom preamble-scan + PDM (250/500, 61 bits) |
| `legrand.c` | `"Legrand"` | `m1_legrand_decode.c` | Custom inverted PWM (375/1125, 18 bits) |
| `linear_delta3.c` | `"LinearDelta3"` | `m1_linear_delta3_decode.c` | Custom 1:7 ratio (500/2000, 8 bits); distinct from `"Linear"` |
| `magellan.c` | `"Magellan"` | `m1_magellan_decode.c` | Generic PWM (200/400, 32 bits) |
| `marantec24.c` | `"Marantec24"` | `m1_marantec24_decode.c` | Custom asymmetric timing (800/1600, 24 bits); distinct from `"Marantec"` |
| `nero_sketch.c` | `"Nero Sketch"` | `m1_nero_sketch_decode.c` | Custom preamble-scan + OOK PWM (330/660, 40 bits); distinct from `"Nero Radio"` |
| `phoenix_v2.c` | `"Phoenix_V2"` | `m1_phoenix_v2_decode.c` | Custom inverted-pair after start bit (427/853, 52 bits) |
| `revers_rb2.c` | `"Revers_RB2"` | `m1_revers_rb2_decode.c` | Generic Manchester (250/500, 64 bits) |
| `roger.c` | `"Roger"` | `m1_roger_decode.c` | Generic PWM (500/1000, 28 bits) |
| `somfy_keytis.c` | `"Somfy Keytis"` | `m1_somfy_keytis_decode.c` | Generic Manchester (640/1280, 80 bits); distinct from `"Somfy Telis"` |

#### Sub-GHz Phase 3 — Weather/Sensor protocols (March 2026)

The following 12 weather/sensor protocols were added. PPM-based decoders use the
new `subghz_decode_generic_ppm()` generic decoder utility.

| Flipper source file | `SUBGHZ_PROTOCOL_*_NAME` | M1 decoder | Notes |
|---------------------|--------------------------|------------|-------|
| `auriol_ahfl.c` | `"Auriol_AHFL"` | `m1_auriol_ahfl_decode.c` | Generic PPM (42 bits, weather sensor) |
| `auriol_hg0601a.c` | `"Auriol_HG0601A"` | `m1_auriol_hg0601a_decode.c` | Generic PPM (37 bits, weather sensor) |
| `gt_wt_02.c` | `"GT-WT-02"` | `m1_gt_wt02_decode.c` | Generic PPM (37 bits, weather sensor) |
| `kedsum_th.c` | `"Kedsum-TH"` | `m1_kedsum_th_decode.c` | Generic PPM (42 bits, temp/humidity sensor) |
| `solight_te44.c` | `"Solight_TE44"` | `m1_solight_te44_decode.c` | Generic PPM (36 bits, weather sensor) |
| `thermopro_tx4.c` | `"ThermoPro_TX4"` | `m1_thermopro_tx4_decode.c` | Generic PPM (37 bits, weather sensor) |
| `vauno_en8822c.c` | `"Vauno_EN8822C"` | `m1_vauno_en8822c_decode.c` | Generic PPM (42 bits, weather sensor) |
| `acurite_606tx.c` | `"Acurite_606TX"` | `m1_acurite_606tx_decode.c` | Generic PPM (32 bits, temp sensor) |
| `acurite_609txc.c` | `"Acurite_609TXC"` | `m1_acurite_609txc_decode.c` | Generic PPM (40 bits, temp/humidity sensor) |
| `emos_e601x.c` | `"Emos_E601x"` | `m1_emos_e601x_decode.c` | Generic PWM (24 bits, weather sensor) |
| `lacrosse_tx141thbv2.c` | `"LaCrosse_TX141THBv2"` | `m1_lacrosse_tx141thbv2_decode.c` | Generic PWM (40 bits, weather sensor) |
| `wendox_w6726.c` | `"Wendox_W6726"` | `m1_wendox_w6726_decode.c` | Generic PWM (29 bits, weather sensor) |

#### Sub-GHz Phase 4 — Remote/Gate/Automation protocols (March 2026)

| Flipper source file | `SUBGHZ_PROTOCOL_*_NAME` | M1 decoder | Notes |
|---------------------|--------------------------|------------|-------|
| `ditec_gol4.c` | `"DITEC_GOL4"` | `m1_ditec_gol4_decode.c` | PWM (54 bits, gate remote, dynamic/rolling) |
| `elplast.c` | `"Elplast"` | `m1_elplast_decode.c` | Inverted PWM (18 bits, remote, static) |
| `honeywell_wdb.c` | `"Honeywell_WDB"` | `m1_honeywell_wdb_decode.c` | PWM w/ parity (48 bits, wireless doorbell, static) |
| `keyfinder.c` | `"KeyFinder"` | `m1_keyfinder_decode.c` | Inverted PWM (24 bits, keyfinder tag, static) |
| `x10.c` | `"X10"` | `m1_x10_decode.c` | PWM w/ preamble (32 bits, home automation, dynamic) |

#### Sub-GHz protocols gap (remaining — no unported protocols as of March 2026)

All protocols identified in the March 2026 gap analysis have now been ported
(17 Phase 2 + 12 Phase 3 weather + 5 Phase 4 remote/gate + 5 Phase 5 advanced weather = 39 new protocols total).
If new Flipper protocols appear in future `dev` branch updates, repeat the
analysis by comparing Flipper's `lib/subghz/protocols/` against M1's
`protocol_text[]` array.

#### Security+ 1.0 naming compatibility — RESOLVED

Flipper saves Security+ 1.0 signals as `Protocol: Security+ 1.0`
(`SUBGHZ_PROTOCOL_SECPLUS_V1_NAME` in `secplus_v1.h`).  The M1 registry now
has the `LIFTMASTER_10BIT` entry registered under the name `"Security+ 1.0"`
(matching Flipper exactly).  The registry-based `.sub` file loader resolves
this name correctly — it looks up `"Security+ 1.0"` in the registry, finds
it as `SubGhzProtocolTypeDynamic`, and rejects it with "rolling code" (which
is correct behaviour — Security+ 1.0 uses rolling codes).

The base Chamberlain entry remains as `"Cham_Code"` (matching Flipper's
`SUBGHZ_PROTOCOL_CHAMB_CODE_NAME`).  Cham_Code `.sub` files now load
correctly via the registry-based lookup and are encoded using the protocol's
timing ratio from the registry.

#### `.sub` file interop — registry-based matching

The `.sub` file KEY-type protocol matching in `m1_sub_ghz.c` now uses the
protocol registry (`subghz_protocol_find_by_name()`) as the primary lookup:

1. Protocol name is looked up in the registry
2. `SubGhzProtocolTypeDynamic` / `Weather` / `TPMS` → rejected as rolling code
3. `SubGhzProtocolTypeStatic` → encoding ratio computed from registry timing
4. Unknown protocol → falls back to legacy `strstr()` matching

This fixed multiple interop issues:
- **Cham_Code** — was previously unmatched (no strstr entry)
- **Marantec24** — was incorrectly caught by `strstr("Marantec")` rolling-code check
- **All Phase 2 static protocols** — Clemsa, BETT, MegaCode, Centurion, etc.
  were missing from both strstr branches

### Dual `.sub` / `.sgh` File Format Support

M1 has two Sub-GHz signal file formats:

| Extension | Format | Description |
|-----------|--------|-------------|
| `.sub` | Flipper-compatible | Key-value text file; `Filetype: Flipper SubGhz Key File` or `RAW`. Preferred for cross-device interoperability. |
| `.sgh` | M1 native | M1's internal data file; uses `Modulation:`, `Data:`, `Payload:`, `BT:`, `Bits:` headers with unsigned timing values. |

**`sub_ghz_replay_flipper_file()` handles both transparently** — it detects the
format by keyword matching (`Preset:` vs `Modulation:`, `RAW_Data:` vs `Data:`,
`Bit:` vs `Bits:`, `TE:` vs `BT:`, etc.), not by file extension.  Both formats
are transcoded to a temporary `.sgh` and replayed identically.

#### Rules for any new Sub-GHz code that loads, lists, or parses files

These rules apply to any imported Sub-GHz feature that touches signal files —
file browsers, loaders, info screens, playlist parsers, savers, etc.

1. **Never filter by extension alone.** File browsers for Sub-GHz signals must
   display both `.sub` and `.sgh` files.  `storage_browse()` has no extension
   filter; do not add one that excludes either format.

2. **Never detect format by file extension.** Always detect format by inspecting
   file content keywords.  Any new parser must use keyword-based dispatch —
   check for `Filetype:`, `Preset:`, `Modulation:`, `RAW_Data:`, `Data:`, etc.

3. **Playlist entries accept both formats.** The `sub:` line prefix in a `.txt`
   playlist file is a field-name tag (inherited from Flipper's playlist format)
   — it does **not** mean the referenced path must end in `.sub`.  Both of these
   are valid playlist entries:
   ```
   sub: /SUBGHZ/Tesla_AM270.sub
   sub: /SUBGHZ/my_raw_capture.sgh
   ```

4. **Info screens must handle both formats.** When displaying file metadata
   (protocol, frequency, key/payload), use keyword-based parsing that works on
   both formats.  `flipper_subghz_load()` handles `.sub`; for `.sgh` files,
   parse `Modulation:`, `Frequency:`, `Payload:`, and `BT:` headers directly.

5. **Save format is user-configurable.** The active save format is stored in
   `subghz_cfg.save_fmt` (0 = Flipper `.sub`, 1 = M1 native `.sgh`) and toggled
   in Settings → Sub-GHz.  New code that saves a captured signal must call
   `subghz_get_save_fmt_ext()` to determine the correct extension and format —
   never hardcode `.sub` or `.sgh` as the output format.

### LF-RFID `.rfid` files

Protocol names are defined as `LFRFID_PROTOCOL_*` enum values in
`lib/lfrfid/protocols/lfrfid_protocol.h` in the Flipper repository.
Match them against M1's `lfrfid/lfrfid_protocol.h`.

### IR `.ir` files

IR protocol names map to IRMP protocol IDs in `m1_csrc/flipper_ir.c`.
When Flipper adds a new IR protocol, add the name-to-IRMP-ID mapping in
`flipper_ir_protocol_to_irmp()`.

#### IR protocols not yet mapped — NONE

All Flipper IR protocol names are mapped in the `ir_proto_table[]` in
`flipper_ir.c`.  `NEC42ext` was added and maps to `IRMP_NEC42_PROTOCOL`.
If Flipper adds new IR protocols in future updates, add the
name-to-IRMP-ID mapping to `ir_proto_table[]`.

### NFC `.nfc` files

NFC card types are parsed in `m1_csrc/flipper_nfc.c`. When Flipper adds a
new NFC device type, add handling in `flipper_nfc_load()`.

**Current type coverage** (as of Hapax.9 / March 2026):

| Flipper type string | `flipper_nfc_type_t` enum | M1 tech |
|---------------------|--------------------------|---------|
| ISO14443-3A, Mifare Classic, NTAG, Mifare Plus, Mifare DESFire | `_ISO14443_3A / _MIFARE_CLASSIC / _NTAG / _ISO14443_4A / _MIFARE_DESFIRE` | `M1NFC_TECH_A` |
| ISO14443-3B, ST25TB, SRIx series | `_ISO14443_3B / _ST25TB` | `M1NFC_TECH_B` |
| FeliCa, Suica, PASMO | `_FELICA` | `M1NFC_TECH_F` |
| ISO15693-3, SLIX, SLI series | `_ISO15693 / _SLIX` | `M1NFC_TECH_V` |

---

## Monstatek Pattern Deviation Log

This section tracks every case where a Flipper/fork pattern was adopted **instead of**
the corresponding Monstatek/M1 pattern.  Each entry documents what was done differently
and why, so that deviations can be reviewed and potentially reconciled once Monstatek
ships a release above 1.0.0.0.

> **Agent instruction:** Whenever you adopt a Flipper pattern over a conflicting Monstatek
> pattern, add a row to the table below **in the same commit**.  Be specific about which
> files are affected so a future agent can locate and reconcile the deviation.

| Date | Area | Flipper pattern adopted | Monstatek pattern diverged from | Affected M1 files | Rationale | Resolution status |
|------|------|------------------------|--------------------------------|-------------------|-----------|-------------------|
| 2025-03 | Sub-GHz decoder structs | `SubGhzBlockDecoder` / `SubGhzBlockGeneric` with `commit_to_m1()` bridge | Direct writes to global `subghz_decenc_ctl` fields inside decode loops | `Sub_Ghz/subghz_block_decoder.h`, `subghz_block_generic.h/.c`, all `m1_*_decode.c` files using new helpers | Flipper's struct-based approach isolates decode state, prevents partial-decode globals corruption, and matches upstream Flipper protocol ports 1:1 | 🔲 Unresolved — review when Monstatek > 1.0.0.0 |
| 2025-03 | Sub-GHz protocol registry | `SubGhzProtocolDef` table with `SubGhzBlockConst` timing, type/flag/filter enums | Parallel `subghz_protocols_list[]` + `protocol_text[]` + switch-case dispatch | `Sub_Ghz/subghz_protocol_registry.h/.c`, `Sub_Ghz/m1_sub_ghz_decenc.c` (legacy arrays populated from registry) | Single-source-of-truth registry eliminates index sync bugs; Flipper protocol metadata (type, flags, filter) enables `.sub` KEY interop and static-TX gating | 🔲 Unresolved — review when Monstatek > 1.0.0.0 |
| 2025-03 | Sub-GHz helper macros | `DURATION_DIFF`, `subghz_protocol_blocks_add_bit`, `bit_read`/`bit_set`/`bit_clear` from Flipper's `subghz_blocks_math.h` | Ad-hoc inline comparisons and manual bit manipulation | `Sub_Ghz/subghz_blocks_math.h`, protocol decoders using these helpers | Consistent helper API across all protocol decoders; matches Flipper source making ports trivial | 🔲 Unresolved — review when Monstatek > 1.0.0.0 |
| 2025-03 | Sub-GHz Manchester codec | Flipper's `manchester_advance()` / `manchester_encoder_advance()` inline functions | No Manchester abstraction; each protocol hand-rolled Manchester logic | `Sub_Ghz/subghz_manchester_decoder.h`, `subghz_manchester_encoder.h` | Eliminates duplicated Manchester code across Somfy Telis, Somfy Keytis, Marantec, FAAC SLH, Revers RB2 | 🔲 Unresolved — review when Monstatek > 1.0.0.0 |

### Deviation entry template

When adding a new deviation, copy this row:

```markdown
| YYYY-MM | <area> | <what Flipper pattern was adopted> | <what Monstatek does differently> | <list of M1 files affected> | <why Flipper was preferred> | 🔲 Unresolved — review when Monstatek > 1.0.0.0 |
```

---

## Checklist for Each Import

- [ ] Monstatek/M1 upstream checked for the protocol (fetch `monstatek/main`)
- [ ] If Monstatek has it: implementation compared against Flipper; Flipper pattern adopted if divergent (see [Pattern Adoption Policy](#pattern-adoption-policy))
- [ ] If Flipper pattern adopted over Monstatek: deviation logged in [Monstatek Pattern Deviation Log](#monstatek-pattern-deviation-log)
- [ ] If Monstatek does NOT have it: new Flipper protocol identified (name, file, timing constants)
- [ ] `m1_<proto>_decode.c` created in `Sub_Ghz/protocols/`
- [ ] Entry added to `subghz_protocol_registry[]` in `Sub_Ghz/subghz_protocol_registry.c` (name, timing, type, decode fn)
- [ ] `extern` forward declaration added at top of `subghz_protocol_registry.c`
- [ ] Protocol index enum/define added to `m1_sub_ghz_decenc.h`
- [ ] New source file added to `cmake/m1_01/CMakeLists.txt`
- [ ] GPLv3 attribution comment added (if porting Flipper code)
- [ ] `README_License.md` updated (if porting Flipper code)
- [ ] Firmware builds without errors
- [ ] Any scrollable list / menu UI uses Hapax font-aware helpers (`m1_menu_font()`, `m1_menu_item_h()`, `m1_menu_max_visible()`, `M1_MENU_VIS()`) — see CLAUDE.md "User-Configurable Font Size"
- [ ] `.sub` file round-trip tested on hardware or in simulation

---

## Example: What a Completed Import Looks Like

When Flipper adds a new protocol called `FooBar` with
`#define SUBGHZ_PROTOCOL_FOOBAR_NAME "FooBar"` and `te_short = 300 us`:

1. **New file** `Sub_Ghz/protocols/m1_foobar_decode.c` — decoder logic.
2. **New file** `Sub_Ghz/protocols/m1_foobar_decode.h` — function prototype.
3. **`Sub_Ghz/m1_sub_ghz_decenc.c`** — new row `{300, 900, PACKET_PULSE_TIME_TOLERANCE20, 0, 24}` in
   `subghz_protocols_list[]` and `"FooBar"` in `protocol_text[]`.
4. **`Sub_Ghz/m1_sub_ghz_decenc.h`** — new `SUBGHZ_PROTO_FOOBAR` enum/define.
5. **`CMakeLists.txt`** — `Sub_Ghz/protocols/m1_foobar_decode.c` in source list.
6. **`README_License.md`** — entry for `m1_foobar_decode.c` with Flipper GPLv3
   attribution.
