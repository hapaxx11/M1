/* See COPYING.txt for license details. */

/*
 * test_rf_sweep_display.c
 *
 * Unit tests for the Signal Identifier display formatting helpers
 * (rf_sweep_display.c/h).
 */

#include "unity.h"
#include "rf_sweep_display.h"

#include <string.h>

void setUp(void) { }
void tearDown(void) { }

/*============================================================================*/
/* rf_sweep_display_strip_tags                                                */
/*============================================================================*/

void test_strip_tags_removes_single_tag(void)
{
    char buf[32];
    rf_sweep_display_strip_tags(buf, sizeof(buf), "Weather Station (868)");
    TEST_ASSERT_EQUAL_STRING("Weather Station", buf);
}

void test_strip_tags_removes_fixed_tag(void)
{
    char buf[32];
    rf_sweep_display_strip_tags(buf, sizeof(buf), "Car Key Fob (fixed)");
    TEST_ASSERT_EQUAL_STRING("Car Key Fob", buf);
}

void test_strip_tags_removes_rolling_tag(void)
{
    char buf[32];
    rf_sweep_display_strip_tags(buf, sizeof(buf), "Garage/Gate (rolling)");
    TEST_ASSERT_EQUAL_STRING("Garage/Gate", buf);
}

void test_strip_tags_no_tag(void)
{
    char buf[32];
    rf_sweep_display_strip_tags(buf, sizeof(buf), "TPMS Tyre Sensor");
    TEST_ASSERT_EQUAL_STRING("TPMS Tyre Sensor", buf);
}

void test_strip_tags_multiple_tags(void)
{
    char buf[48];
    rf_sweep_display_strip_tags(buf, sizeof(buf), "Signal (A) Name (B)");
    TEST_ASSERT_EQUAL_STRING("Signal Name", buf);
}

void test_strip_tags_null_name(void)
{
    char buf[16];
    rf_sweep_display_strip_tags(buf, sizeof(buf), NULL);
    TEST_ASSERT_EQUAL_STRING("", buf);
}

void test_strip_tags_empty_name(void)
{
    char buf[16];
    rf_sweep_display_strip_tags(buf, sizeof(buf), "");
    TEST_ASSERT_EQUAL_STRING("", buf);
}

void test_strip_tags_unclosed_paren(void)
{
    char buf[32];
    rf_sweep_display_strip_tags(buf, sizeof(buf), "Signal (no close");
    TEST_ASSERT_EQUAL_STRING("Signal (no close", buf);
}

void test_strip_tags_truncation(void)
{
    char buf[10];
    rf_sweep_display_strip_tags(buf, sizeof(buf), "Very Long Signal Name (tag)");
    /* Should truncate at buf_len - 1 */
    TEST_ASSERT_EQUAL(9, (int)strlen(buf));
    TEST_ASSERT_EQUAL_STRING("Very Long", buf);
}

/*============================================================================*/
/* rf_sweep_display_security_prefix                                           */
/*============================================================================*/

void test_security_prefix_fixed(void)
{
    TEST_ASSERT_EQUAL_STRING("F:", rf_sweep_display_security_prefix(RF_SEC_FIXED));
}

void test_security_prefix_rolling(void)
{
    TEST_ASSERT_EQUAL_STRING("R:", rf_sweep_display_security_prefix(RF_SEC_ROLLING));
}

void test_security_prefix_encrypted(void)
{
    TEST_ASSERT_EQUAL_STRING("E:", rf_sweep_display_security_prefix(RF_SEC_ENCRYPTED));
}

void test_security_prefix_unknown(void)
{
    TEST_ASSERT_EQUAL_STRING("", rf_sweep_display_security_prefix(RF_SEC_UNKNOWN));
}

/*============================================================================*/
/* rf_sweep_display_format_hit                                                */
/*============================================================================*/

static const rf_protocol_sig_t test_sig_fixed = {
    .name = "Car Key Fob (fixed)",
    .category = RF_CAT_AUTOMOTIVE,
    .mod = RF_MOD_OOK,
    .bands = (RF_BAND_315 | RF_BAND_433),
    .security = RF_SEC_FIXED,
};

static const rf_protocol_sig_t test_sig_rolling = {
    .name = "Garage/Gate (rolling)",
    .category = RF_CAT_HOME,
    .mod = RF_MOD_OOK,
    .bands = (RF_BAND_315 | RF_BAND_433),
    .security = RF_SEC_ROLLING,
};

static const rf_protocol_sig_t test_sig_weather = {
    .name = "Weather Station (868)",
    .category = RF_CAT_WEATHER,
    .mod = RF_MOD_FSK,
    .bands = RF_BAND_868,
    .security = RF_SEC_FIXED,
};

static const rf_protocol_sig_t test_sig_no_tag = {
    .name = "TPMS Tyre Sensor",
    .category = RF_CAT_AUTOMOTIVE,
    .mod = RF_MOD_FSK,
    .bands = (RF_BAND_315 | RF_BAND_433),
    .security = RF_SEC_FIXED,
};

void test_format_hit_basic(void)
{
    char buf[RF_SWEEP_DISP_LINE_LEN];
    rf_sweep_hit_t hit = {
        .sig        = &test_sig_fixed,
        .freq_hz    = 433920000UL,
        .band       = RF_BAND_433,
        .category   = RF_CAT_AUTOMOTIVE,
        .security   = RF_SEC_FIXED,
        .confidence = 87,
        .rssi_dbm   = -62,
        .hits       = 3,
    };

    rf_sweep_display_format_hit(buf, sizeof(buf), &hit, 2);
    TEST_ASSERT_EQUAL_STRING("433.92 F:Car Key Fob 87%", buf);
}

void test_format_hit_rolling(void)
{
    char buf[RF_SWEEP_DISP_LINE_LEN];
    rf_sweep_hit_t hit = {
        .sig        = &test_sig_rolling,
        .freq_hz    = 315000000UL,
        .band       = RF_BAND_315,
        .category   = RF_CAT_HOME,
        .security   = RF_SEC_ROLLING,
        .confidence = 72,
        .rssi_dbm   = -70,
        .hits       = 5,
    };

    rf_sweep_display_format_hit(buf, sizeof(buf), &hit, 2);
    TEST_ASSERT_EQUAL_STRING("315.00 R:Garage/Gate 72%", buf);
}

void test_format_hit_weather_868(void)
{
    char buf[RF_SWEEP_DISP_LINE_LEN];
    rf_sweep_hit_t hit = {
        .sig        = &test_sig_weather,
        .freq_hz    = 868350000UL,
        .band       = RF_BAND_868,
        .category   = RF_CAT_WEATHER,
        .security   = RF_SEC_FIXED,
        .confidence = 58,
        .rssi_dbm   = -80,
        .hits       = 2,
    };

    rf_sweep_display_format_hit(buf, sizeof(buf), &hit, 2);
    TEST_ASSERT_EQUAL_STRING("868.35 F:Weather Station 58%", buf);
}

void test_format_hit_below_min_hits_shows_question(void)
{
    char buf[RF_SWEEP_DISP_LINE_LEN];
    rf_sweep_hit_t hit = {
        .sig        = &test_sig_fixed,
        .freq_hz    = 433920000UL,
        .band       = RF_BAND_433,
        .category   = RF_CAT_AUTOMOTIVE,
        .security   = RF_SEC_FIXED,
        .confidence = 87,
        .rssi_dbm   = -62,
        .hits       = 1,  /* below min_hits=2 */
    };

    rf_sweep_display_format_hit(buf, sizeof(buf), &hit, 2);
    TEST_ASSERT_EQUAL_STRING("433.92 F:Car Key Fob ?%", buf);
}

void test_format_hit_no_tag_in_name(void)
{
    char buf[RF_SWEEP_DISP_LINE_LEN];
    rf_sweep_hit_t hit = {
        .sig        = &test_sig_no_tag,
        .freq_hz    = 315000000UL,
        .band       = RF_BAND_315,
        .category   = RF_CAT_AUTOMOTIVE,
        .security   = RF_SEC_FIXED,
        .confidence = 65,
        .rssi_dbm   = -75,
        .hits       = 4,
    };

    rf_sweep_display_format_hit(buf, sizeof(buf), &hit, 2);
    TEST_ASSERT_EQUAL_STRING("315.00 F:TPMS Tyre Sensor 65%", buf);
}

void test_format_hit_null_hit(void)
{
    char buf[RF_SWEEP_DISP_LINE_LEN];
    buf[0] = 'X';
    rf_sweep_display_format_hit(buf, sizeof(buf), NULL, 2);
    TEST_ASSERT_EQUAL_STRING("", buf);
}

void test_format_hit_min_hits_zero_always_shows_confidence(void)
{
    char buf[RF_SWEEP_DISP_LINE_LEN];
    rf_sweep_hit_t hit = {
        .sig        = &test_sig_fixed,
        .freq_hz    = 433920000UL,
        .band       = RF_BAND_433,
        .category   = RF_CAT_AUTOMOTIVE,
        .security   = RF_SEC_FIXED,
        .confidence = 50,
        .rssi_dbm   = -80,
        .hits       = 1,
    };

    rf_sweep_display_format_hit(buf, sizeof(buf), &hit, 0);
    /* With min_hits=0, hits (1) >= 0 is always true → show numeric */
    TEST_ASSERT_EQUAL_STRING("433.92 F:Car Key Fob 50%", buf);
}

void test_format_hit_decode_confirmed(void)
{
    char buf[RF_SWEEP_DISP_LINE_LEN];
    /* decode-confirmed hit: sig==NULL, decode_name set, confidence=100 */
    rf_sweep_hit_t hit = {
        .sig         = NULL,
        .decode_name = "Princeton",
        .freq_hz     = 433920000UL,
        .band        = RF_BAND_433,
        .category    = RF_CAT_UNKNOWN,
        .security    = RF_SEC_UNKNOWN,
        .confidence  = 100,
        .rssi_dbm    = -68,
        .hits        = 3,
    };

    rf_sweep_display_format_hit(buf, sizeof(buf), &hit, 2);
    /* Security prefix must be absent; confidence shown as "100%" */
    TEST_ASSERT_EQUAL_STRING("433.92 Princeton 100%", buf);
}

void test_format_hit_decode_confirmed_skips_min_hits_guard(void)
{
    char buf[RF_SWEEP_DISP_LINE_LEN];
    /* decode-confirmed hit with only 1 hit but min_hits=2 */
    rf_sweep_hit_t hit = {
        .sig         = NULL,
        .decode_name = "NiceFlorS",
        .freq_hz     = 433920000UL,
        .band        = RF_BAND_433,
        .category    = RF_CAT_UNKNOWN,
        .security    = RF_SEC_UNKNOWN,
        .confidence  = 100,
        .rssi_dbm    = -72,
        .hits        = 1,
    };

    rf_sweep_display_format_hit(buf, sizeof(buf), &hit, 2);
    /* Must show "100%", not "?%" — a decode is definitive */
    TEST_ASSERT_EQUAL_STRING("433.92 NiceFlorS 100%", buf);
}

/*============================================================================*/
/* Main                                                                       */
/*============================================================================*/

int main(void)
{
    UNITY_BEGIN();

    /* strip_tags */
    RUN_TEST(test_strip_tags_removes_single_tag);
    RUN_TEST(test_strip_tags_removes_fixed_tag);
    RUN_TEST(test_strip_tags_removes_rolling_tag);
    RUN_TEST(test_strip_tags_no_tag);
    RUN_TEST(test_strip_tags_multiple_tags);
    RUN_TEST(test_strip_tags_null_name);
    RUN_TEST(test_strip_tags_empty_name);
    RUN_TEST(test_strip_tags_unclosed_paren);
    RUN_TEST(test_strip_tags_truncation);

    /* security_prefix */
    RUN_TEST(test_security_prefix_fixed);
    RUN_TEST(test_security_prefix_rolling);
    RUN_TEST(test_security_prefix_encrypted);
    RUN_TEST(test_security_prefix_unknown);

    /* format_hit */
    RUN_TEST(test_format_hit_basic);
    RUN_TEST(test_format_hit_rolling);
    RUN_TEST(test_format_hit_weather_868);
    RUN_TEST(test_format_hit_below_min_hits_shows_question);
    RUN_TEST(test_format_hit_no_tag_in_name);
    RUN_TEST(test_format_hit_null_hit);
    RUN_TEST(test_format_hit_min_hits_zero_always_shows_confidence);
    RUN_TEST(test_format_hit_decode_confirmed);
    RUN_TEST(test_format_hit_decode_confirmed_skips_min_hits_guard);

    return UNITY_END();
}
