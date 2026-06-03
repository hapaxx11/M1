#include "unity.h"
#include "m1_esp32_scratch.h"
#include <stdlib.h>

void setUp(void) {}
void tearDown(void) {}

static int alloc_count;
static void *counting_alloc(size_t n) { alloc_count++; return malloc(n); }
static void *failing_alloc(size_t n)  { (void)n; alloc_count++; return NULL; }

/* REGRESSION: the SD-card ESP32 image-select path freed these buffers (via the
 * file-browser scene exit) and then used them -> NULL deref / HardFault. The
 * guard must re-allocate any NULL buffer and only report success when both are
 * valid. */
void test_ensure_scratch_reallocates_when_freed(void)
{
    char *full = NULL, *name = NULL; alloc_count = 0;
    TEST_ASSERT_TRUE(esp32_fw_ensure_scratch(&full, &name, 128, 32, counting_alloc));
    TEST_ASSERT_NOT_NULL(full);
    TEST_ASSERT_NOT_NULL(name);
    TEST_ASSERT_EQUAL_INT(2, alloc_count);   /* both were NULL -> both allocated */
    free(full); free(name);
}

void test_ensure_scratch_no_realloc_when_present(void)
{
    char *full = malloc(8), *name = malloc(8);
    char *full0 = full, *name0 = name; alloc_count = 0;
    TEST_ASSERT_TRUE(esp32_fw_ensure_scratch(&full, &name, 128, 32, counting_alloc));
    TEST_ASSERT_EQUAL_INT(0, alloc_count);   /* already valid -> no re-alloc */
    TEST_ASSERT_EQUAL_PTR(full0, full);
    TEST_ASSERT_EQUAL_PTR(name0, name);
    free(full); free(name);
}

void test_ensure_scratch_reports_failure_on_oom(void)
{
    char *full = NULL, *name = NULL; alloc_count = 0;
    /* Heap exhausted -> must return false so the caller bails instead of
     * dereferencing NULL. */
    TEST_ASSERT_FALSE(esp32_fw_ensure_scratch(&full, &name, 128, 32, failing_alloc));
}

void test_ensure_scratch_partial_present(void)
{
    char *full = malloc(8), *name = NULL; alloc_count = 0;
    TEST_ASSERT_TRUE(esp32_fw_ensure_scratch(&full, &name, 128, 32, counting_alloc));
    TEST_ASSERT_EQUAL_INT(1, alloc_count);   /* only the NULL one allocated */
    TEST_ASSERT_NOT_NULL(name);
    free(full); free(name);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_ensure_scratch_reallocates_when_freed);
    RUN_TEST(test_ensure_scratch_no_realloc_when_present);
    RUN_TEST(test_ensure_scratch_reports_failure_on_oom);
    RUN_TEST(test_ensure_scratch_partial_present);
    return UNITY_END();
}
