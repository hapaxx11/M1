#include "unity.h"
#include "m1_esp32_hal_lifecycle.h"

static int s_stage;
static int s_kick_count;
static bool s_lock_result;

void setUp(void)
{
    s_stage = 0;
    s_kick_count = 0;
    s_lock_result = true;
}

void tearDown(void) { }

static void kick(void)
{
    TEST_ASSERT_TRUE(s_kick_count < 2);
    s_kick_count++;
}

static bool lock_wait(uint32_t timeout_ticks)
{
    TEST_ASSERT_EQUAL_UINT32(15000, timeout_ticks);
    TEST_ASSERT_EQUAL_INT(1, s_kick_count);
    s_stage = 1;
    return s_lock_result;
}

static void test_lock_wait_kicks_before_and_after_success(void)
{
    TEST_ASSERT_TRUE(m1_esp32_lifecycle_lock_with_wdt(lock_wait, kick, 15000));
    TEST_ASSERT_EQUAL_INT(1, s_stage);
    TEST_ASSERT_EQUAL_INT(2, s_kick_count);
}

static void test_lock_wait_kicks_after_timeout_too(void)
{
    s_lock_result = false;
    TEST_ASSERT_FALSE(m1_esp32_lifecycle_lock_with_wdt(lock_wait, kick, 15000));
    TEST_ASSERT_EQUAL_INT(1, s_stage);
    TEST_ASSERT_EQUAL_INT(2, s_kick_count);
}

static void test_invalid_callbacks_are_rejected(void)
{
    TEST_ASSERT_FALSE(m1_esp32_lifecycle_lock_with_wdt(NULL, kick, 15000));
    TEST_ASSERT_FALSE(m1_esp32_lifecycle_lock_with_wdt(lock_wait, NULL, 15000));
    TEST_ASSERT_EQUAL_INT(0, s_kick_count);
}

static void test_only_at_transport_requires_legacy_task(void)
{
    TEST_ASSERT_TRUE(m1_esp32_lifecycle_at_task_required(ESP32_TRANSPORT_AT));
    TEST_ASSERT_FALSE(m1_esp32_lifecycle_at_task_required(ESP32_TRANSPORT_RPC));
    TEST_ASSERT_FALSE(m1_esp32_lifecycle_at_task_required(ESP32_TRANSPORT_BINARY_SPI));
    TEST_ASSERT_FALSE(m1_esp32_lifecycle_at_task_required(ESP32_TRANSPORT_NATIVE_V1));
    TEST_ASSERT_FALSE(m1_esp32_lifecycle_at_task_required(ESP32_TRANSPORT_NONE));
}

static void test_foreground_ownership_nests_and_saturates(void)
{
    uint8_t users = 0u;
    users = m1_esp32_lifecycle_foreground_update(users, true);
    users = m1_esp32_lifecycle_foreground_update(users, true);
    TEST_ASSERT_EQUAL_UINT8(2u, users);
    users = m1_esp32_lifecycle_foreground_update(users, false);
    TEST_ASSERT_EQUAL_UINT8(1u, users);
    users = m1_esp32_lifecycle_foreground_update(users, false);
    TEST_ASSERT_EQUAL_UINT8(0u, users);
    TEST_ASSERT_EQUAL_UINT8(0u, m1_esp32_lifecycle_foreground_update(users, false));
    TEST_ASSERT_EQUAL_UINT8(UINT8_MAX,
        m1_esp32_lifecycle_foreground_update(UINT8_MAX, true));
}

static void test_relay_requires_no_foreground_owner_or_ota(void)
{
    TEST_ASSERT_TRUE(m1_esp32_lifecycle_relay_allowed(0u, false));
    TEST_ASSERT_FALSE(m1_esp32_lifecycle_relay_allowed(1u, false));
    TEST_ASSERT_FALSE(m1_esp32_lifecycle_relay_allowed(0u, true));
    TEST_ASSERT_FALSE(m1_esp32_lifecycle_relay_allowed(1u, true));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_lock_wait_kicks_before_and_after_success);
    RUN_TEST(test_lock_wait_kicks_after_timeout_too);
    RUN_TEST(test_invalid_callbacks_are_rejected);
    RUN_TEST(test_only_at_transport_requires_legacy_task);
    RUN_TEST(test_foreground_ownership_nests_and_saturates);
    RUN_TEST(test_relay_requires_no_foreground_owner_or_ota);
    return UNITY_END();
}
