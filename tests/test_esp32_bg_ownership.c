/* See COPYING.txt for license details. */

#include "unity.h"
#include "m1_esp32_bg_owner.h"

void setUp(void) {}
void tearDown(void) {}

void test_background_owners_are_independent(void)
{
    uint8_t owners = 0u;
    owners = m1_esp32_bg_owner_update(
        owners, M1_ESP32_BG_OWNER_BLE_DIRECT, true);
    owners = m1_esp32_bg_owner_update(
        owners, M1_ESP32_BG_OWNER_WIFI_STA, true);

    TEST_ASSERT_TRUE(m1_esp32_bg_owner_any(owners));
    owners = m1_esp32_bg_owner_update(
        owners, M1_ESP32_BG_OWNER_BLE_DIRECT, false);
    TEST_ASSERT_TRUE(m1_esp32_bg_owner_any(owners));
    owners = m1_esp32_bg_owner_update(
        owners, M1_ESP32_BG_OWNER_WIFI_STA, false);
    TEST_ASSERT_FALSE(m1_esp32_bg_owner_any(owners));
}

void test_hotspot_owner_keeps_transport_until_stopped(void)
{
    uint8_t owners = m1_esp32_bg_owner_update(
        0u, M1_ESP32_BG_OWNER_HOTSPOT, true);
    TEST_ASSERT_TRUE(m1_esp32_bg_owner_any(owners));
    owners = m1_esp32_bg_owner_update(
        owners, M1_ESP32_BG_OWNER_HOTSPOT, false);
    TEST_ASSERT_FALSE(m1_esp32_bg_owner_any(owners));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_background_owners_are_independent);
    RUN_TEST(test_hotspot_owner_keeps_transport_until_stopped);
    return UNITY_END();
}
