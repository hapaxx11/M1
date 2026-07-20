/* See COPYING.txt for license details. */

/*
 * test_rf_fingerprint_24.c
 *
 * Host unit tests for the ESP32-C6 2.4 GHz fingerprint backend
 * (Sub_Ghz/rf_fingerprint_24.c) — channel->frequency helpers, the three
 * sensor extractors, the abstract capability mapping, and the sensor-aware
 * rf_match_24() identification path.
 *
 * M1 Project — Hapax fork
 */

#include "unity.h"
#include "rf_fingerprint_24.h"
#include "rf_fingerprint.h"
#include "rf_protocol_db.h"
#include "rf_match.h"

#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/*============================================================================*/
/* Channel -> frequency                                                       */
/*============================================================================*/

static void test_wifi_channel_to_freq(void)
{
    TEST_ASSERT_EQUAL_UINT32(2412000000U, rf_wifi_channel_to_freq(1));
    TEST_ASSERT_EQUAL_UINT32(2437000000U, rf_wifi_channel_to_freq(6));
    TEST_ASSERT_EQUAL_UINT32(2472000000U, rf_wifi_channel_to_freq(13));
    TEST_ASSERT_EQUAL_UINT32(2484000000U, rf_wifi_channel_to_freq(14));
    /* Out of range */
    TEST_ASSERT_EQUAL_UINT32(0U, rf_wifi_channel_to_freq(0));
    TEST_ASSERT_EQUAL_UINT32(0U, rf_wifi_channel_to_freq(15));
}

static void test_ble_channel_to_freq(void)
{
    /* Advertising channels */
    TEST_ASSERT_EQUAL_UINT32(2402000000U, rf_ble_channel_to_freq(37));
    TEST_ASSERT_EQUAL_UINT32(2426000000U, rf_ble_channel_to_freq(38));
    TEST_ASSERT_EQUAL_UINT32(2480000000U, rf_ble_channel_to_freq(39));
    /* Data channels */
    TEST_ASSERT_EQUAL_UINT32(2404000000U, rf_ble_channel_to_freq(0));
    TEST_ASSERT_EQUAL_UINT32(2424000000U, rf_ble_channel_to_freq(10));
    TEST_ASSERT_EQUAL_UINT32(2428000000U, rf_ble_channel_to_freq(11));
    TEST_ASSERT_EQUAL_UINT32(2478000000U, rf_ble_channel_to_freq(36));
    /* Out of range */
    TEST_ASSERT_EQUAL_UINT32(0U, rf_ble_channel_to_freq(40));
    TEST_ASSERT_EQUAL_UINT32(0U, rf_ble_channel_to_freq(255));
}

static void test_802154_channel_to_freq(void)
{
    TEST_ASSERT_EQUAL_UINT32(2405000000U, rf_802154_channel_to_freq(11));
    TEST_ASSERT_EQUAL_UINT32(2440000000U, rf_802154_channel_to_freq(18));
    TEST_ASSERT_EQUAL_UINT32(2480000000U, rf_802154_channel_to_freq(26));
    /* Out of range */
    TEST_ASSERT_EQUAL_UINT32(0U, rf_802154_channel_to_freq(10));
    TEST_ASSERT_EQUAL_UINT32(0U, rf_802154_channel_to_freq(27));
}

/* Every channel that maps to a frequency must land in the 2.4 GHz band. */
static void test_all_channels_in_2400_band(void)
{
    for (uint8_t ch = 1; ch <= 14; ch++)
    {
        uint32_t f = rf_wifi_channel_to_freq(ch);
        if (f != 0U)
            TEST_ASSERT_EQUAL_UINT16(RF_BAND_2400, rf_band_from_freq(f));
    }
    for (uint8_t ch = 0; ch <= 39; ch++)
    {
        uint32_t f = rf_ble_channel_to_freq(ch);
        if (f != 0U)
            TEST_ASSERT_EQUAL_UINT16(RF_BAND_2400, rf_band_from_freq(f));
    }
    for (uint8_t ch = 11; ch <= 26; ch++)
    {
        uint32_t f = rf_802154_channel_to_freq(ch);
        if (f != 0U)
            TEST_ASSERT_EQUAL_UINT16(RF_BAND_2400, rf_band_from_freq(f));
    }
}

/*============================================================================*/
/* Capability mapping                                                         */
/*============================================================================*/

static void test_required_cap_mapping(void)
{
    TEST_ASSERT_EQUAL_INT(RF24_CAP_BLE_SCAN,  rf_sensor_24_required_cap(RF_SENSOR_BLE));
    TEST_ASSERT_EQUAL_INT(RF24_CAP_WIFI_SCAN, rf_sensor_24_required_cap(RF_SENSOR_WIFI));
    TEST_ASSERT_EQUAL_INT(RF24_CAP_802154,    rf_sensor_24_required_cap(RF_SENSOR_802154));
    /* A Sub-GHz sensor is not a 2.4 GHz domain. */
    TEST_ASSERT_EQUAL_INT(RF24_CAP_NONE,      rf_sensor_24_required_cap(RF_SENSOR_SUBGHZ));
}

/*============================================================================*/
/* Extraction                                                                 */
/*============================================================================*/

static void test_extract_null_safe(void)
{
    /* Must not crash on a NULL output. */
    rf_fingerprint_from_ble(37, -60, NULL);
    rf_fingerprint_from_wifi(6, -60, NULL);
    rf_fingerprint_from_802154(15, -60, NULL);
}

static void test_extract_ble(void)
{
    rf_fingerprint_t fp;
    rf_fingerprint_from_ble(38, -55, &fp);

    TEST_ASSERT_EQUAL_INT(RF_SENSOR_BLE, fp.sensor);
    TEST_ASSERT_EQUAL_UINT32(2426000000U, fp.freq_hz);
    TEST_ASSERT_EQUAL_UINT16(RF_BAND_2400, fp.band);
    TEST_ASSERT_EQUAL_INT(RF_MOD_FSK, fp.mod);
    TEST_ASSERT_EQUAL_INT16(-55, fp.rssi_dbm);
    TEST_ASSERT_EQUAL_UINT8(1, fp.repetition);
    /* Data-poor: no timing / no bit estimate. */
    TEST_ASSERT_EQUAL_UINT16(0, fp.te_us);
    TEST_ASSERT_EQUAL_UINT16(0, fp.est_bits);
    /* Band + modulation is enough to name the domain. */
    TEST_ASSERT_TRUE(rf_fingerprint_is_discriminating(&fp));
}

static void test_extract_wifi(void)
{
    rf_fingerprint_t fp;
    rf_fingerprint_from_wifi(11, -70, &fp);

    TEST_ASSERT_EQUAL_INT(RF_SENSOR_WIFI, fp.sensor);
    TEST_ASSERT_EQUAL_UINT32(2462000000U, fp.freq_hz);
    TEST_ASSERT_EQUAL_UINT16(RF_BAND_2400, fp.band);
    TEST_ASSERT_EQUAL_INT(RF_MOD_FSK, fp.mod);
    TEST_ASSERT_EQUAL_INT16(-70, fp.rssi_dbm);
}

static void test_extract_802154(void)
{
    rf_fingerprint_t fp;
    rf_fingerprint_from_802154(20, -80, &fp);

    TEST_ASSERT_EQUAL_INT(RF_SENSOR_802154, fp.sensor);
    TEST_ASSERT_EQUAL_UINT32(2450000000U, fp.freq_hz);
    TEST_ASSERT_EQUAL_UINT16(RF_BAND_2400, fp.band);
    TEST_ASSERT_EQUAL_INT(RF_MOD_FSK, fp.mod);
    TEST_ASSERT_EQUAL_INT16(-80, fp.rssi_dbm);
}

/* An out-of-range channel still yields a valid banded fingerprint. */
static void test_extract_bad_channel_keeps_band(void)
{
    rf_fingerprint_t fp;
    rf_fingerprint_from_wifi(99, -60, &fp);
    TEST_ASSERT_EQUAL_UINT32(0U, fp.freq_hz);
    TEST_ASSERT_EQUAL_UINT16(RF_BAND_2400, fp.band);
    TEST_ASSERT_EQUAL_INT(RF_SENSOR_WIFI, fp.sensor);
}

/*============================================================================*/
/* Identification                                                             */
/*============================================================================*/

static void test_db_find_2400(void)
{
    int ble  = rf_protocol_db_find_2400(RF_SENSOR_BLE);
    int wifi = rf_protocol_db_find_2400(RF_SENSOR_WIFI);
    int zig  = rf_protocol_db_find_2400(RF_SENSOR_802154);

    TEST_ASSERT_GREATER_OR_EQUAL_INT(0, ble);
    TEST_ASSERT_GREATER_OR_EQUAL_INT(0, wifi);
    TEST_ASSERT_GREATER_OR_EQUAL_INT(0, zig);
    /* Three distinct signatures. */
    TEST_ASSERT_NOT_EQUAL(ble, wifi);
    TEST_ASSERT_NOT_EQUAL(ble, zig);
    TEST_ASSERT_NOT_EQUAL(wifi, zig);
    /* Each resolved entry is on the 2.4 GHz band. */
    TEST_ASSERT_TRUE((rf_protocol_db_get((uint16_t)ble)->bands  & RF_BAND_2400) != 0U);
    TEST_ASSERT_TRUE((rf_protocol_db_get((uint16_t)wifi)->bands & RF_BAND_2400) != 0U);
    TEST_ASSERT_TRUE((rf_protocol_db_get((uint16_t)zig)->bands  & RF_BAND_2400) != 0U);
    /* A non-2.4 sensor has no 2.4 GHz signature. */
    TEST_ASSERT_EQUAL_INT(-1, rf_protocol_db_find_2400(RF_SENSOR_SUBGHZ));
}

/* Each sensor identifies as its own domain, not the first same-band entry. */
static void test_match_24_disambiguates(void)
{
    rf_fingerprint_t ble, wifi, zig;
    rf_fingerprint_from_ble(37, -50, &ble);
    rf_fingerprint_from_wifi(6, -50, &wifi);
    rf_fingerprint_from_802154(15, -50, &zig);

    rf_match_result_t rb = rf_match_24(&ble);
    rf_match_result_t rw = rf_match_24(&wifi);
    rf_match_result_t rz = rf_match_24(&zig);

    TEST_ASSERT_NOT_NULL(rb.sig);
    TEST_ASSERT_NOT_NULL(rw.sig);
    TEST_ASSERT_NOT_NULL(rz.sig);

    TEST_ASSERT_EQUAL_INT(rf_protocol_db_find_2400(RF_SENSOR_BLE),    rb.index);
    TEST_ASSERT_EQUAL_INT(rf_protocol_db_find_2400(RF_SENSOR_WIFI),   rw.index);
    TEST_ASSERT_EQUAL_INT(rf_protocol_db_find_2400(RF_SENSOR_802154), rz.index);

    /* Band + modulation both agree -> full confidence. */
    TEST_ASSERT_GREATER_OR_EQUAL_UINT8(RF_MATCH_MIN_CONFIDENCE, rb.confidence);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT8(RF_MATCH_MIN_CONFIDENCE, rw.confidence);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT8(RF_MATCH_MIN_CONFIDENCE, rz.confidence);

    /* The three resolve to different DB entries. */
    TEST_ASSERT_NOT_EQUAL(rb.index, rw.index);
    TEST_ASSERT_NOT_EQUAL(rb.index, rz.index);
    TEST_ASSERT_NOT_EQUAL(rw.index, rz.index);
}

static void test_match_24_null_and_non_24(void)
{
    rf_match_result_t r = rf_match_24(NULL);
    TEST_ASSERT_EQUAL_INT(-1, r.index);
    TEST_ASSERT_NULL(r.sig);

    /* A Sub-GHz fingerprint is not identified by the 2.4 GHz backend. */
    rf_fingerprint_t sub;
    memset(&sub, 0, sizeof(sub));
    sub.sensor = RF_SENSOR_SUBGHZ;
    sub.band   = RF_BAND_433;
    sub.mod    = RF_MOD_OOK;
    rf_match_result_t rs = rf_match_24(&sub);
    TEST_ASSERT_EQUAL_INT(-1, rs.index);
    TEST_ASSERT_NULL(rs.sig);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_wifi_channel_to_freq);
    RUN_TEST(test_ble_channel_to_freq);
    RUN_TEST(test_802154_channel_to_freq);
    RUN_TEST(test_all_channels_in_2400_band);
    RUN_TEST(test_required_cap_mapping);
    RUN_TEST(test_extract_null_safe);
    RUN_TEST(test_extract_ble);
    RUN_TEST(test_extract_wifi);
    RUN_TEST(test_extract_802154);
    RUN_TEST(test_extract_bad_channel_keeps_band);
    RUN_TEST(test_db_find_2400);
    RUN_TEST(test_match_24_disambiguates);
    RUN_TEST(test_match_24_null_and_non_24);
    return UNITY_END();
}
