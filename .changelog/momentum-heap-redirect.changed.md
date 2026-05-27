**Heap: Momentum-style global malloc→FreeRTOS redirect** — All libc heap
  calls (`malloc`/`free`/`calloc`/`realloc` and newlib reentrant `_r` variants)
  are now transparently routed to the FreeRTOS heap-4 allocator via linker
  `--wrap` flags (`Core/Src/memmgr.c`). Added `pvPortRealloc` to heap_4.
  The newlib `_sbrk` heap (`Core/Src/sysmem.c`) is removed from the build.
  Explicit `pvPortMalloc`/`vPortFree` call-site conversions are no longer
  required — plain `malloc()`/`free()` now uses the FreeRTOS heap everywhere.
