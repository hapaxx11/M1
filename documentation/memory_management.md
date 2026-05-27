<!-- See COPYING.txt for license details. -->

# Memory Management — Global Heap Redirect

## Overview

The M1 firmware uses a **single unified heap**: the FreeRTOS heap-4 allocator.
All standard C library heap calls (`malloc`, `free`, `calloc`, `realloc` and
newlib-nano reentrant `_r` variants) are transparently redirected to FreeRTOS
via GNU linker `--wrap` flags.  The newlib `_sbrk`-backed heap
(`Core/Src/sysmem.c`) is no longer in the build.

This architecture is adopted from the
[Momentum firmware](https://github.com/Next-Flip/Momentum-Firmware) project,
which uses the same `--wrap` approach to unify all allocations under the RTOS
heap.

## Motivation

Before this change, the firmware had **two independent heaps**:

| Heap | Backed by | Used via |
|------|-----------|----------|
| Newlib heap | `_sbrk()` (grows upward from `_end`) | `malloc()` / `free()` |
| FreeRTOS heap-4 | Static array `ucHeap[configTOTAL_HEAP_SIZE]` | `pvPortMalloc()` / `vPortFree()` |

This dual-heap model caused several problems:

1. **Per-call-site conversion burden** — every allocation had to be audited and
   potentially rewritten from `malloc` → `pvPortMalloc`.  Missing a site meant
   that allocation came from the wrong (often undersized) heap.
2. **Heap fragmentation** — memory was fragmented across two pools rather than
   one, reducing the maximum allocatable block.
3. **Third-party code** — libraries that call `malloc` internally (e.g. newlib
   `printf` format parsing, `strtod`) could not be redirected without source
   changes.

The `--wrap` redirect eliminates all three issues: every `malloc` in the
firmware — including inside newlib itself — now uses the FreeRTOS heap-4 pool.

## Architecture

### Components

```
┌────────────────────────────────────────────────────────┐
│  Application code                                      │
│  malloc() / free() / calloc() / realloc()              │
└────────────┬───────────────────────────────────────────┘
             │  linker --wrap
┌────────────▼───────────────────────────────────────────┐
│  Core/Src/memmgr.c                                     │
│  __wrap_malloc()  → pvPortMalloc()                     │
│  __wrap_free()    → vPortFree()                        │
│  __wrap_calloc()  → pvPortCalloc()                     │
│  __wrap_realloc() → pvPortRealloc()                    │
│  (+ reentrant _r variants)                             │
└────────────┬───────────────────────────────────────────┘
             │
┌────────────▼───────────────────────────────────────────┐
│  FreeRTOS heap_4.c                                     │
│  pvPortMalloc / vPortFree / pvPortCalloc / pvPortRealloc│
│  Static pool: ucHeap[configTOTAL_HEAP_SIZE]            │
└────────────────────────────────────────────────────────┘
```

### Linker flags

In `cmake/gcc-arm-none-eabi.cmake`:

```cmake
set(CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} -Wl,--wrap=malloc,--wrap=free,--wrap=calloc,--wrap=realloc")
set(CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} -Wl,--wrap=_malloc_r,--wrap=_free_r,--wrap=_calloc_r,--wrap=_realloc_r")
```

The GNU linker `--wrap=symbol` flag rewrites calls so that:
- References to `malloc` resolve to `__wrap_malloc` (our shim)
- References to `__real_malloc` resolve to the original `malloc` (unused, but
  available if needed)

### `pvPortRealloc`

Added to `Middlewares/FreeRTOS/Source/portable/MemMang/heap_4.c` and declared
in `portable.h` alongside `pvPortMalloc` / `pvPortCalloc`.

Implementation:
- `realloc(ptr, 0)` → frees `ptr`, returns `NULL`
- `realloc(NULL, size)` → delegates to `pvPortMalloc(size)`
- Otherwise: reads the `BlockLink_t` header to recover the original usable
  block size, allocates a new block, copies `min(old_size, new_size)` bytes,
  frees the old block.

> **Coupling note:** `pvPortRealloc` is tightly coupled to the heap-4 internal
> `BlockLink_t` layout.  If the FreeRTOS heap implementation changes (e.g.
> upgrade to a newer FreeRTOS version or switch to heap_5), this function must
> be reviewed and updated.

## Usage Guidelines

### For new code

Use plain `malloc()` / `free()` / `calloc()` / `realloc()`.  They are
equivalent to the `pvPort*` variants but more readable and portable.

### For existing code

Existing `pvPortMalloc` / `vPortFree` call sites **do not need conversion**.
Both spellings resolve to the same FreeRTOS allocator.  Convert only if you are
already touching the code for other reasons and want to improve readability.

### ISR context — do not allocate

`pvPortMalloc` calls `vTaskSuspendAll()` to protect the free-list.  Calling
`malloc` (directly or indirectly via newlib functions like `printf`) from an
interrupt handler will trigger a FreeRTOS assertion or hang.

If you need memory in an ISR, pre-allocate it before entering the ISR and pass
it via a queue or shared pointer.

### Heap sizing

All allocations now share `configTOTAL_HEAP_SIZE` (defined in
`FreeRTOSConfig.h`).  If heap usage grows — for example, when adding a new
module with large buffers — increase this constant.  Monitor free heap at
runtime with `xPortGetFreeHeapSize()` and `xPortGetMinimumEverFreeHeapSize()`.

## Anti-patterns

These are the failure modes introduced by the single-heap redirect.  They must
be checked when implementing any new feature.

### AP-1 · ISR-context allocation (hangs / asserts)

**The problem:** `pvPortMalloc` calls `vTaskSuspendAll()`.  Any `malloc` call
(or any libc call that allocates internally, such as `printf` with certain
format strings) from inside an ISR, DMA callback, or timer callback will hang
or trigger `configASSERT`.

**How to check:** Grep new interrupt handlers (`*_IRQHandler`, HAL callbacks,
timer callbacks, DMA callbacks) for `malloc`, `free`, `printf`, `sprintf`,
`sscanf`, `strtod`, `strtol`.  If any of them — or any function they call — can
reach an allocating function, that is a defect.

**The fix:** Pre-allocate outside the ISR.  Pass data into/out of the ISR via
queues or pre-allocated shared buffers.

**Test coverage:** This pattern cannot be caught by host-side unit tests because
the host does not run real ISR context or `vTaskSuspendAll`.  Audit is the only
reliable guard.

---

### AP-2 · Ignoring NULL return from `malloc` (heap overflow)

**The problem:** The FreeRTOS pool is fixed.  When it is exhausted, `pvPortMalloc`
returns NULL.  Code that dereferences a NULL pointer without checking will hard-fault.

**How to check:** Every `malloc()` / `pvPortMalloc()` call in new code must be
followed immediately by a NULL check.  Either return an error code to the caller
or call `configASSERT(0)` if the allocation is truly required.

**The fix:**
```c
uint8_t *buf = malloc(len);
if (buf == NULL) { return ERROR_OOM; }
```

**Test coverage:** `tests/test_memmgr.c` includes `test_wrap_malloc_oom_returns_null`
which verifies that an oversized allocation returns NULL rather than crashing.
The host sanitizer build (`ENABLE_SANITIZERS=ON`) will catch NULL-dereferences in
any test that exercises an OOM path.

---

### AP-3 · Unbounded allocation growth (pool exhaustion)

**The problem:** Before the heap redirect, large allocations from newlib's `_sbrk`
heap would silently compete with stack.  Now they reduce the FreeRTOS pool, which
affects all tasks, including critical ones.

**How to check:** When adding a new feature that allocates large or persistent
buffers:
1. Estimate the maximum allocation size and add a comment near the call site.
2. Confirm the remaining free heap (`xPortGetMinimumEverFreeHeapSize()`) after
   startup is at least ~8 KB in a debug build.
3. If pool headroom is insufficient, increase `configTOTAL_HEAP_SIZE` in
   `Core/Inc/FreeRTOSConfig.h`.

**Test coverage:** Host-side tests cannot simulate FreeRTOS pool exhaustion
because the host stub maps `pvPortMalloc` to the system `malloc`.  Firmware
integration testing is required.

---

### AP-4 · `pvPortRealloc` coupling to `heap_4` internals

**The problem:** `pvPortRealloc` reads `BlockLink_t` header bytes directly to
recover the original allocation size.  If the FreeRTOS heap implementation
changes (version upgrade, or switch from `heap_4` to `heap_5` for non-contiguous
regions), the header layout may change silently, causing `pvPortRealloc` to
compute the wrong size or pass an invalid pointer to `configASSERT`.

**How to check:** Whenever `Middlewares/FreeRTOS/Source/portable/MemMang/heap_4.c`
is upgraded or replaced, review `pvPortRealloc` immediately afterwards:
- Confirm `xHeapStructSize` calculation still matches.
- Confirm `heapBLOCK_ALLOCATED_BITMASK` and `heapBLOCK_IS_ALLOCATED` macros
  are unchanged.
- Run `tests/test_memmgr.c` to verify the shim still builds and delegates.

**Test coverage:** The host-side tests (`test_wrap_realloc_*`) verify that
`__wrap_realloc` delegates correctly via the stub, but they do not exercise the
real `BlockLink_t` header walking.  Manual review on every FreeRTOS upgrade is
required.

## Testing

Host-side unit tests for the redirect shim are in `tests/test_memmgr.c`.  They
verify:
- `malloc` / `free` round-trip
- `calloc` zero-initialization
- `realloc` grow, shrink, NULL-input, and zero-size edge cases
- Reentrant `_r` variant forwarding

Run with the standard test command:

```bash
cmake -B build-tests -S tests -DCMAKE_BUILD_TYPE=Debug
cmake --build build-tests
ctest --test-dir build-tests --output-on-failure
```

## Files

| File | Purpose |
|------|---------|
| `Core/Inc/memmgr.h` | Public header — declares `__wrap_*` prototypes |
| `Core/Src/memmgr.c` | Shim implementation — wraps libc calls to FreeRTOS |
| `cmake/gcc-arm-none-eabi.cmake` | Linker `--wrap` flags |
| `Middlewares/FreeRTOS/Source/portable/MemMang/heap_4.c` | `pvPortRealloc` implementation |
| `Middlewares/FreeRTOS/Source/include/portable.h` | `pvPortRealloc` / `pvPortCalloc` declarations |
| `tests/test_memmgr.c` | Host-side unit tests |
