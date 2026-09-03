/* See COPYING.txt for license details. */

/*
 * test_subghz_weather_history.c
 *
 * Unit tests for the Weather Station sensor list
 * (Sub_Ghz/subghz_weather_history.c).
 *
 * Bug context:
 *   The Weather Station scene previously kept only the single most recent
 *   decode in a scratch struct, so multiple sensors overwrote each other and
 *   there was nothing to select from.  This module keeps one row per physical
 *   sensor (protocol + serial + channel), refreshed in place, which the scene
 *   renders as a Flipper-style list.  These tests pin the matching, eviction,
 *   ageing and unit-conversion behaviour.
 *
 * Build:
 *   cmake -B build-tests -S tests && cmake --build build-tests
 *   ctest --test-dir build-tests --output-on-failure -R subghz_weather_history
 */

#include "unity.h"
#include "subghz_weather_history.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static SubGhzWeatherSensor mk(uint16_t proto, uint32_t serial, uint8_t ch,
                              int16_t temp_d10)
{
    SubGhzWeatherSensor s;
    memset(&s, 0, sizeof(s));
    s.protocol = proto;
    s.serial   = serial;
    s.channel  = ch;
    s.temp_raw = temp_d10;
    s.has_temp = true;
    s.humidity = 50;
    return s;
}

/* ------------------------------------------------------------------ */
/* Insertion / matching                                                */
/* ------------------------------------------------------------------ */

void test_reset_clears(void)
{
    SubGhzWeatherHistory h;
    SubGhzWeatherSensor s = mk(1, 0x67, 1, 215);
    subghz_weather_history_reset(&h);
    TEST_ASSERT_EQUAL_UINT8(0, h.count);
    subghz_weather_history_add(&h, &s, 0);
    TEST_ASSERT_EQUAL_UINT8(1, h.count);
    subghz_weather_history_reset(&h);
    TEST_ASSERT_EQUAL_UINT8(0, h.count);
}

void test_null_safe(void)
{
    SubGhzWeatherHistory h;
    SubGhzWeatherSensor s = mk(1, 1, 1, 0);
    subghz_weather_history_reset(NULL);
    subghz_weather_history_reset(&h);
    TEST_ASSERT_EQUAL_INT(-1, subghz_weather_history_add(NULL, &s, 0));
    TEST_ASSERT_EQUAL_INT(-1, subghz_weather_history_add(&h, NULL, 0));
    TEST_ASSERT_NULL(subghz_weather_history_get(NULL, 0));
    TEST_ASSERT_NULL(subghz_weather_history_get(&h, 0));
    TEST_ASSERT_EQUAL_UINT8(0, subghz_weather_history_age_min(NULL, 1000));
}

void test_same_sensor_updates_in_place(void)
{
    SubGhzWeatherHistory h;
    SubGhzWeatherSensor a = mk(7, 0x67, 1, 215);
    SubGhzWeatherSensor b = mk(7, 0x67, 1, 230);
    subghz_weather_history_reset(&h);

    TEST_ASSERT_EQUAL_INT(0, subghz_weather_history_add(&h, &a, 1000));
    TEST_ASSERT_EQUAL_INT(0, subghz_weather_history_add(&h, &b, 61000));

    TEST_ASSERT_EQUAL_UINT8(1, h.count);
    const SubGhzWeatherSensor *r = subghz_weather_history_get(&h, 0);
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_EQUAL_INT16(230, r->temp_raw);
    TEST_ASSERT_EQUAL_UINT16(2, r->count);
    TEST_ASSERT_EQUAL_UINT32(61000, r->last_seen_ms);
}

void test_distinct_sensors_get_own_rows(void)
{
    SubGhzWeatherHistory h;
    SubGhzWeatherSensor a = mk(7, 0x67, 1, 215);
    SubGhzWeatherSensor b = mk(7, 0x67, 2, 215);   /* other channel */
    SubGhzWeatherSensor c = mk(7, 0x68, 1, 215);   /* other serial  */
    SubGhzWeatherSensor d = mk(8, 0x67, 1, 215);   /* other proto   */
    subghz_weather_history_reset(&h);

    TEST_ASSERT_EQUAL_INT(0, subghz_weather_history_add(&h, &a, 0));
    TEST_ASSERT_EQUAL_INT(1, subghz_weather_history_add(&h, &b, 0));
    TEST_ASSERT_EQUAL_INT(2, subghz_weather_history_add(&h, &c, 0));
    TEST_ASSERT_EQUAL_INT(3, subghz_weather_history_add(&h, &d, 0));
    TEST_ASSERT_EQUAL_UINT8(4, h.count);
}

void test_row_order_is_stable_on_refresh(void)
{
    SubGhzWeatherHistory h;
    SubGhzWeatherSensor a = mk(1, 1, 1, 100);
    SubGhzWeatherSensor b = mk(1, 2, 1, 200);
    subghz_weather_history_reset(&h);
    subghz_weather_history_add(&h, &a, 0);
    subghz_weather_history_add(&h, &b, 0);

    /* Refreshing the first sensor must not move it to the top/bottom —
     * the user may be scrolling the list. */
    TEST_ASSERT_EQUAL_INT(0, subghz_weather_history_add(&h, &a, 5000));
    TEST_ASSERT_EQUAL_UINT32(1, subghz_weather_history_get(&h, 0)->serial);
    TEST_ASSERT_EQUAL_UINT32(2, subghz_weather_history_get(&h, 1)->serial);
}

void test_full_table_evicts_least_recently_seen(void)
{
    SubGhzWeatherHistory h;
    uint8_t i;
    subghz_weather_history_reset(&h);

    for (i = 0; i < WX_HISTORY_MAX; i++) {
        SubGhzWeatherSensor s = mk(1, (uint32_t)(i + 1), 1, 100);
        /* Row 0 is the oldest (t=1000), each next one newer. */
        subghz_weather_history_add(&h, &s, 1000U + i * 1000U);
    }
    TEST_ASSERT_EQUAL_UINT8(WX_HISTORY_MAX, h.count);

    SubGhzWeatherSensor fresh = mk(1, 0xAA, 1, 100);
    int idx = subghz_weather_history_add(&h, &fresh, 100000U);
    TEST_ASSERT_EQUAL_INT(0, idx);   /* oldest row replaced */
    TEST_ASSERT_EQUAL_UINT8(WX_HISTORY_MAX, h.count);
    TEST_ASSERT_EQUAL_UINT32(0xAA, subghz_weather_history_get(&h, 0)->serial);
    TEST_ASSERT_EQUAL_UINT16(1, subghz_weather_history_get(&h, 0)->count);
}

/* ------------------------------------------------------------------ */
/* Ageing                                                              */
/* ------------------------------------------------------------------ */

void test_age_minutes(void)
{
    SubGhzWeatherSensor s = mk(1, 1, 1, 0);
    s.last_seen_ms = 10000U;
    TEST_ASSERT_EQUAL_UINT8(0, subghz_weather_history_age_min(&s, 10000U));
    TEST_ASSERT_EQUAL_UINT8(0, subghz_weather_history_age_min(&s, 69999U));
    TEST_ASSERT_EQUAL_UINT8(1, subghz_weather_history_age_min(&s, 70000U));
    TEST_ASSERT_EQUAL_UINT8(5, subghz_weather_history_age_min(&s, 310000U));
}

void test_age_saturates_at_99(void)
{
    SubGhzWeatherSensor s = mk(1, 1, 1, 0);
    s.last_seen_ms = 0;
    TEST_ASSERT_EQUAL_UINT8(99, subghz_weather_history_age_min(&s, 100U * 60000U));
    TEST_ASSERT_EQUAL_UINT8(99, subghz_weather_history_age_min(&s, 0xF0000000U));
}

void test_age_handles_tick_wraparound(void)
{
    SubGhzWeatherSensor s = mk(1, 1, 1, 0);
    s.last_seen_ms = 0xFFFFF000U;
    /* 0x1000 (4096 ms) before wrap + 60000-4096 ms after == 1 minute. */
    TEST_ASSERT_EQUAL_UINT8(0, subghz_weather_history_age_min(&s, 0x00000FFFU));
    TEST_ASSERT_EQUAL_UINT8(1, subghz_weather_history_age_min(&s, 60000U - 0x1000U));
}

void test_eviction_handles_tick_wraparound(void)
{
    SubGhzWeatherHistory h;
    uint8_t i;
    subghz_weather_history_reset(&h);

    for (i = 0; i < WX_HISTORY_MAX; i++) {
        SubGhzWeatherSensor s = mk(1, (uint32_t)(i + 1), 1, 100);
        subghz_weather_history_add(&h, &s, 0xFFFFF000U + i * 100U);
    }
    /* "now" has wrapped past zero — the row seen at 0xFFFFF000 is oldest. */
    SubGhzWeatherSensor fresh = mk(1, 0xBB, 1, 100);
    TEST_ASSERT_EQUAL_INT(0, subghz_weather_history_add(&h, &fresh, 0x00001000U));
    TEST_ASSERT_EQUAL_UINT32(0xBB, subghz_weather_history_get(&h, 0)->serial);
}

/* ------------------------------------------------------------------ */
/* Unit conversion                                                     */
/* ------------------------------------------------------------------ */

void test_celsius_to_fahrenheit(void)
{
    TEST_ASSERT_EQUAL_INT16(320,  subghz_weather_c_to_f_d10(0));
    TEST_ASSERT_EQUAL_INT16(1000, subghz_weather_c_to_f_d10(378));   /* 37.8C */
    TEST_ASSERT_EQUAL_INT16(212,  subghz_weather_c_to_f_d10(-60));   /* -6.0C */
    TEST_ASSERT_EQUAL_INT16(-400, subghz_weather_c_to_f_d10(-400));  /* -40C  */
    TEST_ASSERT_EQUAL_INT16(2120, subghz_weather_c_to_f_d10(1000));  /* 100C  */
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_reset_clears);
    RUN_TEST(test_null_safe);
    RUN_TEST(test_same_sensor_updates_in_place);
    RUN_TEST(test_distinct_sensors_get_own_rows);
    RUN_TEST(test_row_order_is_stable_on_refresh);
    RUN_TEST(test_full_table_evicts_least_recently_seen);
    RUN_TEST(test_age_minutes);
    RUN_TEST(test_age_saturates_at_99);
    RUN_TEST(test_age_handles_tick_wraparound);
    RUN_TEST(test_eviction_handles_tick_wraparound);
    RUN_TEST(test_celsius_to_fahrenheit);
    return UNITY_END();
}
