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

## Step-by-Step Process

### Step 0 — Check Monstatek upstream first

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
   - Cherry-pick or merge the relevant commits from `monstatek/main`.
   - Adjust for any C3-specific additions (version fields, build-date injection,
     RPC extensions) before committing.
   - **Skip Steps 1–4 below** — the Flipper porting work is already done.
4. If Monstatek does **not** yet contain the protocol, continue with Step 1.

> **Why this matters:** Monstatek's code is already adapted to M1's architecture
> and has been tested on real hardware.  Pulling from there is faster and safer
> than re-porting from Flipper independently.

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

#### Sub-GHz protocols not yet ported (gap analysis — March 2026)

The following protocols are present in the Flipper `dev` branch but have no
counterpart in M1's `Sub_Ghz/protocols/` or `protocol_text[]`.  Each row lists
the Flipper source file, the exact `SUBGHZ_PROTOCOL_*_NAME` string that
**must** be used in `protocol_text[]`, and any relevant M1 notes.

| Flipper source file | `SUBGHZ_PROTOCOL_*_NAME` | M1 notes |
|---------------------|--------------------------|----------|
| `dickert_mahs.c` | `"Dickert_MAHS"` | Gate/garage remote |
| `feron.c` | `"Feron"` | Smart home switch |
| `gangqi.c` | `"GangQi"` | Rolling-code garage remote |
| `hay21.c` | `"Hay21"` | Decode-only (no encoder in Flipper) |
| `hollarm.c` | `"Hollarm"` | Security alarm keyfob |
| `holtek.c` | `"Holtek"` | **Base Holtek** encoder/decoder — distinct from `"Holtek_HT12X"` which M1 already has |
| `intertechno_v3.c` | `"Intertechno_V3"` | M1 has `"Intertechno"` (older variant) — this is a **separate, newer** protocol; both must coexist |
| `kia.c` | `"KIA Seed"` | KIA car key — decode-only |
| `legrand.c` | `"Legrand"` | Smart-home remote |
| `linear_delta3.c` | `"LinearDelta3"` | Linear gate — distinct from existing `"Linear"` |
| `magellan.c` | `"Magellan"` | Rolling-code remote |
| `marantec24.c` | `"Marantec24"` | M1 has `"Marantec"` — this 24-bit variant is a **separate** protocol |
| `nero_sketch.c` | `"Nero Sketch"` | M1 has `"Nero Radio"` — Nero Sketch is a **different** protocol |
| `phoenix_v2.c` | `"Phoenix_V2"` | Rolling-code (Phoenix) |
| `revers_rb2.c` | `"Revers_RB2"` | Rolling-code remote |
| `roger.c` | `"Roger"` | Roger/BFT gate remotes |
| `somfy_keytis.c` | `"Somfy Keytis"` | M1 has `"Somfy Telis"` — Keytis is a separate Somfy product line |

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

---

## Checklist for Each Import

- [ ] Monstatek/M1 upstream checked for the protocol (fetch `monstatek/main`)
- [ ] If Monstatek has it: cherry-pick/merge applied and C3 adjustments made
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
