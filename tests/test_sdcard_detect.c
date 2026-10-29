#include "unity.h"
#include "m1_sdcard_detect.h"

void setUp(void) {}
void tearDown(void) {}

void test_early_irq_before_queue_creation_does_not_post(void)
{
    TEST_ASSERT_FALSE(m1_sdcard_detect_should_post(false, 0u));
}

void test_irq_after_queue_creation_posts_first_transition(void)
{
    TEST_ASSERT_TRUE(m1_sdcard_detect_should_post(true, 0u));
}

void test_duplicate_transition_does_not_post(void)
{
    TEST_ASSERT_FALSE(m1_sdcard_detect_should_post(true, 1u));
}

void test_no_queue_and_duplicate_transition_stays_idle(void)
{
    TEST_ASSERT_FALSE(m1_sdcard_detect_should_post(false, 1u));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_early_irq_before_queue_creation_does_not_post);
    RUN_TEST(test_irq_after_queue_creation_posts_first_transition);
    RUN_TEST(test_duplicate_transition_does_not_post);
    RUN_TEST(test_no_queue_and_duplicate_transition_stays_idle);
    return UNITY_END();
}
