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
| UI framework, display rendering, keypad handling | **Monstatek/M1** | M1's UIView framework differs fundamentally from Flipper's ViewPort model. |

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
| Sub-GHz dispatch table | `Sub_Ghz/m1_sub_ghz_decenc.c` — `subghz_protocols_list[]` and `protocol_text[]` |
| Sub-GHz dispatch header | `Sub_Ghz/m1_sub_ghz_decenc.h` |
| LF-RFID protocol files | `lfrfid/lfrfid_protocol_<proto>.c` and `.h` |
| LF-RFID registry | `lfrfid/lfrfid_protocol.c` — `lfrfid_protocols[]` table |
| Flipper file parsers | `m1_csrc/flipper_*.c` and `.h` |
| Flipper integration API | `m1_csrc/m1_flipper_integration.c` / `.h` |
| Build list | `CMakeLists.txt` — `target_sources(...)` section |

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

The 74 decoder files in `Sub_Ghz/protocols/` were **independently re-implemented** based on
the same RF timing specifications as Flipper (so `.sub` files remain compatible), but the M1
code is not a copy of Flipper source.  The sole exception is noted below.  The total of 74
comprises 73 named protocol decoders plus one utility file (`m1_bin_raw_decode.c`).

| M1 file | Flipper origin | Notes |
|---------|---------------|-------|
| `Sub_Ghz/protocols/m1_chamberlain_decode.c` | `lib/subghz/protocols/secplus_v1.c` | Contains direct port of argilo/secplus ternary algorithm; GPLv3 attribution present |
| All other `Sub_Ghz/protocols/m1_*_decode.c` (73 files) | Various | **Independent ports** — same algorithm, original M1 implementation; no Flipper source copied |

**Subtotal:** 1 directly-copied file; 72 independent ports (+ 1 bin_raw utility file)

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
| Sub-GHz decoders | 74 | 1 | 72 (+ 1 bin_raw) |
| Furi HAL reference (docs) | 45 | 45 | — |
| Flipper file parsers | 10 | 0 | 10 |
| **Total** | **186** | **58 compiled + 45 docs** | **82** |

The **directly-copied compiled files** total ~58, comfortably below the 80-file submodule
threshold.  Continue monitoring this count as new LF-RFID protocols are ported.

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
 *  Ported from Flipper Zero firmware
 *  https://github.com/flipperdevices/flipperzero-firmware
 *  Original file: lib/subghz/protocols/<proto>.c
 *  Copyright (C) Flipper Devices Inc. — Licensed under GPLv3
 *
 * M1 Project
 */

#include <string.h>
#include <stdlib.h>
#include "stm32h5xx_hal.h"
#include "bit_util.h"
#include "m1_sub_ghz_decenc.h"
#include "m1_log_debug.h"

#define M1_LOGDB_TAG  "SUBGHZ_<PROTO>"

/* ---- timing constants (from Flipper <proto>.h) ---- */
#define TE_SHORT   <value_us>
#define TE_LONG    <value_us>

/* ---- decoder implementation ---- */
uint8_t subghz_decode_<proto>(uint16_t p, uint16_t pulsecount)
{
    /* port Flipper decode logic here */
    return 0;
}
```

Create the matching `m1_<proto>_decode.h` with the function prototype.

#### 2b. Add timing entry to `subghz_protocols_list[]`

In `Sub_Ghz/m1_sub_ghz_decenc.c`, add a row to `subghz_protocols_list[]`:

```c
{TE_SHORT, TE_LONG, PACKET_PULSE_TIME_TOLERANCE20, <preamble_bits>, <data_bits>},
```

Keep the order consistent with `protocol_text[]` — both arrays are indexed by
the same `SUBGHZ_PROTO_*` enum value.

#### 2c. Add the protocol name to `protocol_text[]`

Use the **exact** string from Flipper's `#define SUBGHZ_PROTOCOL_*_NAME`:

```c
"<Flipper SUBGHZ_PROTOCOL_*_NAME>",   /* SUBGHZ_PROTOCOL_*_NAME */
```

If no Flipper equivalent exists for the protocol, keep the M1 display name and
note it with `/* no Flipper lib/subghz equivalent */`.

#### 2d. Add enum entry in `m1_sub_ghz_decenc.h`

Add a new constant to the protocol enum (or `#define` block) that identifies
the protocol index. Keep it in the same position as in `subghz_protocols_list[]`
and `protocol_text[]`.

#### 2e. Register the decoder callback

In `Sub_Ghz/m1_sub_ghz_decenc.c`, inside `subghz_decode_protocol()`, add a
case for the new index that calls `subghz_decode_<proto>()`.

### Step 3 — Port an LF-RFID protocol

1. Create `lfrfid/lfrfid_protocol_<proto>.c` and `.h`, following the pattern of
   `lfrfid/lfrfid_protocol_em4100.c`.
2. Add the GPLv3 header comment referencing the Flipper source file.
3. Register the protocol in the `lfrfid_protocols[]` table in
   `lfrfid/lfrfid_protocol.c`.

### Step 4 — Update the build system

Add each new source file to `CMakeLists.txt` inside the `target_sources(M1 PRIVATE ...)` block:

```cmake
Sub_Ghz/protocols/m1_<proto>_decode.c
```

or for LF-RFID:

```cmake
lfrfid/lfrfid_protocol_<proto>.c
```

### Step 5 — Verify `.sub` / `.rfid` file interoperability

After adding the protocol:

- Confirm the string in `protocol_text[]` matches `SUBGHZ_PROTOCOL_*_NAME`
  from the Flipper header exactly (case-sensitive, spaces vs underscores matter).
- Check the existing name mappings in `m1_sub_ghz_decenc.c` (lines ~107-164)
  for precedent — some names differ from naive expectations (e.g.
  `"Holtek_HT12X"` not `"Holtek"`, `"GateTX"` not `"Gate TX"`).
- Consult `CLAUDE.md` → *Flipper compatibility* section for known name corrections.

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
in `Sub_Ghz/protocols/`, an entry in `subghz_protocols_list[]`,
`protocol_text[]`, the enum, and the dispatch switch.

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

#### Sub-GHz protocols gap (remaining — no unported protocols as of March 2026)

All protocols identified in the March 2026 gap analysis have now been ported.
If new Flipper protocols appear in future `dev` branch updates, repeat the
analysis by comparing Flipper's `lib/subghz/protocols/` against M1's
`protocol_text[]` array.

#### Security+ 1.0 naming compatibility issue

Flipper saves Security+ 1.0 signals as `Protocol: Security+ 1.0`
(`SUBGHZ_PROTOCOL_SECPLUS_V1_NAME` in `secplus_v1.h`).  M1's Security+ 1.0
decode logic already exists inside `m1_chamberlain_decode.c` and
`m1_liftmaster_decode.c`, but those entries appear in `protocol_text[]` as
`"Cham_Code"` and `"Liftmaster"` respectively.

**Impact:** A `.sub` file captured on a Flipper and saved as `Security+ 1.0`
will **fail to load** on M1 because no `protocol_text[]` entry matches the
string `"Security+ 1.0"`.

**Fix required:** Add `"Security+ 1.0"` as an additional entry in
`protocol_text[]` (and a corresponding alias entry in `subghz_protocols_list[]`
and `subghz_decode_protocol()`) that routes to the existing Security+ 1.0
decode path.  The existing `"Cham_Code"` and `"Liftmaster"` entries must be
kept for backwards compatibility with M1-saved files.

### LF-RFID `.rfid` files

Protocol names are defined as `LFRFID_PROTOCOL_*` enum values in
`lib/lfrfid/protocols/lfrfid_protocol.h` in the Flipper repository.
Match them against M1's `lfrfid/lfrfid_protocol.h`.

### IR `.ir` files

IR protocol names map to IRMP protocol IDs in `m1_csrc/flipper_ir.c`.
When Flipper adds a new IR protocol, add the name-to-IRMP-ID mapping in
`flipper_ir_protocol_to_irmp()`.

#### IR protocols not yet mapped (gap analysis — March 2026)

| Flipper `InfraredProtocol` enum | Flipper name string | Fix |
|---------------------------------|---------------------|-----|
| `InfraredProtocolNEC42ext` | `"NEC42ext"` | Add `{ "NEC42ext", IRMP_NEC42_PROTOCOL }` to the mapping table in `flipper_ir.c` — NEC42 extended addressing uses the same IRMP decoder |

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
- [ ] `m1_<proto>_decode.c` and `.h` created in `Sub_Ghz/protocols/`
- [ ] Timing entry added to `subghz_protocols_list[]`
- [ ] Protocol name added to `protocol_text[]` (matches Flipper constant exactly)
- [ ] Protocol index enum/define added to `m1_sub_ghz_decenc.h`
- [ ] Decoder callback registered in `subghz_decode_protocol()`
- [ ] New source file added to `CMakeLists.txt`
- [ ] GPLv3 attribution comment added (if porting Flipper code)
- [ ] `README_License.md` updated (if porting Flipper code)
- [ ] Firmware builds without errors
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
