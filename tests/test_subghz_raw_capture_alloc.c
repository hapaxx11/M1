/* See COPYING.txt for license details. */

/*
 * test_subghz_raw_capture_alloc.c
 *
 * Unit tests for subghz_raw_capture_alloc — the pure-logic heap reserve probe
 * used by Sub-GHz Read Raw capture startup before starting the SD writer.
 *
 * Tests verify:
 *   - Successful reserve probes free the temporary allocation
 *   - Failed reserve allocation is reported without calling free
 *   - Missing callbacks are rejected
 *   - Zero-byte probes do not require callbacks or allocation
 */

#include "unity.h"
#include "subghz_raw_capture_alloc.h"

static size_t requested_bytes;
static unsigned alloc_calls;
static unsigned free_calls;
static unsigned char reserve_block;
static bool allocator_should_fail;

static void *test_alloc(size_t size)
{
    alloc_calls++;
    requested_bytes = size;
    return allocator_should_fail ? NULL : &reserve_block;
}

static void test_free(void *ptr)
{
    TEST_ASSERT_EQUAL_PTR(&reserve_block, ptr);
    free_calls++;
}

void setUp(void)
{
    requested_bytes = 0;
    alloc_calls = 0;
    free_calls = 0;
    allocator_should_fail = false;
}

void tearDown(void)
{
}

void test_reserve_heap_succeeds_and_releases_probe_allocation(void)
{
    TEST_ASSERT_TRUE(subghz_raw_capture_reserve_heap(28672U, test_alloc, test_free));
    TEST_ASSERT_EQUAL_UINT(1, alloc_calls);
    TEST_ASSERT_EQUAL_UINT(1, free_calls);
    TEST_ASSERT_EQUAL_size_t(28672U, requested_bytes);
}

void test_reserve_heap_fails_when_probe_allocation_fails(void)
{
    allocator_should_fail = true;

    TEST_ASSERT_FALSE(subghz_raw_capture_reserve_heap(28672U, test_alloc, test_free));
    TEST_ASSERT_EQUAL_UINT(1, alloc_calls);
    TEST_ASSERT_EQUAL_UINT(0, free_calls);
}

void test_reserve_heap_rejects_missing_callbacks(void)
{
    TEST_ASSERT_FALSE(subghz_raw_capture_reserve_heap(28672U, NULL, test_free));
    TEST_ASSERT_FALSE(subghz_raw_capture_reserve_heap(28672U, test_alloc, NULL));
    TEST_ASSERT_EQUAL_UINT(0, alloc_calls);
    TEST_ASSERT_EQUAL_UINT(0, free_calls);
}

void test_reserve_heap_zero_bytes_needs_no_allocation(void)
{
    TEST_ASSERT_TRUE(subghz_raw_capture_reserve_heap(0U, NULL, NULL));
    TEST_ASSERT_EQUAL_UINT(0, alloc_calls);
    TEST_ASSERT_EQUAL_UINT(0, free_calls);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_reserve_heap_succeeds_and_releases_probe_allocation);
    RUN_TEST(test_reserve_heap_fails_when_probe_allocation_fails);
    RUN_TEST(test_reserve_heap_rejects_missing_callbacks);
    RUN_TEST(test_reserve_heap_zero_bytes_needs_no_allocation);
    return UNITY_END();
}
