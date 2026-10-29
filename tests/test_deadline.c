#include "unity.h"
#include "m1_deadline.h"

void setUp(void) {}
void tearDown(void) {}

void test_deadline_returns_only_unexpired_budget(void)
{
    TEST_ASSERT_EQUAL_UINT32(800u, m1_deadline_remaining_ms(2000u, 1200u));
    TEST_ASSERT_EQUAL_UINT32(0u, m1_deadline_remaining_ms(2000u, 2000u));
    TEST_ASSERT_EQUAL_UINT32(0u, m1_deadline_remaining_ms(2000u, 2500u));
}

void test_deadline_rounds_response_seconds_up_without_exceeding_budget(void)
{
    TEST_ASSERT_EQUAL_INT(2, m1_deadline_remaining_seconds(2000u, 500u));
    TEST_ASSERT_EQUAL_INT(1, m1_deadline_remaining_seconds(2000u, 1200u));
    TEST_ASSERT_EQUAL_INT(1, m1_deadline_remaining_seconds(2000u, 1999u));
    TEST_ASSERT_EQUAL_INT(0, m1_deadline_remaining_seconds(2000u, 2000u));
}

void test_deadline_handles_tick_wraparound(void)
{
    TEST_ASSERT_EQUAL_UINT32(50u,
        m1_deadline_remaining_ms(25u, UINT32_MAX - 24u));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_deadline_returns_only_unexpired_budget);
    RUN_TEST(test_deadline_rounds_response_seconds_up_without_exceeding_budget);
    RUN_TEST(test_deadline_handles_tick_wraparound);
    return UNITY_END();
}
