/*
 * test_t5577_timing.c
 *
 * Regression test for the T5577 write-timing bug fix (da-pingwing,
 * github.com/da-pingwing/M1_T-1000_RFID, GPL-3.0).
 *
 * The previous values (DATA_1=56, WRITE_GAP=18) put the "1" bit
 * gap-to-gap at 74 Tc, out of the T5577 datasheet maximum of 64 Tc.
 * The chip garbled the bit and clones read back as a consistent but
 * wrong value.
 *
 * These tests verify that the timing constants meet the T5577 spec:
 *   Write gap:  8..20 Tc
 *   "0" bit gap-to-gap: 16..32 Tc
 *   "1" bit gap-to-gap: 48..64 Tc
 *
 * The same constraints are also enforced as compile-time #error checks
 * in t5577_timing.h so out-of-spec constants will fail the build.
 */

#include "unity.h"
#include "t5577_timing.h"

void setUp(void) {}
void tearDown(void) {}

/* --- write gap spec [8..20 Tc] ------------------------------------------ */

void test_write_gap_at_least_8_tc(void)
{
    TEST_ASSERT_GREATER_OR_EQUAL_INT(8, T5577_TIMING_WRITE_GAP);
}

void test_write_gap_at_most_20_tc(void)
{
    TEST_ASSERT_LESS_OR_EQUAL_INT(20, T5577_TIMING_WRITE_GAP);
}

/* --- "0" bit gap-to-gap [16..32 Tc] --------------------------------------- */

void test_bit0_gap_to_gap_at_least_16_tc(void)
{
    TEST_ASSERT_GREATER_OR_EQUAL_INT(16, T5577_TIMING_DATA_0 + T5577_TIMING_WRITE_GAP);
}

void test_bit0_gap_to_gap_at_most_32_tc(void)
{
    TEST_ASSERT_LESS_OR_EQUAL_INT(32, T5577_TIMING_DATA_0 + T5577_TIMING_WRITE_GAP);
}

/* --- "1" bit gap-to-gap [48..64 Tc] --------------------------------------- */

void test_bit1_gap_to_gap_at_least_48_tc(void)
{
    TEST_ASSERT_GREATER_OR_EQUAL_INT(48, T5577_TIMING_DATA_1 + T5577_TIMING_WRITE_GAP);
}

void test_bit1_gap_to_gap_at_most_64_tc(void)
{
    /* This is the exact constraint the bug violated: old values gave 74 Tc. */
    TEST_ASSERT_LESS_OR_EQUAL_INT(64, T5577_TIMING_DATA_1 + T5577_TIMING_WRITE_GAP);
}

/* --- typical-value spot checks -------------------------------------------- */

void test_bit0_gap_to_gap_typical_24_tc(void)
{
    TEST_ASSERT_EQUAL_INT(24, T5577_TIMING_DATA_0 + T5577_TIMING_WRITE_GAP);
}

void test_bit1_gap_to_gap_typical_56_tc(void)
{
    TEST_ASSERT_EQUAL_INT(56, T5577_TIMING_DATA_1 + T5577_TIMING_WRITE_GAP);
}

void test_write_gap_typical_10_tc(void)
{
    TEST_ASSERT_EQUAL_INT(10, T5577_TIMING_WRITE_GAP);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_write_gap_at_least_8_tc);
    RUN_TEST(test_write_gap_at_most_20_tc);
    RUN_TEST(test_bit0_gap_to_gap_at_least_16_tc);
    RUN_TEST(test_bit0_gap_to_gap_at_most_32_tc);
    RUN_TEST(test_bit1_gap_to_gap_at_least_48_tc);
    RUN_TEST(test_bit1_gap_to_gap_at_most_64_tc);
    RUN_TEST(test_bit0_gap_to_gap_typical_24_tc);
    RUN_TEST(test_bit1_gap_to_gap_typical_56_tc);
    RUN_TEST(test_write_gap_typical_10_tc);
    return UNITY_END();
}
