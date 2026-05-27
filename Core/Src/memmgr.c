/* See COPYING.txt for license details. */

/**
 * @file  memmgr.c
 * @brief Momentum-style global heap redirect — routes all libc heap calls
 *        through the FreeRTOS heap-4 allocator.
 *
 * This file provides __wrap_ implementations for malloc, free, calloc,
 * realloc, and their newlib reentrant (_r) variants.  The corresponding
 * --wrap linker flags in cmake/gcc-arm-none-eabi.cmake cause the linker to
 * resolve every call to malloc() → __wrap_malloc(), etc.
 *
 * With this shim in place the newlib _sbrk heap (Core/Src/sysmem.c) is no
 * longer used and can be removed from the build.
 */

#include "memmgr.h"

#include "FreeRTOS.h"
#include "portable.h"

/* ------------------------------------------------------------------ */
/*  Standard libc wrappers                                            */
/* ------------------------------------------------------------------ */

void *__wrap_malloc(size_t size)
{
    return pvPortMalloc(size);
}

void __wrap_free(void *ptr)
{
    vPortFree(ptr);
}

void *__wrap_calloc(size_t num, size_t size)
{
    return pvPortCalloc(num, size);
}

void *__wrap_realloc(void *ptr, size_t size)
{
    return pvPortRealloc(ptr, size);
}

/* ------------------------------------------------------------------ */
/*  Newlib-nano reentrant (_r) wrappers                               */
/* ------------------------------------------------------------------ */

void *__wrap__malloc_r(struct _reent *r, size_t size)
{
    (void)r;
    return pvPortMalloc(size);
}

void __wrap__free_r(struct _reent *r, void *ptr)
{
    (void)r;
    vPortFree(ptr);
}

void *__wrap__calloc_r(struct _reent *r, size_t num, size_t size)
{
    (void)r;
    return pvPortCalloc(num, size);
}

void *__wrap__realloc_r(struct _reent *r, void *ptr, size_t size)
{
    (void)r;
    return pvPortRealloc(ptr, size);
}
