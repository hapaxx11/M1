---
name: vendored-deps
description: Rules and audit procedure for updating vendored dependencies (u8g2, FreeRTOS, FatFs, IRMP), including the Local Modification Registry that must be preserved across upstream updates. Load before touching Drivers/u8g2_*, Middlewares/FreeRTOS/, FatFs/, or Infrared/.
---

# Vendored Dependency Updates

> Extracted from CLAUDE.md. Load before updating any vendored library.

## Vendored Dependency Update Rules

> **NEVER do a wholesale file replacement of vendored library directories without
> first auditing for M1-specific (local) modifications.**  Blindly replacing files
> with upstream versions has caused real regressions — the most critical being
> the loss of `U8G2_REF_MAN_PIC` which made `m1_message_box()` block forever.

### Local Modification Registry

Before updating any vendored dependency, check this registry.  Every M1-specific
modification to a vendored file is listed here.  After the update, verify that
each modification is still present.  If a modification was lost, re-apply it.

| Directory | File | Modification | Purpose | Added by |
|-----------|------|-------------|---------|----------|
| `Drivers/u8g2_csrc/` | `u8g2.h` | `#define U8G2_REF_MAN_PIC` | Makes u8g2 built-in UI widgets (message box, selection list, input value) draw once and return immediately instead of entering u8g2's internal GPIO polling loop. Required because M1 uses FreeRTOS queue-based event handling, not u8g2's `u8x8_GetMenuEvent()`. Without this, `m1_message_box()` blocks forever. | Thomas (2025-12-08) |
| `Middlewares/FreeRTOS/` | `portable/MemMang/heap_4.c` | `pvPortRealloc()` function | FreeRTOS upstream dropped `pvPortRealloc` in V11.  M1's `Core/Src/memmgr.c` wraps libc `realloc` through it via `--wrap`. Must be re-added after any FreeRTOS upgrade. | Hapax |
| `Middlewares/FreeRTOS/` | `include/portable.h` | `void *pvPortRealloc(void *pv, size_t xWantedSize);` declaration | Companion declaration for the `pvPortRealloc` implementation in `heap_4.c`. | Hapax |

> **FreeRTOS upgrades also endanger the rest of the #526 heap redirect, not just
> `pvPortRealloc`.**  After any FreeRTOS / newlib / linker update, run the full
> **Heap-Redirect Component Checklist** in the "Heap Redirect — `malloc()` ≡
> `pvPortMalloc()`" section (Architecture Rules).  Verify all 8 components,
> including the `--wrap` flags, the `memmgr.c` link, the `sysmem.c` exclusion, and
> `configTOTAL_HEAP_SIZE`.  Breaking any one silently reverts to a broken or
> split-heap state (the root-cause concern behind issue #610).

### Mandatory Audit Procedure

When updating **any** vendored dependency (`Drivers/u8g2_csrc/`, `Drivers/u8g2_cppsrc/`,
`FatFs/`, `Middlewares/FreeRTOS/`, `Infrared/`):

1. **Before replacing files**, generate a diff of the existing vendored files against
   the *previous* upstream version (not `origin/main`, but the upstream tag the files
   were originally sourced from).  Any hunks that are NOT in upstream are M1 local
   modifications.
   ```bash
   # Example: audit u8g2 for local modifications before update
   git clone --depth 1 --branch v2.35.x https://github.com/olikraus/u8g2.git /tmp/u8g2-old
   diff -rq Drivers/u8g2_csrc/ /tmp/u8g2-old/csrc/
   # Files that differ → inspect each diff for M1-specific changes
   ```

2. **Check the Local Modification Registry** above.  Every listed modification MUST
   be preserved or re-applied after the update.

3. **After replacing files**, re-apply all registered modifications.  Add a comment
   block near each modification explaining its purpose, so the next update cycle
   does not accidentally remove it:
   ```c
   /* M1 LOCAL MODIFICATION — do not remove during upgrades.
      <explanation of why this modification exists> */
   ```

4. **If you discover a NEW local modification** not in the registry, add it to the
   registry table in this section before committing.

5. **Build and test** — run `cmake -B build-tests -S tests && cmake --build build-tests &&
   ctest --test-dir build-tests --output-on-failure` after every vendored update.

6. **Never assume "no local modifications"** — always verify by diffing against the
   known upstream version.  The commit message from the original import often notes the
   upstream tag/SHA, or check `SOURCES.md` / changelog entries.

### M1-Specific Config Files (Never Replace)

These files live alongside vendored code but are **entirely M1-specific**.  They must
NEVER be replaced during an upstream update:

| Directory | Files | Purpose |
|-----------|-------|---------|
| `FatFs/` | `diskio.c`, `diskio.h`, `ff_gen_drv.c`, `ffsystem_baremetal.c`, `ffsystem_cmsis_os.c` | SD card driver, M1 FreeRTOS integration |
| `FatFs/` | `ffconf.h` | FatFs configuration — update individual defines as needed, never wholesale replace |
| `Core/Inc/` | `FreeRTOSConfig.h` | FreeRTOS configuration — add new required defines, never wholesale replace |
| `Infrared/` | `irmp.c`, `irsnd.c`, `irmp_data.h`, `irsnd_data.h`, STM32 HAL files | M1 IRMP implementation — only headers are synced from upstream |

---
