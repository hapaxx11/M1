#include "unity.h"
#include "lfrfid_tpms_power.h"

void setUp(void) {}
void tearDown(void) {}

void test_tpms_activation_restores_disabled_supply(void)
{
    lfrfid_tpms_power_state_t state = lfrfid_tpms_power_started(false);

    TEST_ASSERT_TRUE(lfrfid_tpms_power_is_active(state));
    TEST_ASSERT_FALSE(lfrfid_tpms_power_was_enabled(state));
}

void test_tpms_activation_preserves_preexisting_supply(void)
{
    lfrfid_tpms_power_state_t state = lfrfid_tpms_power_started(true);

    TEST_ASSERT_TRUE(lfrfid_tpms_power_is_active(state));
    TEST_ASSERT_TRUE(lfrfid_tpms_power_was_enabled(state));
}

void test_tpms_activation_state_is_cleared_after_stop(void)
{
    lfrfid_tpms_power_state_t state = lfrfid_tpms_power_started(true);
    state = 0U;
    TEST_ASSERT_FALSE(lfrfid_tpms_power_is_active(state));
    TEST_ASSERT_FALSE(lfrfid_tpms_power_was_enabled(state));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_tpms_activation_restores_disabled_supply);
    RUN_TEST(test_tpms_activation_preserves_preexisting_supply);
    RUN_TEST(test_tpms_activation_state_is_cleared_after_stop);
    return UNITY_END();
}
