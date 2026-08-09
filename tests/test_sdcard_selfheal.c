#include "unity.h"
#include "m1_sdcard_selfheal.h"

void setUp(void) {}
void tearDown(void) {}

void test_present_notready_empty_queue_self_heals(void)
{
    TEST_ASSERT_TRUE(
        m1_sdcard_should_self_heal(true, SD_access_NotReady, 0));
}

void test_present_notready_but_event_already_queued_does_not_pile_up(void)
{
    TEST_ASSERT_FALSE(
        m1_sdcard_should_self_heal(true, SD_access_NotReady, 1));
}

void test_card_removed_is_left_alone(void)
{
    TEST_ASSERT_FALSE(
        m1_sdcard_should_self_heal(false, SD_access_NotReady, 0));
}

void test_present_and_ok_does_not_self_heal(void)
{
    TEST_ASSERT_FALSE(
        m1_sdcard_should_self_heal(true, SD_access_OK, 0));
}

void test_present_and_unmounted_does_not_self_heal(void)
{
    /* SD_access_UnMounted is the intentional USB-storage-mode state; the
     * self-heal path must never disturb it. */
    TEST_ASSERT_FALSE(
        m1_sdcard_should_self_heal(true, SD_access_UnMounted, 0));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_present_notready_empty_queue_self_heals);
    RUN_TEST(test_present_notready_but_event_already_queued_does_not_pile_up);
    RUN_TEST(test_card_removed_is_left_alone);
    RUN_TEST(test_present_and_ok_does_not_self_heal);
    RUN_TEST(test_present_and_unmounted_does_not_self_heal);
    return UNITY_END();
}
