/* See COPYING.txt for license details. */

/*
 * test_subghz_weather_parse.c
 *
 * Unit tests for weather-station field extraction
 * (Sub_Ghz/subghz_weather_parse.c).
 *
 * Bug context:
 *   The Weather Station scene never displayed a sensor reading: only four of
 *   the ~30 weather protocols populated the shared weather_data struct, so a
 *   successful decode of (for example) a LaCrosse TX141TH-Bv2 produced a raw
 *   data word and nothing else.  This module turns the decoded word into
 *   id / channel / battery / temperature / humidity using the same bit layouts
 *   and checksums as the Flipper Weather Station app, and rejects frames that
 *   fail their checksum (the scene retries decoding at sliding pulse offsets,
 *   so mis-aligned garbage must not reach the UI).
 *
 * The LaCrosse vector is a real over-the-air capture (the Flipper screenshot
 * in the issue: "TX141THBv2 41b, Sn: 0x67, Btn: 0, Batt: ok,
 * Data: 0xCE061093CD, 81.7F, 73%").  The remaining vectors are built from the
 * documented field layouts with checksums computed by an independent Python
 * reference implementation.
 *
 * Build:
 *   cmake -B build-tests -S tests && cmake --build build-tests
 *   ctest --test-dir build-tests --output-on-failure -R subghz_weather_parse
 */

#include "unity.h"
#include "subghz_weather_parse.h"
#include "m1_sub_ghz_decenc.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/* ------------------------------------------------------------------ */
/* LaCrosse TX141TH-Bv2 — real capture from the issue screenshot       */
/* ------------------------------------------------------------------ */

void test_lacrosse_tx141thbv2_real_capture(void)
{
    SubGhzWeatherFields f;
    TEST_ASSERT_TRUE(subghz_weather_parse(LACROSSE_TX141THBV2,
                                          0xCE061093CDULL, 41, &f));
    TEST_ASSERT_EQUAL_UINT32(0x67, f.id);
    TEST_ASSERT_EQUAL_UINT8(1, f.channel);
    TEST_ASSERT_EQUAL_UINT8(0, f.button);
    TEST_ASSERT_EQUAL_UINT8(0, f.battery_low);
    TEST_ASSERT_TRUE(f.has_temp);
    TEST_ASSERT_EQUAL_INT16(276, f.temp_d10);      /* 27.6 C == 81.7 F */
    TEST_ASSERT_EQUAL_UINT8(73, f.humidity);
}

void test_lacrosse_tx141thbv2_40bit_form(void)
{
    /* The same frame without the stray leading bit. */
    SubGhzWeatherFields f;
    TEST_ASSERT_TRUE(subghz_weather_parse(LACROSSE_TX141THBV2,
                                          0xCE061093CDULL >> 1, 40, &f));
    TEST_ASSERT_EQUAL_UINT32(0x67, f.id);
    TEST_ASSERT_EQUAL_INT16(276, f.temp_d10);
}

void test_lacrosse_bad_crc_rejected(void)
{
    SubGhzWeatherFields f;
    /* Flip a payload bit — the LFSR digest must no longer match. */
    TEST_ASSERT_FALSE(subghz_weather_parse(LACROSSE_TX141THBV2,
                                           0xCE061093CDULL ^ 0x1000ULL, 41, &f));
}

void test_wrong_bit_length_rejected(void)
{
    SubGhzWeatherFields f;
    TEST_ASSERT_FALSE(subghz_weather_parse(LACROSSE_TX141THBV2,
                                           0xCE061093CDULL, 39, &f));
}

/* ------------------------------------------------------------------ */
/* Remaining protocols                                                 */
/* ------------------------------------------------------------------ */

void test_nexus_th(void)
{
    SubGhzWeatherFields f;
    TEST_ASSERT_TRUE(subghz_weather_parse(NEXUS_TH, 0x5A90EAF37ULL, 36, &f));
    TEST_ASSERT_EQUAL_UINT32(0x5A, f.id);
    TEST_ASSERT_EQUAL_UINT8(0, f.battery_low);     /* flag 1 == OK */
    TEST_ASSERT_EQUAL_UINT8(2, f.channel);
    TEST_ASSERT_EQUAL_INT16(234, f.temp_d10);
    TEST_ASSERT_EQUAL_UINT8(55, f.humidity);
}

void test_nexus_th_requires_const_nibble(void)
{
    SubGhzWeatherFields f;
    TEST_ASSERT_FALSE(subghz_weather_parse(NEXUS_TH,
                                           0x5A90EAF37ULL & ~0x100ULL, 36, &f));
}

void test_gt_wt02_negative_temp(void)
{
    SubGhzWeatherFields f;
    TEST_ASSERT_TRUE(subghz_weather_parse(GT_WT02, 0x78DF93001ULL, 37, &f));
    TEST_ASSERT_EQUAL_UINT32(0x3C, f.id);
    TEST_ASSERT_EQUAL_UINT8(1, f.button);
    TEST_ASSERT_EQUAL_UINT8(3, f.channel);
    TEST_ASSERT_EQUAL_INT16(-55, f.temp_d10);
    TEST_ASSERT_EQUAL_UINT8(64, f.humidity);
}

void test_acurite_609txc(void)
{
    SubGhzWeatherFields f;
    TEST_ASSERT_TRUE(subghz_weather_parse(ACURITE_609TXC, 0xB1807D30DEULL, 40, &f));
    TEST_ASSERT_EQUAL_UINT32(0xB1, f.id);
    TEST_ASSERT_EQUAL_INT16(125, f.temp_d10);
    TEST_ASSERT_EQUAL_UINT8(48, f.humidity);
}

void test_acurite_606tx_no_humidity(void)
{
    SubGhzWeatherFields f;
    TEST_ASSERT_TRUE(subghz_weather_parse(ACURITE_606TX, 0x770FE0FDULL, 32, &f));
    TEST_ASSERT_EQUAL_UINT32(0x77, f.id);
    TEST_ASSERT_EQUAL_INT16(-32, f.temp_d10);
    TEST_ASSERT_EQUAL_UINT8(WX_NO_HUMIDITY, f.humidity);
    TEST_ASSERT_EQUAL_UINT8(WX_NO_CHANNEL, f.channel);
}

void test_acurite_592txr(void)
{
    SubGhzWeatherFields f;
    TEST_ASSERT_TRUE(subghz_weather_parse(ACURITE_592TXR,
                                          0x923444AA09E29FULL, 56, &f));
    TEST_ASSERT_EQUAL_UINT32(0x1234, f.id);
    TEST_ASSERT_EQUAL_UINT8(2, f.channel);
    TEST_ASSERT_EQUAL_UINT8(0, f.battery_low);
    TEST_ASSERT_EQUAL_UINT8(42, f.humidity);
    TEST_ASSERT_EQUAL_INT16(250, f.temp_d10);
}

void test_infactory_fahrenheit_conversion(void)
{
    SubGhzWeatherFields f;
    TEST_ASSERT_TRUE(subghz_weather_parse(INFACTORY, 0x9E24654472ULL, 40, &f));
    TEST_ASSERT_EQUAL_UINT32(0x9E, f.id);
    TEST_ASSERT_EQUAL_UINT8(1, f.battery_low);
    TEST_ASSERT_EQUAL_INT16(222, f.temp_d10);      /* 72.0 F -> 22.2 C */
    TEST_ASSERT_EQUAL_UINT8(47, f.humidity);
    TEST_ASSERT_EQUAL_UINT8(2, f.channel);
}

void test_ambient_weather(void)
{
    SubGhzWeatherFields f;
    TEST_ASSERT_TRUE(subghz_weather_parse(AMBIENT_WEATHER, 0x2B247E3D82ULL, 48, &f));
    TEST_ASSERT_EQUAL_UINT32(0x2B, f.id);
    TEST_ASSERT_EQUAL_UINT8(3, f.channel);
    TEST_ASSERT_EQUAL_INT16(239, f.temp_d10);      /* 75.0 F -> 23.9 C */
    TEST_ASSERT_EQUAL_UINT8(61, f.humidity);
}

void test_thermopro(void)
{
    SubGhzWeatherFields f;
    TEST_ASSERT_TRUE(subghz_weather_parse(THERMOPRO_TX4, 0x1387017A68ULL, 37, &f));
    TEST_ASSERT_EQUAL_UINT32(0xC3, f.id);
    TEST_ASSERT_EQUAL_UINT8(1, f.battery_low);
    TEST_ASSERT_EQUAL_UINT8(1, f.channel);
    TEST_ASSERT_EQUAL_INT16(189, f.temp_d10);
    TEST_ASSERT_EQUAL_UINT8(52, f.humidity);
    /* TX-2 shares the decoder. */
    TEST_ASSERT_TRUE(subghz_weather_parse(THERMOPRO_TX2, 0x1387017A68ULL, 37, &f));
}

void test_thermopro_rejects_bad_type_nibble(void)
{
    SubGhzWeatherFields f;
    TEST_ASSERT_FALSE(subghz_weather_parse(THERMOPRO_TX4,
                                           0x1387017A68ULL ^ (1ULL << 33), 37, &f));
}

void test_auriol_ahfl(void)
{
    SubGhzWeatherFields f;
    TEST_ASSERT_TRUE(subghz_weather_parse(AURIOL_AHFL, 0x1127FC6C13FULL, 42, &f));
    TEST_ASSERT_EQUAL_UINT32(0x44, f.id);
    TEST_ASSERT_EQUAL_UINT8(2, f.channel);
    TEST_ASSERT_EQUAL_INT16(-15, f.temp_d10);
    TEST_ASSERT_EQUAL_UINT8(88, f.humidity);
}

void test_solight_te44(void)
{
    SubGhzWeatherFields f;
    TEST_ASSERT_TRUE(subghz_weather_parse(SOLIGHT_TE44, 0x81B12CF4CULL, 36, &f));
    TEST_ASSERT_EQUAL_UINT32(0x81, f.id);
    TEST_ASSERT_EQUAL_UINT8(4, f.channel);
    TEST_ASSERT_EQUAL_UINT8(0, f.battery_low);
    TEST_ASSERT_EQUAL_INT16(300, f.temp_d10);
    TEST_ASSERT_EQUAL_UINT8(WX_NO_HUMIDITY, f.humidity);
}

void test_kedsum_th(void)
{
    SubGhzWeatherFields f;
    TEST_ASSERT_TRUE(subghz_weather_parse(KEDSUM_TH, 0x2A5AC262402ULL, 42, &f));
    TEST_ASSERT_EQUAL_UINT32(0x2A5, f.id);
    TEST_ASSERT_EQUAL_UINT8(3, f.channel);
    TEST_ASSERT_EQUAL_UINT8(0, f.battery_low);
    TEST_ASSERT_EQUAL_INT16(200, f.temp_d10);      /* 68.0 F -> 20.0 C */
    TEST_ASSERT_EQUAL_UINT8(66, f.humidity);
}

void test_vauno_en8822c(void)
{
    SubGhzWeatherFields f;
    TEST_ASSERT_TRUE(subghz_weather_parse(VAUNO_EN8822C, 0x1707E153800ULL, 42, &f));
    TEST_ASSERT_EQUAL_UINT32(0x5C, f.id);
    TEST_ASSERT_EQUAL_UINT8(1, f.channel);
    TEST_ASSERT_EQUAL_INT16(-123, f.temp_d10);
    TEST_ASSERT_EQUAL_UINT8(39, f.humidity);
}

void test_wendox_w6726(void)
{
    SubGhzWeatherFields f;
    TEST_ASSERT_TRUE(subghz_weather_parse(WENDOX_W6726, 0x5AF0008ULL, 29, &f));
    TEST_ASSERT_EQUAL_INT16(200, f.temp_d10);
    TEST_ASSERT_EQUAL_UINT8(WX_NO_HUMIDITY, f.humidity);
}

void test_bresser_3ch(void)
{
    SubGhzWeatherFields f;
    TEST_ASSERT_TRUE(subghz_weather_parse(BRESSER_3CH, 0x6396862DACULL, 40, &f));
    TEST_ASSERT_EQUAL_UINT32(0x63, f.id);
    TEST_ASSERT_EQUAL_UINT8(1, f.channel);
    TEST_ASSERT_EQUAL_INT16(250, f.temp_d10);      /* 77.0 F -> 25.0 C */
    TEST_ASSERT_EQUAL_UINT8(45, f.humidity);
}

void test_tx_8300(void)
{
    SubGhzWeatherFields f;
    TEST_ASSERT_TRUE(subghz_weather_parse(TX_8300, 0x5812D237ULL, 32, &f));
    TEST_ASSERT_EQUAL_UINT32(0x2D, f.id);
    TEST_ASSERT_EQUAL_UINT8(1, f.channel);
    TEST_ASSERT_EQUAL_INT16(237, f.temp_d10);
    TEST_ASSERT_EQUAL_UINT8(58, f.humidity);
}

/* ------------------------------------------------------------------ */
/* Dispatch / helpers                                                  */
/* ------------------------------------------------------------------ */

void test_unsupported_protocol_returns_false(void)
{
    SubGhzWeatherFields f;
    TEST_ASSERT_FALSE(subghz_weather_parse_supported(PRINCETON));
    TEST_ASSERT_FALSE(subghz_weather_parse(PRINCETON, 0x123456, 24, &f));
    TEST_ASSERT_TRUE(subghz_weather_parse_supported(NEXUS_TH));
}

void test_null_output_is_safe(void)
{
    TEST_ASSERT_FALSE(subghz_weather_parse(NEXUS_TH, 0x5A90EAF37ULL, 36, NULL));
}

void test_fahrenheit_to_celsius_helper(void)
{
    TEST_ASSERT_EQUAL_INT16(0,    subghz_weather_f_to_c_d10(320));
    TEST_ASSERT_EQUAL_INT16(1000, subghz_weather_f_to_c_d10(2120));
    TEST_ASSERT_EQUAL_INT16(-400, subghz_weather_f_to_c_d10(-400));
    TEST_ASSERT_EQUAL_INT16(-178, subghz_weather_f_to_c_d10(0));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_lacrosse_tx141thbv2_real_capture);
    RUN_TEST(test_lacrosse_tx141thbv2_40bit_form);
    RUN_TEST(test_lacrosse_bad_crc_rejected);
    RUN_TEST(test_wrong_bit_length_rejected);
    RUN_TEST(test_nexus_th);
    RUN_TEST(test_nexus_th_requires_const_nibble);
    RUN_TEST(test_gt_wt02_negative_temp);
    RUN_TEST(test_acurite_609txc);
    RUN_TEST(test_acurite_606tx_no_humidity);
    RUN_TEST(test_acurite_592txr);
    RUN_TEST(test_infactory_fahrenheit_conversion);
    RUN_TEST(test_ambient_weather);
    RUN_TEST(test_thermopro);
    RUN_TEST(test_thermopro_rejects_bad_type_nibble);
    RUN_TEST(test_auriol_ahfl);
    RUN_TEST(test_solight_te44);
    RUN_TEST(test_kedsum_th);
    RUN_TEST(test_vauno_en8822c);
    RUN_TEST(test_wendox_w6726);
    RUN_TEST(test_bresser_3ch);
    RUN_TEST(test_tx_8300);
    RUN_TEST(test_unsupported_protocol_returns_false);
    RUN_TEST(test_null_output_is_safe);
    RUN_TEST(test_fahrenheit_to_celsius_helper);
    return UNITY_END();
}
