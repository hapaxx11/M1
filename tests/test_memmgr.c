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
 *   - __wrap_calloc zero-initializes memory
 *   - __wrap_realloc grows, shrinks, and handles NULL / 0 edge cases
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
    /* Standard says returning NULL on size==0 is valid */
    (void)q;
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
    RUN_TEST(test_wrap_malloc_r_returns_nonnull);
    RUN_TEST(test_wrap_calloc_r_zeroes_memory);
    RUN_TEST(test_wrap_realloc_r_preserves_content);
    return UNITY_END();
}
