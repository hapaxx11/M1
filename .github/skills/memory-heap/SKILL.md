---
name: memory-heap
description: FreeRTOS heap-4 redirect (malloc == pvPortMalloc), the 8-component Heap-Redirect Checklist to verify after any RTOS/libc/linker update, heap anti-patterns (ISR allocation, NULL checks, unbounded growth), and the idempotent heap-init rule. Load when touching malloc/free, FreeRTOS, the linker, or any *_init that allocates.
---

# Memory & Heap Rules

> Extracted from CLAUDE.md Architecture Rules. See also documentation/memory_management.md.

### Heap Redirect — `malloc()` ≡ `pvPortMalloc()`

All libc heap calls are **globally redirected** to the FreeRTOS heap-4 allocator
via linker `--wrap` flags (see `cmake/gcc-arm-none-eabi.cmake` and
`Core/Src/memmgr.c`).  There is **no separate newlib `_sbrk` heap**.

**Rules for agents:**

1. **Plain `malloc()` / `free()` is fine** — it transparently hits FreeRTOS heap-4.
   New code should prefer `malloc()` / `free()` for readability.
2. **Existing `pvPortMalloc` / `vPortFree` call sites do not need conversion.**
   Both spellings go to the same allocator.
3. **Never allocate from ISR context** — `pvPortMalloc` calls `vTaskSuspendAll()`.
   Any `malloc` (or libc function that allocates internally) called from an
   interrupt handler will assert or hang.
4. `realloc()` is supported (via `pvPortRealloc` in `heap_4.c`).
5. All allocations share a single `configTOTAL_HEAP_SIZE` pool.
6. The old guidance "must use `pvPortMalloc`/`vPortFree` instead of
   `malloc`/`free`" is **obsolete** — they are now equivalent.

#### 🔒 Heap-Redirect Component Checklist — verify after EVERY RTOS / libc / linker update

> **The heap redirect introduced in #526 is a multi-component system.  ALL of the
> pieces below must coexist — removing or breaking any one silently reverts the
> firmware to a broken or split-heap state.  This is not hypothetical: the
> FreeRTOS V10.5.1 → V11.3.0 upgrade in #589 dropped `pvPortRealloc` and the
> firmware failed to link until it was restored *within the same PR*.  Issue #610
> ("SubGhz Read Raw Broken") was filed because the owner suspected #589 had
> reverted the #526 heap behaviour.**
>
> **Whenever you upgrade FreeRTOS, newlib/picolibc, the linker, or touch
> `cmake/gcc-arm-none-eabi.cmake` / `cmake/m1_01/CMakeLists.txt`, you MUST
> re-verify every row of this table before considering the update complete.
> Never "override" or remove any of these during a vendored RTOS/library update.**

| # | Component | Location | How to verify |
|---|-----------|----------|---------------|
| 1 | `__wrap_*` shim (malloc/free/calloc/realloc + `_r` variants) | `Core/Src/memmgr.c`, `Core/Inc/memmgr.h` | File present and compiled |
| 2 | Linker `--wrap` flags (all 8) | `cmake/gcc-arm-none-eabi.cmake` | `--wrap=malloc,free,calloc,realloc` **and** `--wrap=_malloc_r,_free_r,_calloc_r,_realloc_r` |
| 3 | `memmgr.c` linked into firmware | `cmake/m1_01/CMakeLists.txt` | `../../Core/Src/memmgr.c` present in `target_sources` |
| 4 | `sysmem.c` (newlib `_sbrk` heap) **excluded** | `cmake/m1_01/CMakeLists.txt` | `sysmem.c` is **NOT** referenced anywhere under `cmake/` |
| 5 | `pvPortRealloc()` (M1 local mod) | `Middlewares/FreeRTOS/.../heap_4.c` + `include/portable.h` | Implementation + declaration both present (see Local Modification Registry in the `vendored-deps` skill) |
| 6 | `pvPortCalloc()` | `Middlewares/FreeRTOS/.../heap_4.c` + `portable.h` | Present (native to FreeRTOS ≥ V11; restore if a future kernel drops it) |
| 7 | `configTOTAL_HEAP_SIZE` unchanged | `Core/Inc/FreeRTOSConfig.h` | `262144` (256 KB) — do not shrink |
| 8 | `test_memmgr` green | `tests/test_memmgr.c` | `ctest -R memmgr` passes |

> **Do NOT route large or hot-path allocations through `malloc_critical` /
> `m1_malloc`.**  Wrapping `pvPortMalloc`/`vPortFree` (which suspend the
> scheduler) inside `taskENTER_CRITICAL()` is an anti-pattern (see AP-1) that the
> FreeRTOS V11 upgrade made riskier.  The Read Raw record path allocates its SD
> write buffer (`m1_sdm_memory_init`, `m1_csrc/m1_sdcard_man.c`) and capture ring
> buffers (`sub_ghz_ring_buffers_init`, `m1_csrc/m1_sub_ghz.c`) **directly via
> `pvPortMalloc`/`vPortFree`** for this reason.  Keep them on the direct FreeRTOS
> heap-4 path; do not convert them back to `m1_malloc` during future updates.

#### ⚠ Heap anti-patterns — check for EVERY new feature

Whenever you add or modify a feature, verify all four of the following.  These
are real failure modes introduced by the single-heap architecture.

**AP-1 · ISR-context allocation (hangs / asserts)**

> Scan every new or modified interrupt handler (`*_IRQHandler`, callbacks
> registered with `HAL_*_RegisterCallback`, timer callbacks, DMA callbacks).
> If any of them — directly or via a called function — can reach `malloc`,
> `free`, `printf`, `sprintf`, or any other libc function that may allocate
> internally, flag it as a defect.  The fix is to pre-allocate before entering
> the ISR and pass data via a queue or shared pointer.

**AP-2 · Ignoring NULL return from `malloc` / `pvPortMalloc` (heap overflow)**

> All allocations from a fixed `configTOTAL_HEAP_SIZE` pool can fail.  Every
> call to `malloc()` / `pvPortMalloc()` in new code must guard the return value.
> Never dereference the pointer without a NULL check.  On NULL, either return an
> error code to the caller or call `configASSERT(0)` if the allocation is truly
> required for the system to continue.

**AP-3 · Unbounded allocation growth (pool exhaustion)**

> When a new feature adds heap allocation — especially large or persistent
> buffers — account for the bytes in `configTOTAL_HEAP_SIZE`
> (`Core/Inc/FreeRTOSConfig.h`).  Add a comment near the allocation with the
> expected size so reviewers can reason about the budget.  Call
> `xPortGetFreeHeapSize()` or `xPortGetMinimumEverFreeHeapSize()` in a debug
> build to confirm free headroom remains above ~8 KB after startup.

**AP-4 · `pvPortRealloc` / heap_4 coupling (silent breakage on upgrade)**

> `pvPortRealloc` (in `Middlewares/FreeRTOS/Source/portable/MemMang/heap_4.c`)
> reads the `BlockLink_t` header directly.  If you upgrade FreeRTOS, switch
> from heap_4 to heap_5, or otherwise change the heap layout, `pvPortRealloc`
> **must be reviewed and updated** before merging.  The `configASSERT` on
> `heapBLOCK_IS_ALLOCATED` will fire at runtime if the struct layout has
> changed, but only on a code path that calls `realloc` — not during startup.

### Heap-Allocation Init Functions — Idempotency Rule

> **Every `foo_init()` that only allocates heap memory and sets pointers MUST call
> `foo_deinit()` as its very first statement.**  Without this, calling `init()` twice in
> a row (e.g. after a missed deinit on an error path) leaks the first allocation and
> attempts a second `malloc` on an already-fragmented heap.

**The rule:**
- `foo_init()` calls `foo_deinit()` unconditionally at entry, then proceeds with `malloc`.
- `foo_deinit()` must be a no-op when pointers are NULL (guarded `free()` + NULL the pointer).
  That makes the deinit-at-entry call free for the common case where nothing is allocated.

**Canonical example** — `sub_ghz_ring_buffers_init()` in `m1_csrc/m1_sub_ghz.c`:

```c
static uint8_t sub_ghz_ring_buffers_init(void)
{
    /* Guard: release any stale buffers from a prior operation that was not
     * cleanly deinit'd.  deinit() is a no-op when pointers are NULL. */
    sub_ghz_ring_buffers_deinit();

    /* ... malloc, initialise, return 0 on success or 1 on failure ... */
}
```

**This pattern does NOT apply to hardware peripheral or RTOS init functions**
(`menu_sub_ghz_init()`, `m1_esp32_init()`, `infrared_encode_sys_init()`, etc.).
Those functions carry their own guards (status flags, HAL state checks, or strict
caller-owned lifecycle rules) and an unconditional deinit at entry would power off
a peripheral that is legitimately active.  See the state-management sections below
for the correct patterns for each hardware subsystem.
