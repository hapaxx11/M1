#include "unity.h"
#include "m1_esp32_progress.h"

void setUp(void) {}
void tearDown(void) {}

void test_progress_start_is_zero(void)   { TEST_ASSERT_EQUAL_UINT8(0,   esp32_update_progress_percent(1000, 1000)); }
void test_progress_done_is_100(void)     { TEST_ASSERT_EQUAL_UINT8(100, esp32_update_progress_percent(1000, 0)); }
void test_progress_half(void)            { TEST_ASSERT_EQUAL_UINT8(50,  esp32_update_progress_percent(1000, 500)); }
void test_progress_three_quarter(void)   { TEST_ASSERT_EQUAL_UINT8(75,  esp32_update_progress_percent(1000, 250)); }
void test_progress_total_zero_guarded(void) { TEST_ASSERT_EQUAL_UINT8(0, esp32_update_progress_percent(0, 0)); }
void test_progress_remainder_gt_total_clamped(void) { TEST_ASSERT_EQUAL_UINT8(0, esp32_update_progress_percent(1000, 2000)); }
void test_progress_real_image_half(void) { TEST_ASSERT_EQUAL_UINT8(50, esp32_update_progress_percent(1404624, 702312)); }

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_progress_start_is_zero);
    RUN_TEST(test_progress_done_is_100);
    RUN_TEST(test_progress_half);
    RUN_TEST(test_progress_three_quarter);
    RUN_TEST(test_progress_total_zero_guarded);
    RUN_TEST(test_progress_remainder_gt_total_clamped);
    RUN_TEST(test_progress_real_image_half);
    return UNITY_END();
}
