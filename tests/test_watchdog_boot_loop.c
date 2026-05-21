#include "unity.h"
#include "m1_watchdog_boot_loop.h"

void setUp(void) {}
void tearDown(void) {}

void test_stale_signature_resets_in_range_counter_before_increment(void)
{
    const uint32_t expected_sig = m1_wdt_boot_loop_build_sig_components(0u, 9u, 1u, 0u, 0u);
    const m1_wdt_boot_loop_eval_t eval = m1_wdt_boot_loop_evaluate(
        2u,                  /* stale in-range value */
        expected_sig ^ 1u,   /* invalid signature */
        true,                /* current reset is from WDT */
        expected_sig,
        3u);

    TEST_ASSERT_EQUAL_UINT32(1u, eval.runtime_ctr);
    TEST_ASSERT_EQUAL_UINT32(1u, eval.stored_ctr);
    TEST_ASSERT_FALSE(eval.disable_iwdg);
}

void test_valid_signature_hits_boot_loop_threshold(void)
{
    const uint32_t expected_sig = m1_wdt_boot_loop_build_sig_components(0u, 9u, 1u, 0u, 0u);
    const m1_wdt_boot_loop_eval_t eval = m1_wdt_boot_loop_evaluate(
        2u,
        expected_sig,
        true,
        expected_sig,
        3u);

    TEST_ASSERT_EQUAL_UINT32(3u, eval.runtime_ctr);
    TEST_ASSERT_EQUAL_UINT32(0u, eval.stored_ctr);
    TEST_ASSERT_TRUE(eval.disable_iwdg);
}

void test_non_wdt_reset_clears_counter_even_when_signature_matches(void)
{
    const uint32_t expected_sig = m1_wdt_boot_loop_build_sig_components(0u, 9u, 1u, 0u, 0u);
    const m1_wdt_boot_loop_eval_t eval = m1_wdt_boot_loop_evaluate(
        2u,
        expected_sig,
        false,
        expected_sig,
        3u);

    TEST_ASSERT_EQUAL_UINT32(0u, eval.runtime_ctr);
    TEST_ASSERT_EQUAL_UINT32(0u, eval.stored_ctr);
    TEST_ASSERT_FALSE(eval.disable_iwdg);
}

void test_out_of_range_counter_is_clamped_before_increment(void)
{
    const uint32_t expected_sig = m1_wdt_boot_loop_build_sig_components(0u, 9u, 1u, 0u, 0u);
    const m1_wdt_boot_loop_eval_t eval = m1_wdt_boot_loop_evaluate(
        100u,
        expected_sig,
        true,
        expected_sig,
        3u);

    TEST_ASSERT_EQUAL_UINT32(1u, eval.runtime_ctr);
    TEST_ASSERT_EQUAL_UINT32(1u, eval.stored_ctr);
    TEST_ASSERT_FALSE(eval.disable_iwdg);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_stale_signature_resets_in_range_counter_before_increment);
    RUN_TEST(test_valid_signature_hits_boot_loop_threshold);
    RUN_TEST(test_non_wdt_reset_clears_counter_even_when_signature_matches);
    RUN_TEST(test_out_of_range_counter_is_clamped_before_increment);
    return UNITY_END();
}
