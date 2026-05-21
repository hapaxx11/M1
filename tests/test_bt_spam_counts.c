/* See COPYING.txt for license details. */

#include "unity.h"
#include "m1_bt.h"

void setUp(void)
{
}

void tearDown(void)
{
}

void test_google_fastpair_payload_count_is_six(void)
{
    TEST_ASSERT_EQUAL_UINT8(6U, ble_spam_payload_count(4U));
}

void test_other_ble_spam_variants_remain_four(void)
{
    TEST_ASSERT_EQUAL_UINT8(4U, ble_spam_payload_count(0U));
    TEST_ASSERT_EQUAL_UINT8(4U, ble_spam_payload_count(1U));
    TEST_ASSERT_EQUAL_UINT8(4U, ble_spam_payload_count(2U));
    TEST_ASSERT_EQUAL_UINT8(4U, ble_spam_payload_count(3U));
    TEST_ASSERT_EQUAL_UINT8(4U, ble_spam_payload_count(5U));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_google_fastpair_payload_count_is_six);
    RUN_TEST(test_other_ble_spam_variants_remain_four);
    return UNITY_END();
}
