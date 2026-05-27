/* See COPYING.txt for license details. */

/*
 * test_memmgr.c
 *
 * Unit tests for the Momentum-style heap redirect (Core/Src/memmgr.c).
 *
 * On the host, pvPortMalloc/vPortFree/pvPortCalloc/pvPortRealloc are
 * #defined to libc in the FreeRTOS stub, so the __wrap_ functions
 * exercise the delegation path with standard libc as the backend.
 *
 * Tests verify:
 *   - __wrap_malloc / __wrap_free basic allocation round-trip
 *   - __wrap_malloc(SIZE_MAX) returns NULL (AP-2: OOM graceful handling)
 *   - __wrap_calloc zero-initializes memory
 *   - __wrap_realloc grows, shrinks (AP-4: prefix preserved), and handles NULL / 0 edge cases
 *   - Reentrant _r variants delegate identically
 */

#include "unity.h"
#include "memmgr.h"
#include <string.h>

void setUp(void)  { }
void tearDown(void) { }

/* ------------------------------------------------------------------ */
/*  malloc / free                                                     */
/* ------------------------------------------------------------------ */

void test_wrap_malloc_returns_nonnull(void)
{
    void *p = __wrap_malloc(64);
    TEST_ASSERT_NOT_NULL(p);
    __wrap_free(p);
}

void test_wrap_malloc_memory_is_writable(void)
{
    uint8_t *p = __wrap_malloc(16);
    TEST_ASSERT_NOT_NULL(p);
    memset(p, 0xAA, 16);
    TEST_ASSERT_EQUAL_UINT8(0xAA, p[0]);
    TEST_ASSERT_EQUAL_UINT8(0xAA, p[15]);
    __wrap_free(p);
}

void test_wrap_free_null_is_safe(void)
{
    /* Must not crash */
    __wrap_free(NULL);
}

/* ------------------------------------------------------------------ */
/*  calloc                                                            */
/* ------------------------------------------------------------------ */

void test_wrap_calloc_zeroes_memory(void)
{
    uint8_t *p = __wrap_calloc(4, 8);
    TEST_ASSERT_NOT_NULL(p);
    for (int i = 0; i < 32; i++)
    {
        TEST_ASSERT_EQUAL_UINT8(0, p[i]);
    }
    __wrap_free(p);
}

/* ------------------------------------------------------------------ */
/*  realloc                                                           */
/* ------------------------------------------------------------------ */

void test_wrap_realloc_null_acts_as_malloc(void)
{
    void *p = __wrap_realloc(NULL, 32);
    TEST_ASSERT_NOT_NULL(p);
    __wrap_free(p);
}

void test_wrap_realloc_zero_acts_as_free(void)
{
    void *p = __wrap_malloc(32);
    TEST_ASSERT_NOT_NULL(p);
    void *q = __wrap_realloc(p, 0);
    /* Standard allows either NULL or a freeable non-NULL pointer */
    if (q != NULL)
    {
        __wrap_free(q);
    }
}

void test_wrap_realloc_preserves_content(void)
{
    uint8_t *p = __wrap_malloc(8);
    TEST_ASSERT_NOT_NULL(p);
    memcpy(p, "ABCDEFGH", 8);

    uint8_t *q = __wrap_realloc(p, 64);
    TEST_ASSERT_NOT_NULL(q);
    TEST_ASSERT_EQUAL_MEMORY("ABCDEFGH", q, 8);
    __wrap_free(q);
}

void test_wrap_realloc_shrink_preserves_prefix(void)
{
    /* Allocate 64 bytes, write a pattern, shrink to 8, verify first 8 bytes
     * survive.  Exercises AP-4: realloc must copy min(old, new) bytes. */
    uint8_t *p = __wrap_malloc(64);
    TEST_ASSERT_NOT_NULL(p);
    memset(p, 0x5A, 64);

    uint8_t *q = __wrap_realloc(p, 8);
    TEST_ASSERT_NOT_NULL(q);
    for (int i = 0; i < 8; i++)
    {
        TEST_ASSERT_EQUAL_UINT8(0x5A, q[i]);
    }
    __wrap_free(q);
}

/* AP-2: oversized allocation must return NULL, not crash/hang.
 * On the firmware the FreeRTOS pool is fixed; here the host libc backs the
 * stub, so SIZE_MAX will reliably fail and return NULL on any POSIX system. */
void test_wrap_malloc_oom_returns_null(void)
{
    void *p = __wrap_malloc((size_t)-1);  /* SIZE_MAX — will always fail */
    TEST_ASSERT_NULL(p);
    /* No free needed — a NULL pointer must never be passed to free */
}

/* ------------------------------------------------------------------ */
/*  Reentrant _r variants                                             */
/* ------------------------------------------------------------------ */

void test_wrap_malloc_r_returns_nonnull(void)
{
    void *p = __wrap__malloc_r(NULL, 64);
    TEST_ASSERT_NOT_NULL(p);
    __wrap__free_r(NULL, p);
}

void test_wrap_calloc_r_zeroes_memory(void)
{
    uint8_t *p = __wrap__calloc_r(NULL, 2, 16);
    TEST_ASSERT_NOT_NULL(p);
    for (int i = 0; i < 32; i++)
    {
        TEST_ASSERT_EQUAL_UINT8(0, p[i]);
    }
    __wrap__free_r(NULL, p);
}

void test_wrap_realloc_r_preserves_content(void)
{
    uint8_t *p = __wrap__malloc_r(NULL, 8);
    TEST_ASSERT_NOT_NULL(p);
    memcpy(p, "12345678", 8);

    uint8_t *q = __wrap__realloc_r(NULL, p, 64);
    TEST_ASSERT_NOT_NULL(q);
    TEST_ASSERT_EQUAL_MEMORY("12345678", q, 8);
    __wrap__free_r(NULL, q);
}

/* ------------------------------------------------------------------ */
/*  Runner                                                            */
/* ------------------------------------------------------------------ */

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_wrap_malloc_returns_nonnull);
    RUN_TEST(test_wrap_malloc_memory_is_writable);
    RUN_TEST(test_wrap_free_null_is_safe);
    RUN_TEST(test_wrap_calloc_zeroes_memory);
    RUN_TEST(test_wrap_realloc_null_acts_as_malloc);
    RUN_TEST(test_wrap_realloc_zero_acts_as_free);
    RUN_TEST(test_wrap_realloc_preserves_content);
    RUN_TEST(test_wrap_realloc_shrink_preserves_prefix);
    RUN_TEST(test_wrap_malloc_oom_returns_null);
    RUN_TEST(test_wrap_malloc_r_returns_nonnull);
    RUN_TEST(test_wrap_calloc_r_zeroes_memory);
    RUN_TEST(test_wrap_realloc_r_preserves_content);
    return UNITY_END();
}
