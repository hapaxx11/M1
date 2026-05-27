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
