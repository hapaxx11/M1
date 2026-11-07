#include "unity.h"
#include "esp32_ota_route.h"
#include "m1_esp32_caps.h"

void setUp(void) { }
void tearDown(void) { }

static void test_app_image_names(void)
{
    TEST_ASSERT_TRUE(esp32_ota_image_is_app("app_m1-esp32-brain.bin"));
    TEST_ASSERT_TRUE(esp32_ota_image_is_app("0:/ESP32_FW/app_m1-esp32-brain.bin"));
    TEST_ASSERT_TRUE(esp32_ota_image_is_app("m1-esp32-brain.bin"));
    TEST_ASSERT_FALSE(esp32_ota_image_is_app("factory_ESP32C6-SPI.bin"));
    TEST_ASSERT_FALSE(esp32_ota_image_is_app(NULL));
}

static void test_rpc_requires_app_and_capability(void)
{
    TEST_ASSERT_TRUE(esp32_ota_use_rpc("app_m1-esp32-brain.bin", M1_ESP32_CAP_OTA));
    TEST_ASSERT_FALSE(esp32_ota_use_rpc("app_m1-esp32-brain.bin", 0));
    TEST_ASSERT_FALSE(esp32_ota_use_rpc("factory_ESP32C6-SPI.bin", M1_ESP32_CAP_OTA));
}

static void test_host_cleanup_owns_esp_reboot(void)
{
    TEST_ASSERT_FALSE(esp32_ota_end_requests_reboot());
}

static void test_native_ota_does_not_reinit_uart(void)
{
    TEST_ASSERT_TRUE(esp32_ota_uart_reset_required(false));
    TEST_ASSERT_FALSE(esp32_ota_uart_reset_required(true));
}

static void test_relay_pauses_for_ota(void)
{
    TEST_ASSERT_TRUE(esp32_ota_relay_allowed(false));
    TEST_ASSERT_FALSE(esp32_ota_relay_allowed(true));
}

static void test_native_ota_skips_legacy_at_task(void)
{
    TEST_ASSERT_TRUE(esp32_ota_legacy_task_required(false));
    TEST_ASSERT_FALSE(esp32_ota_legacy_task_required(true));
}

static void test_native_ota_resets_over_rpc(void)
{
    TEST_ASSERT_FALSE(esp32_ota_reset_via_rpc(false));
    TEST_ASSERT_TRUE(esp32_ota_reset_via_rpc(true));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_app_image_names);
    RUN_TEST(test_rpc_requires_app_and_capability);
    RUN_TEST(test_host_cleanup_owns_esp_reboot);
    RUN_TEST(test_native_ota_does_not_reinit_uart);
    RUN_TEST(test_relay_pauses_for_ota);
    RUN_TEST(test_native_ota_skips_legacy_at_task);
    RUN_TEST(test_native_ota_resets_over_rpc);
    return UNITY_END();
}
