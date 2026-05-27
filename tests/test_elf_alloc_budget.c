/* Unit tests for ELF loader cumulative allocation budget. */

#include "unity.h"
#include "m1_elf_loader.h"

void setUp(void) {}
void tearDown(void) {}

/* ------------------------------------------------------------------ */
/* Budget constant sanity                                             */
/* ------------------------------------------------------------------ */

void test_elf_alloc_budget_is_96kb(void)
{
    TEST_ASSERT_EQUAL_UINT32(96U * 1024U, ELF_ALLOC_BUDGET);
}

/* ------------------------------------------------------------------ */
/* ELF_ERR_BUDGET enum exists and differs from ELF_ERR_NO_MEMORY      */
/* ------------------------------------------------------------------ */

void test_elf_err_budget_is_distinct(void)
{
    TEST_ASSERT_NOT_EQUAL((int)ELF_ERR_NO_MEMORY, (int)ELF_ERR_BUDGET);
    TEST_ASSERT_NOT_EQUAL((int)ELF_OK, (int)ELF_ERR_BUDGET);
}

/* ------------------------------------------------------------------ */
/* Budget value leaves room for ESP32/STM32 flashing                  */
/* ------------------------------------------------------------------ */

void test_budget_leaves_headroom_for_flashing(void)
{
    /* configTOTAL_HEAP_SIZE is 262144 (256 KB).
     * ESP32 flash operations use ~1 KB chunk buffers on the stack plus
     * small malloc'd paths.  We need at least 64 KB headroom after an
     * ELF load for safe flashing and system operation. */
    const uint32_t total_heap = 262144U;
    const uint32_t min_headroom = 64U * 1024U;

    TEST_ASSERT_TRUE_MESSAGE(
        ELF_ALLOC_BUDGET + min_headroom <= total_heap,
        "ELF_ALLOC_BUDGET too large — insufficient headroom for flashing");
}

/* ------------------------------------------------------------------ */
/* Entry point                                                        */
/* ------------------------------------------------------------------ */

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_elf_alloc_budget_is_96kb);
    RUN_TEST(test_elf_err_budget_is_distinct);
    RUN_TEST(test_budget_leaves_headroom_for_flashing);
    return UNITY_END();
}
