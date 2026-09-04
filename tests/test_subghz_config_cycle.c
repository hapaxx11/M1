/* See COPYING.txt for license details. */

/*
 * test_subghz_config_cycle.c
 *
 * Regression tests for subghz_cfg_cycle_next_allowed(), the pure
 * mask-based cycling/wraparound helper used by the Sub-GHz Config scene
 * for both the Frequency and Modulation controls.
 *
 * Bug context: cfg_next_allowed() used to short-circuit and return the
 * unchanged index whenever the active filter mode was
 * SubGhzConfigFilterNone, so Read Raw's supposedly unrestricted
 * Frequency/Modulation controls could not cycle at all. The fix removes
 * that special case; unfiltered cycling now simply relies on an
 * all-bits-set mask, exercised here directly.
 */

#include "unity.h"
#include "subghz_config_cycle.h"

void setUp(void) {}
void tearDown(void) {}

/* ================================================================
 * Unfiltered cycling (mask == all bits set, e.g. SubGhzConfigFilterNone)
 * ================================================================ */

void test_unfiltered_cycling_advances_forward(void)
{
    uint8_t idx = subghz_cfg_cycle_next_allowed(0, +1, 4, UINT64_MAX);
    TEST_ASSERT_EQUAL_UINT8(1, idx);
}

void test_unfiltered_cycling_advances_backward(void)
{
    uint8_t idx = subghz_cfg_cycle_next_allowed(1, -1, 4, UINT64_MAX);
    TEST_ASSERT_EQUAL_UINT8(0, idx);
}

void test_unfiltered_cycling_wraps_forward_past_last(void)
{
    uint8_t idx = subghz_cfg_cycle_next_allowed(3, +1, 4, UINT64_MAX);
    TEST_ASSERT_EQUAL_UINT8(0, idx);
}

void test_unfiltered_cycling_wraps_backward_past_first(void)
{
    uint8_t idx = subghz_cfg_cycle_next_allowed(0, -1, 4, UINT64_MAX);
    TEST_ASSERT_EQUAL_UINT8(3, idx);
}

/* ================================================================
 * Modulation-style masks (4 entries, both directions)
 * ================================================================ */

void test_modulation_mask_skips_disallowed_forward(void)
{
    /* Only AM650 (bit 1) and FM476 (bit 3) allowed. */
    uint64_t mask = (1u << 1) | (1u << 3);
    uint8_t idx = subghz_cfg_cycle_next_allowed(1, +1, 4, mask);
    TEST_ASSERT_EQUAL_UINT8(3, idx);
}

void test_modulation_mask_skips_disallowed_backward(void)
{
    uint64_t mask = (1u << 1) | (1u << 3);
    uint8_t idx = subghz_cfg_cycle_next_allowed(3, -1, 4, mask);
    TEST_ASSERT_EQUAL_UINT8(1, idx);
}

void test_modulation_mask_wraps_around_when_only_one_allowed(void)
{
    /* Only FM238 (bit 2) allowed — cycling from it in either direction
     * must land back on itself. */
    uint64_t mask = (1u << 2);
    TEST_ASSERT_EQUAL_UINT8(2, subghz_cfg_cycle_next_allowed(2, +1, 4, mask));
    TEST_ASSERT_EQUAL_UINT8(2, subghz_cfg_cycle_next_allowed(2, -1, 4, mask));
}

/* ================================================================
 * Frequency-style masks (64 entries, high bit near SUBGHZ_FREQ_PRESET_CUSTOM)
 * ================================================================ */

void test_frequency_mask_finds_next_allowed_high_bit(void)
{
    /* Only index 40 (default 433.92 MHz) and 63 (Custom) allowed. */
    uint64_t mask = (UINT64_C(1) << 40) | (UINT64_C(1) << 63);
    uint8_t idx = subghz_cfg_cycle_next_allowed(0, +1, 64, mask);
    TEST_ASSERT_EQUAL_UINT8(40, idx);
}

void test_frequency_mask_bit_63_is_not_truncated(void)
{
    /* Regression: a uint32_t mask would truncate/undefined-shift bit 63
     * (SUBGHZ_FREQ_PRESET_CUSTOM). Verify it is reachable and correctly
     * detected with a uint64_t mask. */
    uint64_t mask = (UINT64_C(1) << 63);
    uint8_t idx = subghz_cfg_cycle_next_allowed(62, +1, 64, mask);
    TEST_ASSERT_EQUAL_UINT8(63, idx);
}

/* ================================================================
 * Edge cases
 * ================================================================ */

void test_zero_count_returns_index_unchanged(void)
{
    TEST_ASSERT_EQUAL_UINT8(5, subghz_cfg_cycle_next_allowed(5, +1, 0, UINT64_MAX));
}

void test_empty_mask_returns_index_unchanged(void)
{
    TEST_ASSERT_EQUAL_UINT8(2, subghz_cfg_cycle_next_allowed(2, +1, 4, 0));
}

/* ================================================================
 * Main
 * ================================================================ */
int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_unfiltered_cycling_advances_forward);
    RUN_TEST(test_unfiltered_cycling_advances_backward);
    RUN_TEST(test_unfiltered_cycling_wraps_forward_past_last);
    RUN_TEST(test_unfiltered_cycling_wraps_backward_past_first);
    RUN_TEST(test_modulation_mask_skips_disallowed_forward);
    RUN_TEST(test_modulation_mask_skips_disallowed_backward);
    RUN_TEST(test_modulation_mask_wraps_around_when_only_one_allowed);
    RUN_TEST(test_frequency_mask_finds_next_allowed_high_bit);
    RUN_TEST(test_frequency_mask_bit_63_is_not_truncated);
    RUN_TEST(test_zero_count_returns_index_unchanged);
    RUN_TEST(test_empty_mask_returns_index_unchanged);

    return UNITY_END();
}
