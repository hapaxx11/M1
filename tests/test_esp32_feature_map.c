/* See COPYING.txt for license details. */

/**
 * @file   test_esp32_feature_map.c
 * @brief  Host-side unit tests for the ESP32 feature-to-capability classifier.
 *
 *

 * Covers:
 *   esp32_feature_supported()       — bitmap × feature → bool
 *   esp32_feature_required_caps()   — feature → required M1_ESP32_CAP_* bits
 *   esp32_feature_label()           — feature → UI label string
 *   esp32_firmware_is_sin360()      — firmware-class classifier
 *
 * All functions are pure logic with zero HAL deps; no stubs required.
 *
 * Build:
 *   cmake -B build-tests -S tests && cmake --build build-tests
 *   ctest --test-dir build-tests --output-on-failure
 */

#include "unity.h"
#include "esp32_feature_map.h"
#include "m1_esp32_caps.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

void setUp(void)    {}
void tearDown(void) {}

/* =========================================================================
 * Helpers
 * =========================================================================*/

/** Build a capability bitmap that satisfies exactly the bits needed for fid. */
static uint64_t caps_for(esp32_feature_id_t fid)
{
    return esp32_feature_required_caps(fid);
}

/* =========================================================================
 * esp32_feature_required_caps — out-of-range guard
 * =========================================================================*/

void test_required_caps_invalid_id_returns_zero(void)
{
    TEST_ASSERT_EQUAL_UINT64(0u,
        esp32_feature_required_caps((esp32_feature_id_t)ESP32_FEATURE_COUNT));
    TEST_ASSERT_EQUAL_UINT64(0u,
        esp32_feature_required_caps((esp32_feature_id_t)255));
}

/* =========================================================================
 * esp32_feature_label — out-of-range guard + non-empty checks
 * =========================================================================*/

void test_label_invalid_id_returns_empty_string(void)
{
    const char *l = esp32_feature_label((esp32_feature_id_t)ESP32_FEATURE_COUNT);
    TEST_ASSERT_NOT_NULL(l);
    TEST_ASSERT_EQUAL_UINT(0u, strlen(l));
}

void test_label_all_valid_ids_are_nonempty(void)
{
    for (int i = 0; i < (int)ESP32_FEATURE_COUNT; i++)
    {
        const char *l = esp32_feature_label((esp32_feature_id_t)i);
        TEST_ASSERT_NOT_NULL_MESSAGE(l, "label returned NULL");
        TEST_ASSERT_GREATER_THAN_MESSAGE(0u, strlen(l),
            "label is empty string for a valid feature ID");
    }
}

/* =========================================================================
 * esp32_feature_supported — boundary conditions
 * =========================================================================*/

void test_supported_invalid_id_returns_false(void)
{
    TEST_ASSERT_FALSE(
        esp32_feature_supported(UINT64_MAX,
                                (esp32_feature_id_t)ESP32_FEATURE_COUNT));
}

void test_supported_zero_bitmap_is_unsupported_for_all(void)
{
    for (int i = 0; i < (int)ESP32_FEATURE_COUNT; i++)
    {
        /* Every feature requires at least one cap bit, so an all-zero
         * bitmap must yield false for every feature. */
        TEST_ASSERT_FALSE_MESSAGE(
            esp32_feature_supported(0u, (esp32_feature_id_t)i),
            esp32_feature_label((esp32_feature_id_t)i));
    }
}

void test_supported_all_ones_bitmap_is_supported_for_all(void)
{
    for (int i = 0; i < (int)ESP32_FEATURE_COUNT; i++)
    {
        TEST_ASSERT_TRUE_MESSAGE(
            esp32_feature_supported(UINT64_MAX, (esp32_feature_id_t)i),
            esp32_feature_label((esp32_feature_id_t)i));
    }
}

/** Each feature is supported exactly when the required caps are present. */
void test_supported_exact_required_caps_returns_true(void)
{
    for (int i = 0; i < (int)ESP32_FEATURE_COUNT; i++)
    {
        uint64_t need = caps_for((esp32_feature_id_t)i);
        TEST_ASSERT_TRUE_MESSAGE(
            esp32_feature_supported(need, (esp32_feature_id_t)i),
            esp32_feature_label((esp32_feature_id_t)i));
    }
}

/** A bitmap with exactly the required bits set supports the feature; removing
 *  any single required bit makes it unsupported. */
void test_supported_missing_one_required_bit_returns_false(void)
{
    for (int i = 0; i < (int)ESP32_FEATURE_COUNT; i++)
    {
        uint64_t need = caps_for((esp32_feature_id_t)i);

        /* Iterate over each set bit in 'need' and verify removing it
         * makes the feature unsupported. */
        for (uint64_t bit = 1u; bit != 0u; bit <<= 1)
        {
            if (!(need & bit))
                continue;
            uint64_t missing_one = need & ~bit;
            TEST_ASSERT_FALSE_MESSAGE(
                esp32_feature_supported(missing_one, (esp32_feature_id_t)i),
                esp32_feature_label((esp32_feature_id_t)i));
        }
    }
}

/* =========================================================================
 * Per-feature: required_caps matches the corresponding M1_ESP32_CAP_* bit
 * =========================================================================*/

void test_wifi_scan_maps_to_cap_wifi_scan(void)
{
    TEST_ASSERT_EQUAL_UINT64(M1_ESP32_CAP_WIFI_SCAN,
        esp32_feature_required_caps(ESP32_FEATURE_WIFI_SCAN));
}

void test_sta_scan_maps_to_cap_sta_scan(void)
{
    TEST_ASSERT_EQUAL_UINT64(M1_ESP32_CAP_STA_SCAN,
        esp32_feature_required_caps(ESP32_FEATURE_STA_SCAN));
}

void test_pktmon_maps_to_cap_pktmon(void)
{
    TEST_ASSERT_EQUAL_UINT64(M1_ESP32_CAP_PKTMON,
        esp32_feature_required_caps(ESP32_FEATURE_PKTMON));
}

void test_deauth_maps_to_cap_deauth(void)
{
    TEST_ASSERT_EQUAL_UINT64(M1_ESP32_CAP_DEAUTH,
        esp32_feature_required_caps(ESP32_FEATURE_DEAUTH));
}

void test_beacon_maps_to_cap_beacon(void)
{
    TEST_ASSERT_EQUAL_UINT64(M1_ESP32_CAP_BEACON,
        esp32_feature_required_caps(ESP32_FEATURE_BEACON));
}

void test_probe_flood_maps_to_cap_probe_flood(void)
{
    TEST_ASSERT_EQUAL_UINT64(M1_ESP32_CAP_PROBE_FLOOD,
        esp32_feature_required_caps(ESP32_FEATURE_PROBE_FLOOD));
}

void test_karma_maps_to_cap_karma(void)
{
    TEST_ASSERT_EQUAL_UINT64(M1_ESP32_CAP_KARMA,
        esp32_feature_required_caps(ESP32_FEATURE_KARMA));
}

void test_portal_maps_to_cap_portal(void)
{
    TEST_ASSERT_EQUAL_UINT64(M1_ESP32_CAP_PORTAL,
        esp32_feature_required_caps(ESP32_FEATURE_PORTAL));
}

void test_wifi_join_maps_to_cap_wifi_join(void)
{
    TEST_ASSERT_EQUAL_UINT64(M1_ESP32_CAP_WIFI_JOIN,
        esp32_feature_required_caps(ESP32_FEATURE_WIFI_JOIN));
}

void test_wifi_set_mac_maps_to_cap_wifi_set_mac(void)
{
    TEST_ASSERT_EQUAL_UINT64(M1_ESP32_CAP_WIFI_SET_MAC,
        esp32_feature_required_caps(ESP32_FEATURE_WIFI_SET_MAC));
}

void test_wifi_set_chan_maps_to_cap_wifi_set_chan(void)
{
    TEST_ASSERT_EQUAL_UINT64(M1_ESP32_CAP_WIFI_SET_CHAN,
        esp32_feature_required_caps(ESP32_FEATURE_WIFI_SET_CHAN));
}

void test_netscan_maps_to_cap_netscan(void)
{
    TEST_ASSERT_EQUAL_UINT64(M1_ESP32_CAP_NETSCAN,
        esp32_feature_required_caps(ESP32_FEATURE_NETSCAN));
}

void test_802154_maps_to_cap_802154(void)
{
    TEST_ASSERT_EQUAL_UINT64(M1_ESP32_CAP_802154,
        esp32_feature_required_caps(ESP32_FEATURE_802154));
}

void test_ble_scan_maps_to_cap_ble_scan(void)
{
    TEST_ASSERT_EQUAL_UINT64(M1_ESP32_CAP_BLE_SCAN,
        esp32_feature_required_caps(ESP32_FEATURE_BLE_SCAN));
}

void test_ble_adv_maps_to_cap_ble_adv(void)
{
    TEST_ASSERT_EQUAL_UINT64(M1_ESP32_CAP_BLE_ADV,
        esp32_feature_required_caps(ESP32_FEATURE_BLE_ADV));
}

void test_ble_hid_maps_to_cap_ble_hid(void)
{
    TEST_ASSERT_EQUAL_UINT64(M1_ESP32_CAP_BLE_HID,
        esp32_feature_required_caps(ESP32_FEATURE_BLE_HID));
}

void test_ble_gatt_maps_to_cap_ble_gatt(void)
{
    TEST_ASSERT_EQUAL_UINT64(M1_ESP32_CAP_BLE_GATT,
        esp32_feature_required_caps(ESP32_FEATURE_BLE_GATT));
}

void test_bt_manage_maps_to_cap_bt_manage(void)
{
    TEST_ASSERT_EQUAL_UINT64(M1_ESP32_CAP_BT_MANAGE,
        esp32_feature_required_caps(ESP32_FEATURE_BT_MANAGE));
}

void test_wifi_disconnect_maps_to_cap_wifi_set_chan(void)
{
    /* ESP32_FEATURE_WIFI_DISCONNECT reuses M1_ESP32_CAP_WIFI_SET_CHAN as its
     * gate: neither dag T-800 nor SiN360 set this bit, so the "WiFi Disconnect"
     * label is shown on both firmware types until a firmware that properly
     * supports CMD_WIFI_DISCONNECT sets the bit. */
    TEST_ASSERT_EQUAL_UINT64(M1_ESP32_CAP_WIFI_SET_CHAN,
        esp32_feature_required_caps(ESP32_FEATURE_WIFI_DISCONNECT));
}

void test_pmkid_maps_to_cap_pmkid(void)
{
    TEST_ASSERT_EQUAL_UINT64(M1_ESP32_CAP_PMKID,
        esp32_feature_required_caps(ESP32_FEATURE_PMKID));
}

void test_handshake_maps_to_cap_handshake(void)
{
    TEST_ASSERT_EQUAL_UINT64(M1_ESP32_CAP_HANDSHAKE,
        esp32_feature_required_caps(ESP32_FEATURE_HANDSHAKE));
}

void test_ota_maps_to_cap_ota(void)
{
    TEST_ASSERT_EQUAL_UINT64(M1_ESP32_CAP_OTA,
        esp32_feature_required_caps(ESP32_FEATURE_OTA));
}

void test_espnow_maps_to_cap_espnow(void)
{
    TEST_ASSERT_EQUAL_UINT64(M1_ESP32_CAP_ESPNOW,
        esp32_feature_required_caps(ESP32_FEATURE_ESPNOW));
}

void test_wifi_hotspot_maps_to_cap_wifi_hotspot(void)
{
    TEST_ASSERT_EQUAL_UINT64(M1_ESP32_CAP_WIFI_HOTSPOT,
        esp32_feature_required_caps(ESP32_FEATURE_WIFI_HOTSPOT));
}

/* =========================================================================
 * Per-feature: supported() on SiN360 profile bitmap
 * =========================================================================*/

/** SiN360 profile does NOT set WIFI_JOIN, so AT-layer connect features
 *  must be unsupported on SiN360. */
void test_sin360_profile_lacks_wifi_join(void)
{
    TEST_ASSERT_FALSE(
        esp32_feature_supported(M1_ESP32_CAP_PROFILE_SIN360,
                                ESP32_FEATURE_WIFI_JOIN));
    TEST_ASSERT_FALSE(
        esp32_feature_supported(M1_ESP32_CAP_PROFILE_SIN360,
                                ESP32_FEATURE_WIFI_SET_MAC));
    TEST_ASSERT_FALSE(
        esp32_feature_supported(M1_ESP32_CAP_PROFILE_SIN360,
                                ESP32_FEATURE_WIFI_SET_CHAN));
    TEST_ASSERT_FALSE(
        esp32_feature_supported(M1_ESP32_CAP_PROFILE_SIN360,
                                ESP32_FEATURE_WIFI_DISCONNECT));
}

/** SiN360 profile supports all listed attack + scan features. */
void test_sin360_profile_supports_attack_features(void)
{
    static const esp32_feature_id_t sin360_features[] = {
        ESP32_FEATURE_WIFI_SCAN,
        ESP32_FEATURE_STA_SCAN,
        ESP32_FEATURE_PKTMON,
        ESP32_FEATURE_DEAUTH,
        ESP32_FEATURE_BEACON,
        ESP32_FEATURE_PROBE_FLOOD,
        ESP32_FEATURE_KARMA,
        ESP32_FEATURE_PORTAL,
        ESP32_FEATURE_NETSCAN,
        ESP32_FEATURE_BLE_SCAN,
        ESP32_FEATURE_BLE_ADV,
        ESP32_FEATURE_BLE_HID,
        ESP32_FEATURE_BLE_GATT,
    };
    for (size_t i = 0; i < sizeof(sin360_features)/sizeof(sin360_features[0]); i++)
    {
        TEST_ASSERT_TRUE_MESSAGE(
            esp32_feature_supported(M1_ESP32_CAP_PROFILE_SIN360,
                                    sin360_features[i]),
            esp32_feature_label(sin360_features[i]));
    }
}

/* =========================================================================
 * Per-feature: supported() on CD3 profile bitmap
 * =========================================================================*/

void test_cd3_profile_has_pmkid_handshake_ota(void)
{
    TEST_ASSERT_TRUE(
        esp32_feature_supported(M1_ESP32_CAP_PROFILE_CD3, ESP32_FEATURE_PMKID));
    TEST_ASSERT_TRUE(
        esp32_feature_supported(M1_ESP32_CAP_PROFILE_CD3, ESP32_FEATURE_HANDSHAKE));
    TEST_ASSERT_TRUE(
        esp32_feature_supported(M1_ESP32_CAP_PROFILE_CD3, ESP32_FEATURE_OTA));
}

void test_cd3_profile_lacks_netscan(void)
{
    /* CD3 v1 does not include a ping/ARP network scanner */
    TEST_ASSERT_FALSE(
        esp32_feature_supported(M1_ESP32_CAP_PROFILE_CD3, ESP32_FEATURE_NETSCAN));
}

void test_cd3_profile_supports_wifi_and_ble(void)
{
    static const esp32_feature_id_t cd3_features[] = {
        ESP32_FEATURE_WIFI_SCAN,
        ESP32_FEATURE_WIFI_JOIN,
        ESP32_FEATURE_STA_SCAN,
        ESP32_FEATURE_DEAUTH,
        ESP32_FEATURE_BEACON,
        ESP32_FEATURE_PROBE_FLOOD,
        ESP32_FEATURE_KARMA,
        ESP32_FEATURE_PKTMON,
        ESP32_FEATURE_PORTAL,
        ESP32_FEATURE_BLE_SCAN,
        ESP32_FEATURE_BLE_ADV,
        ESP32_FEATURE_BLE_HID,
        ESP32_FEATURE_BLE_GATT,
        ESP32_FEATURE_802154,
    };
    for (size_t i = 0; i < sizeof(cd3_features)/sizeof(cd3_features[0]); i++)
    {
        TEST_ASSERT_TRUE_MESSAGE(
            esp32_feature_supported(M1_ESP32_CAP_PROFILE_CD3, cd3_features[i]),
            esp32_feature_label(cd3_features[i]));
    }
}

/* =========================================================================
 * esp32_firmware_is_cd3 — classifier
 * =========================================================================*/

void test_is_cd3_zero_bitmap_returns_false(void)
{
    TEST_ASSERT_FALSE(esp32_firmware_is_cd3(0u));
}

void test_is_cd3_profile_cd3_returns_true(void)
{
    TEST_ASSERT_TRUE(esp32_firmware_is_cd3(M1_ESP32_CAP_PROFILE_CD3));
}

void test_is_cd3_handshake_and_ota_returns_true(void)
{
    /* Minimum CD3 discriminator: both HANDSHAKE and OTA must be set */
    TEST_ASSERT_TRUE(
        esp32_firmware_is_cd3(M1_ESP32_CAP_HANDSHAKE | M1_ESP32_CAP_OTA));
}

void test_is_cd3_handshake_only_returns_false(void)
{
    /* HANDSHAKE without OTA is not enough to identify CD3 */
    TEST_ASSERT_FALSE(esp32_firmware_is_cd3(M1_ESP32_CAP_HANDSHAKE));
}

void test_is_cd3_ota_only_returns_false(void)
{
    /* OTA without HANDSHAKE is not enough to identify CD3 */
    TEST_ASSERT_FALSE(esp32_firmware_is_cd3(M1_ESP32_CAP_OTA));
}

void test_is_cd3_sin360_profile_returns_false(void)
{
    /* SiN360 has neither HANDSHAKE nor OTA */
    TEST_ASSERT_FALSE(esp32_firmware_is_cd3(M1_ESP32_CAP_PROFILE_SIN360));
}

void test_is_cd3_all_ones_returns_true(void)
{
    /* All bits set includes both HANDSHAKE and OTA → CD3 */
    TEST_ASSERT_TRUE(esp32_firmware_is_cd3(UINT64_MAX));
}

/* =========================================================================
 * esp32_firmware_is_sin360 — classifier
 * =========================================================================*/

void test_is_sin360_zero_bitmap_returns_false(void)
{
    TEST_ASSERT_FALSE(esp32_firmware_is_sin360(0u));
}

void test_is_sin360_profile_sin360_returns_true(void)
{
    /* SiN360 profile: BLE_HID set, WIFI_JOIN absent */
    TEST_ASSERT_TRUE(esp32_firmware_is_sin360(M1_ESP32_CAP_PROFILE_SIN360));
}

void test_is_sin360_ble_hid_only_returns_true(void)
{
    /* Minimum SiN360 discriminator: BLE_HID present, WIFI_JOIN absent */
    TEST_ASSERT_TRUE(
        esp32_firmware_is_sin360(M1_ESP32_CAP_BLE_HID));
}

void test_is_sin360_wifi_join_only_returns_false(void)
{
    /* AT firmware: WIFI_JOIN present — cannot be SiN360 */
    TEST_ASSERT_FALSE(
        esp32_firmware_is_sin360(M1_ESP32_CAP_WIFI_JOIN));
}

void test_is_sin360_ble_hid_plus_wifi_join_returns_false(void)
{
    /* Both bits set → AT firmware path (e.g. bedge117 with BLE HID ext) */
    TEST_ASSERT_FALSE(
        esp32_firmware_is_sin360(M1_ESP32_CAP_BLE_HID | M1_ESP32_CAP_WIFI_JOIN));
}

void test_is_sin360_all_ones_returns_false(void)
{
    /* All bits set includes WIFI_JOIN → classified as AT-path, not SiN360 */
    TEST_ASSERT_FALSE(esp32_firmware_is_sin360(UINT64_MAX));
}

void test_is_sin360_bt_manage_only_returns_false(void)
{
    /* BT_MANAGE without BLE_HID → unknown/partial AT bitmap, not SiN360 */
    TEST_ASSERT_FALSE(
        esp32_firmware_is_sin360(M1_ESP32_CAP_BT_MANAGE));
}

/* =========================================================================
 * Feature count sanity
 * =========================================================================*/

void test_feature_count_equals_24(void)
{
    /* Update this test (and the feature table) whenever new IDs are added. */
    TEST_ASSERT_EQUAL_INT(24, (int)ESP32_FEATURE_COUNT);
}

/* =========================================================================
 * Test runner
 * =========================================================================*/

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_required_caps_invalid_id_returns_zero);

    RUN_TEST(test_label_invalid_id_returns_empty_string);
    RUN_TEST(test_label_all_valid_ids_are_nonempty);

    RUN_TEST(test_supported_invalid_id_returns_false);
    RUN_TEST(test_supported_zero_bitmap_is_unsupported_for_all);
    RUN_TEST(test_supported_all_ones_bitmap_is_supported_for_all);
    RUN_TEST(test_supported_exact_required_caps_returns_true);
    RUN_TEST(test_supported_missing_one_required_bit_returns_false);

    RUN_TEST(test_wifi_scan_maps_to_cap_wifi_scan);
    RUN_TEST(test_sta_scan_maps_to_cap_sta_scan);
    RUN_TEST(test_pktmon_maps_to_cap_pktmon);
    RUN_TEST(test_deauth_maps_to_cap_deauth);
    RUN_TEST(test_beacon_maps_to_cap_beacon);
    RUN_TEST(test_probe_flood_maps_to_cap_probe_flood);
    RUN_TEST(test_karma_maps_to_cap_karma);
    RUN_TEST(test_portal_maps_to_cap_portal);
    RUN_TEST(test_wifi_join_maps_to_cap_wifi_join);
    RUN_TEST(test_wifi_set_mac_maps_to_cap_wifi_set_mac);
    RUN_TEST(test_wifi_set_chan_maps_to_cap_wifi_set_chan);
    RUN_TEST(test_wifi_disconnect_maps_to_cap_wifi_set_chan);
    RUN_TEST(test_netscan_maps_to_cap_netscan);
    RUN_TEST(test_802154_maps_to_cap_802154);
    RUN_TEST(test_ble_scan_maps_to_cap_ble_scan);
    RUN_TEST(test_ble_adv_maps_to_cap_ble_adv);
    RUN_TEST(test_ble_hid_maps_to_cap_ble_hid);
    RUN_TEST(test_ble_gatt_maps_to_cap_ble_gatt);
    RUN_TEST(test_bt_manage_maps_to_cap_bt_manage);
    RUN_TEST(test_pmkid_maps_to_cap_pmkid);
    RUN_TEST(test_handshake_maps_to_cap_handshake);
    RUN_TEST(test_ota_maps_to_cap_ota);
    RUN_TEST(test_espnow_maps_to_cap_espnow);
    RUN_TEST(test_wifi_hotspot_maps_to_cap_wifi_hotspot);

    RUN_TEST(test_sin360_profile_lacks_wifi_join);
    RUN_TEST(test_sin360_profile_supports_attack_features);

    RUN_TEST(test_cd3_profile_has_pmkid_handshake_ota);
    RUN_TEST(test_cd3_profile_lacks_netscan);
    RUN_TEST(test_cd3_profile_supports_wifi_and_ble);

    RUN_TEST(test_is_sin360_zero_bitmap_returns_false);
    RUN_TEST(test_is_sin360_profile_sin360_returns_true);
    RUN_TEST(test_is_sin360_ble_hid_only_returns_true);
    RUN_TEST(test_is_sin360_wifi_join_only_returns_false);
    RUN_TEST(test_is_sin360_ble_hid_plus_wifi_join_returns_false);
    RUN_TEST(test_is_sin360_all_ones_returns_false);
    RUN_TEST(test_is_sin360_bt_manage_only_returns_false);

    RUN_TEST(test_is_cd3_zero_bitmap_returns_false);
    RUN_TEST(test_is_cd3_profile_cd3_returns_true);
    RUN_TEST(test_is_cd3_handshake_and_ota_returns_true);
    RUN_TEST(test_is_cd3_handshake_only_returns_false);
    RUN_TEST(test_is_cd3_ota_only_returns_false);
    RUN_TEST(test_is_cd3_sin360_profile_returns_false);
    RUN_TEST(test_is_cd3_all_ones_returns_true);

    RUN_TEST(test_feature_count_equals_24);

    return UNITY_END();
}
