/* See COPYING.txt for license details. */

#include "unity.h"
#include "wifi_boot_config.h"

void setUp(void) {}
void tearDown(void) {}

void test_default_config_has_no_primary_or_auto_connect(void)
{
    wifi_boot_config_t config;
    uint8_t index = 42;

    wifi_boot_config_reset(&config);
    TEST_ASSERT_EQUAL_UINT8(WIFI_BOOT_PRIMARY_NONE, config.primary_index);
    TEST_ASSERT_FALSE(config.auto_connect);
    TEST_ASSERT_FALSE(wifi_boot_config_target(&config, 3, &index));
    TEST_ASSERT_EQUAL_UINT8(42, index);
}

void test_primary_selection_enables_single_saved_network(void)
{
    wifi_boot_config_t config;
    uint8_t index = WIFI_BOOT_PRIMARY_NONE;

    wifi_boot_config_reset(&config);
    TEST_ASSERT_TRUE(wifi_boot_config_set_primary(&config, 2, 3));
    TEST_ASSERT_TRUE(config.auto_connect);
    TEST_ASSERT_TRUE(wifi_boot_config_target(&config, 3, &index));
    TEST_ASSERT_EQUAL_UINT8(2, index);
}

void test_invalid_or_deleted_primary_never_falls_back(void)
{
    wifi_boot_config_t config;
    uint8_t index = 42;

    wifi_boot_config_reset(&config);
    TEST_ASSERT_FALSE(wifi_boot_config_set_primary(&config, 3, 3));
    TEST_ASSERT_FALSE(config.auto_connect);

    TEST_ASSERT_TRUE(wifi_boot_config_set_primary(&config, 1, 2));
    wifi_boot_config_on_delete(&config, 1);
    TEST_ASSERT_FALSE(wifi_boot_config_target(&config, 1, &index));
    TEST_ASSERT_EQUAL_UINT8(WIFI_BOOT_PRIMARY_NONE, config.primary_index);
    TEST_ASSERT_FALSE(config.auto_connect);
}

void test_delete_before_primary_shifts_saved_index(void)
{
    wifi_boot_config_t config;
    uint8_t index = WIFI_BOOT_PRIMARY_NONE;

    wifi_boot_config_reset(&config);
    TEST_ASSERT_TRUE(wifi_boot_config_set_primary(&config, 3, 4));
    wifi_boot_config_on_delete(&config, 1);
    TEST_ASSERT_TRUE(wifi_boot_config_target(&config, 3, &index));
    TEST_ASSERT_EQUAL_UINT8(2, index);
}

void test_disabling_auto_connect_preserves_primary_selection(void)
{
    wifi_boot_config_t config;
    uint8_t index = WIFI_BOOT_PRIMARY_NONE;

    wifi_boot_config_reset(&config);
    TEST_ASSERT_TRUE(wifi_boot_config_set_primary(&config, 0, 1));
    wifi_boot_config_set_enabled(&config, false);
    TEST_ASSERT_FALSE(wifi_boot_config_target(&config, 1, &index));
    TEST_ASSERT_EQUAL_UINT8(0, config.primary_index);

    wifi_boot_config_set_enabled(&config, true);
    TEST_ASSERT_TRUE(wifi_boot_config_target(&config, 1, &index));
    TEST_ASSERT_EQUAL_UINT8(0, index);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_default_config_has_no_primary_or_auto_connect);
    RUN_TEST(test_primary_selection_enables_single_saved_network);
    RUN_TEST(test_invalid_or_deleted_primary_never_falls_back);
    RUN_TEST(test_delete_before_primary_shifts_saved_index);
    RUN_TEST(test_disabling_auto_connect_preserves_primary_selection);
    return UNITY_END();
}
