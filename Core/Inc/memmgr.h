/* See COPYING.txt for license details. */

/**
 * @file  memmgr.h
 * @brief Momentum-style global heap redirect.
 *
 * When this module is linked with the --wrap flags listed in
 * cmake/gcc-arm-none-eabi.cmake, every call to malloc / free / calloc /
 * realloc (and the newlib-nano reentrant _r variants) is transparently
 * redirected to the FreeRTOS heap-4 allocator (pvPortMalloc / vPortFree).
 *
 * This eliminates the need for per-call-site pvPortMalloc conversions and
 * makes the newlib _sbrk implementation (Core/Src/sysmem.c) unnecessary.
 */

#ifndef MEMMGR_H
#define MEMMGR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

/*
 * These are the --wrap entry points provided by memmgr.c.
 * Application code should continue to call plain malloc() / free() etc.;
 * the linker --wrap option redirects those calls here automatically.
 */
void *__wrap_malloc(size_t size);
void  __wrap_free(void *ptr);
void *__wrap_calloc(size_t num, size_t size);
void *__wrap_realloc(void *ptr, size_t size);

/* Newlib-nano reentrant variants */
struct _reent; /* forward declaration */
void *__wrap__malloc_r(struct _reent *r, size_t size);
void  __wrap__free_r(struct _reent *r, void *ptr);
void *__wrap__calloc_r(struct _reent *r, size_t num, size_t size);
void *__wrap__realloc_r(struct _reent *r, void *ptr, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* MEMMGR_H */
