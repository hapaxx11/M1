/* See COPYING.txt for license details. */

/*
 * test_rf_fingerprint.c
 *
 * Unit tests for rf_fingerprint — the sensor-agnostic fingerprint extractor
 * that composes subghz_mod_suggest (modulation) and rf_repetition_detect
 * (repeat count) and fills band/bandwidth/SNR from capture metadata.
 *
 * Build:
 *   cmake -B build-tests -S tests && cmake --build build-tests
 *   ctest --test-dir build-tests --output-on-failure -R rf_fingerprint
 */

#include <string.h>
#include "unity.h"
#include "rf_fingerprint.h"

void setUp(void) {}
void tearDown(void) {}

/* Clean OOK PWM (Princeton-like) burst builder. */
static uint16_t push_ook(int16_t *buf, uint16_t idx, int pairs, int te)
{
    for (int i = 0; i < pairs; i++)
    {
        if (i & 1) { buf[idx++] = (int16_t)te;        buf[idx++] = (int16_t)(-3*te); }
        else       { buf[idx++] = (int16_t)(3*te);    buf[idx++] = (int16_t)(-te);   }
    }
    return idx;
}

/*============================================================================*/
/* Band mapping                                                               */
/*============================================================================*/

void test_band_from_freq(void)
{
    TEST_ASSERT_EQUAL_UINT16(RF_BAND_315,  rf_band_from_freq(315000000U));
    TEST_ASSERT_EQUAL_UINT16(RF_BAND_433,  rf_band_from_freq(433920000U));
    TEST_ASSERT_EQUAL_UINT16(RF_BAND_868,  rf_band_from_freq(868350000U));
    TEST_ASSERT_EQUAL_UINT16(RF_BAND_915,  rf_band_from_freq(915000000U));
    TEST_ASSERT_EQUAL_UINT16(RF_BAND_2400, rf_band_from_freq(2440000000U));
    TEST_ASSERT_EQUAL_UINT16(RF_BAND_300,  rf_band_from_freq(303875000U));
    TEST_ASSERT_EQUAL_UINT16(0,            rf_band_from_freq(1000000U)); /* out of band */
}

void test_preset_bandwidth(void)
{
    TEST_ASSERT_EQUAL_UINT32(270000U, rf_bandwidth_for_preset(0)); /* AM270 */
    TEST_ASSERT_EQUAL_UINT32(650000U, rf_bandwidth_for_preset(1)); /* AM650 */
    TEST_ASSERT_EQUAL_UINT32(238000U, rf_bandwidth_for_preset(2)); /* FM238 */
    TEST_ASSERT_EQUAL_UINT32(476000U, rf_bandwidth_for_preset(3)); /* FM476 */
    TEST_ASSERT_EQUAL_UINT32(0U,      rf_bandwidth_for_preset(99));
}

/*============================================================================*/
/* Extraction                                                                 */
/*============================================================================*/

void test_null_out_is_safe(void)
{
    int16_t buf[4] = { 300, -300, 300, -300 };
    rf_fingerprint_from_subghz_raw(buf, 4, 433920000U, 1, -60, -95, NULL);
    /* no crash == pass */
    TEST_PASS();
}

void test_subghz_ook_fingerprint(void)
{
    int16_t buf[512];
    uint16_t n = push_ook(buf, 0, 60, 350);

    rf_fingerprint_t fp;
    rf_fingerprint_from_subghz_raw(buf, n, 433920000U, 1 /*AM650*/, -55, -98, &fp);

    TEST_ASSERT_EQUAL(RF_SENSOR_SUBGHZ, fp.sensor);
    TEST_ASSERT_EQUAL_UINT16(RF_BAND_433, fp.band);
    TEST_ASSERT_EQUAL(RF_MOD_OOK, fp.mod);           /* preset AM650 => OOK */
    TEST_ASSERT_EQUAL_UINT32(650000U, fp.bandwidth_hz);
    TEST_ASSERT_TRUE(fp.te_us > 0);
    TEST_ASSERT_TRUE(fp.pulse_count > 0);
    TEST_ASSERT_EQUAL_INT16(-55, fp.rssi_dbm);
    TEST_ASSERT_EQUAL_INT16(-98, fp.noise_dbm);
    TEST_ASSERT_EQUAL_INT16(43,  fp.snr_db);          /* -55 - (-98) */
}

void test_preset_forces_modulation_family(void)
{
    /* Even a clean OOK waveform must report FSK when the operator picked an
     * FM preset — the demod path chosen is definitive for the family. */
    int16_t buf[512];
    uint16_t n = push_ook(buf, 0, 60, 350);

    rf_fingerprint_t fp;
    rf_fingerprint_from_subghz_raw(buf, n, 868350000U, 2 /*FM238*/, 0, 0, &fp);

    TEST_ASSERT_EQUAL(RF_MOD_FSK, fp.mod);
    TEST_ASSERT_EQUAL_UINT32(238000U, fp.bandwidth_hz);
    TEST_ASSERT_EQUAL_UINT16(RF_BAND_868, fp.band);
    TEST_ASSERT_EQUAL_INT16(0, fp.snr_db);            /* unknown rssi/noise */
}

void test_repetition_populates_fingerprint(void)
{
    int16_t buf[1024];
    uint16_t n = 0;
    for (int b = 0; b < 4; b++)
    {
        n = push_ook(buf, n, 20, 300);
        buf[n++] = (int16_t)(-9000);   /* gap */
    }
    rf_fingerprint_t fp;
    rf_fingerprint_from_subghz_raw(buf, n, 433920000U, 1, 0, 0, &fp);
    TEST_ASSERT_EQUAL_UINT8(4, fp.repetition);
    TEST_ASSERT_TRUE(fp.rep_confidence > 0);
}

void test_mod_family_strings(void)
{
    TEST_ASSERT_EQUAL_STRING("OOK/AM", rf_mod_family_str(RF_MOD_OOK));
    TEST_ASSERT_EQUAL_STRING("FSK/FM", rf_mod_family_str(RF_MOD_FSK));
    TEST_ASSERT_EQUAL_STRING("?",      rf_mod_family_str(RF_MOD_UNKNOWN));
}

/*============================================================================*/
/* Runner                                                                     */
/*============================================================================*/

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_band_from_freq);
    RUN_TEST(test_preset_bandwidth);

    RUN_TEST(test_null_out_is_safe);
    RUN_TEST(test_subghz_ook_fingerprint);
    RUN_TEST(test_preset_forces_modulation_family);
    RUN_TEST(test_repetition_populates_fingerprint);
    RUN_TEST(test_mod_family_strings);

    return UNITY_END();
}
