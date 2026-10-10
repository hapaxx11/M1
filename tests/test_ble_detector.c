/* See COPYING.txt for license details. */

#include "unity.h"
#include "ble_detector.h"

void setUp(void) { }
void tearDown(void) { }

void test_skimmer_module_names_match_exactly_and_case_insensitively(void)
{
    TEST_ASSERT_TRUE(ble_detector_name_matches("HMSoft",
                                               BLE_DETECTOR_SKIMMER_MODULE));
    TEST_ASSERT_TRUE(ble_detector_name_matches("jdy-10",
                                               BLE_DETECTOR_SKIMMER_MODULE));
    TEST_ASSERT_FALSE(ble_detector_name_matches("HMSoft Sensor",
                                                BLE_DETECTOR_SKIMMER_MODULE));
    TEST_ASSERT_FALSE(ble_detector_name_matches(NULL,
                                                BLE_DETECTOR_SKIMMER_MODULE));
}

void test_flock_name_requires_flock_marker_or_six_hex_device_suffix(void)
{
    TEST_ASSERT_TRUE(ble_detector_name_matches("Flock Camera",
                                               BLE_DETECTOR_FLOCK));
    TEST_ASSERT_TRUE(ble_detector_name_matches("FS-01a2F9",
                                               BLE_DETECTOR_FLOCK));
    TEST_ASSERT_FALSE(ble_detector_name_matches("FS-01a2G9",
                                                BLE_DETECTOR_FLOCK));
    TEST_ASSERT_FALSE(ble_detector_name_matches("FS-12345",
                                                BLE_DETECTOR_FLOCK));
    TEST_ASSERT_FALSE(ble_detector_name_matches("Flockish",
                                                BLE_DETECTOR_FLOCK));
}

void test_meta_name_matches_ray_ban_meta_names(void)
{
    TEST_ASSERT_TRUE(ble_detector_name_matches("Ray-Ban Meta 1234",
                                               BLE_DETECTOR_RAY_BAN_META));
    TEST_ASSERT_TRUE(ble_detector_name_matches("RAYBAN META",
                                               BLE_DETECTOR_RAY_BAN_META));
    TEST_ASSERT_FALSE(ble_detector_name_matches("Meta Quest",
                                                BLE_DETECTOR_RAY_BAN_META));
}

void test_airtag_advertisement_requires_apple_manufacturer_signature(void)
{
    const uint8_t airtag[] = {5, 0xFF, 0x4C, 0x00, 0x12, 0x01};
    const uint8_t unrelated_apple[] = {5, 0xFF, 0x4C, 0x00, 0x07, 0x01};

    TEST_ASSERT_TRUE(ble_detector_advertisement_matches(
        airtag, sizeof(airtag), BLE_DETECTOR_AIRTAG));
    TEST_ASSERT_FALSE(ble_detector_advertisement_matches(
        unrelated_apple, sizeof(unrelated_apple), BLE_DETECTOR_AIRTAG));
}

void test_meta_advertisement_matches_service_uuid_or_service_data(void)
{
    const uint8_t service_uuid[] = {3, 0x03, 0x5F, 0xFD};
    const uint8_t service_data[] = {4, 0x16, 0x5F, 0xFD, 0x01};
    const uint8_t unrelated[] = {3, 0x03, 0x0D, 0x18};

    TEST_ASSERT_TRUE(ble_detector_advertisement_matches(
        service_uuid, sizeof(service_uuid), BLE_DETECTOR_RAY_BAN_META_ADV));
    TEST_ASSERT_TRUE(ble_detector_advertisement_matches(
        service_data, sizeof(service_data), BLE_DETECTOR_RAY_BAN_META_ADV));
    TEST_ASSERT_FALSE(ble_detector_advertisement_matches(
        unrelated, sizeof(unrelated), BLE_DETECTOR_RAY_BAN_META_ADV));
}

void test_advertisement_parser_rejects_truncated_fields(void)
{
    const uint8_t truncated[] = {5, 0xFF, 0x4C, 0x00};
    const uint8_t empty[] = {0};

    TEST_ASSERT_FALSE(ble_detector_advertisement_matches(
        truncated, sizeof(truncated), BLE_DETECTOR_AIRTAG));
    TEST_ASSERT_FALSE(ble_detector_advertisement_matches(
        empty, sizeof(empty), BLE_DETECTOR_AIRTAG));
    TEST_ASSERT_FALSE(ble_detector_advertisement_matches(
        NULL, 1u, BLE_DETECTOR_AIRTAG));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_skimmer_module_names_match_exactly_and_case_insensitively);
    RUN_TEST(test_flock_name_requires_flock_marker_or_six_hex_device_suffix);
    RUN_TEST(test_meta_name_matches_ray_ban_meta_names);
    RUN_TEST(test_airtag_advertisement_requires_apple_manufacturer_signature);
    RUN_TEST(test_meta_advertisement_matches_service_uuid_or_service_data);
    RUN_TEST(test_advertisement_parser_rejects_truncated_fields);
    return UNITY_END();
}
