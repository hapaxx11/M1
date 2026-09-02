/* See COPYING.txt for license details. */

/*
 * test_subghz_weather_scan.c
 *
 * Unit tests for the Momentum-style Weather Station RX scan state machine
 * (Sub_Ghz/subghz_weather_scan.c).
 *
 * Bug context:
 *   The Weather Station monitor previously ran only the OOK demodulator on a
 *   fixed 433.92 MHz config, so 2FSK stations (LaCrosse TX141TH-Bv2, Bresser,
 *   Fine Offset) were never demodulated.  The scan module alternates OOK/FSK
 *   on a dwell timer.  These tests pin the dwell/switch behaviour, wrap-around
 *   handling, and single-modulation mode.
 *
 * Build:
 *   cmake -B build-tests -S tests && cmake --build build-tests
 *   ctest --test-dir build-tests --output-on-failure -R subghz_weather_scan
 */

#include "unity.h"
#include "subghz_weather_scan.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/* ------------------------------------------------------------------ */
/* other() / label() basics                                            */
/* ------------------------------------------------------------------ */

void test_other_toggles(void)
{
    TEST_ASSERT_EQUAL(WX_SCAN_MOD_FSK, subghz_weather_scan_other(WX_SCAN_MOD_OOK));
    TEST_ASSERT_EQUAL(WX_SCAN_MOD_OOK, subghz_weather_scan_other(WX_SCAN_MOD_FSK));
}

void test_label_strings(void)
{
    TEST_ASSERT_EQUAL_STRING("AM",  subghz_weather_scan_label(WX_SCAN_MOD_OOK));
    TEST_ASSERT_EQUAL_STRING("FSK", subghz_weather_scan_label(WX_SCAN_MOD_FSK));
}

/* ------------------------------------------------------------------ */
/* init                                                                */
/* ------------------------------------------------------------------ */

void test_init_sets_fields(void)
{
    SubGhzWeatherScan s;
    subghz_weather_scan_init(&s, 4000, WX_SCAN_MOD_OOK, true, 1000);
    TEST_ASSERT_EQUAL_UINT32(4000, s.dwell_ms);
    TEST_ASSERT_EQUAL_UINT32(1000, s.last_switch_ms);
    TEST_ASSERT_EQUAL(WX_SCAN_MOD_OOK, s.mod);
    TEST_ASSERT_TRUE(s.dual);
}

void test_init_clamps_zero_dwell(void)
{
    SubGhzWeatherScan s;
    subghz_weather_scan_init(&s, 0, WX_SCAN_MOD_FSK, true, 0);
    TEST_ASSERT_EQUAL_UINT32(1, s.dwell_ms);
    TEST_ASSERT_EQUAL(WX_SCAN_MOD_FSK, s.mod);
}

void test_init_null_safe(void)
{
    /* Must not crash. */
    subghz_weather_scan_init(NULL, 100, WX_SCAN_MOD_OOK, true, 0);
}

/* ------------------------------------------------------------------ */
/* tick / dwell switching                                              */
/* ------------------------------------------------------------------ */

void test_tick_no_switch_before_dwell(void)
{
    SubGhzWeatherScan s;
    subghz_weather_scan_init(&s, 1000, WX_SCAN_MOD_OOK, true, 0);

    TEST_ASSERT_FALSE(subghz_weather_scan_tick(&s, 0));
    TEST_ASSERT_FALSE(subghz_weather_scan_tick(&s, 500));
    TEST_ASSERT_FALSE(subghz_weather_scan_tick(&s, 999));
    TEST_ASSERT_EQUAL(WX_SCAN_MOD_OOK, s.mod);
}

void test_tick_switches_at_dwell_boundary(void)
{
    SubGhzWeatherScan s;
    subghz_weather_scan_init(&s, 1000, WX_SCAN_MOD_OOK, true, 0);

    /* Exactly at the boundary counts as elapsed. */
    TEST_ASSERT_TRUE(subghz_weather_scan_tick(&s, 1000));
    TEST_ASSERT_EQUAL(WX_SCAN_MOD_FSK, s.mod);
    TEST_ASSERT_EQUAL_UINT32(1000, s.last_switch_ms);
}

void test_tick_alternates_over_time(void)
{
    SubGhzWeatherScan s;
    subghz_weather_scan_init(&s, 1000, WX_SCAN_MOD_OOK, true, 0);

    TEST_ASSERT_TRUE(subghz_weather_scan_tick(&s, 1000));   /* -> FSK */
    TEST_ASSERT_EQUAL(WX_SCAN_MOD_FSK, s.mod);
    TEST_ASSERT_FALSE(subghz_weather_scan_tick(&s, 1500));  /* still FSK */
    TEST_ASSERT_TRUE(subghz_weather_scan_tick(&s, 2000));   /* -> OOK */
    TEST_ASSERT_EQUAL(WX_SCAN_MOD_OOK, s.mod);
    TEST_ASSERT_TRUE(subghz_weather_scan_tick(&s, 3000));   /* -> FSK */
    TEST_ASSERT_EQUAL(WX_SCAN_MOD_FSK, s.mod);
}

void test_tick_handles_tick_wraparound(void)
{
    SubGhzWeatherScan s;
    /* last_switch near the 32-bit tick ceiling; now wraps past 0. */
    subghz_weather_scan_init(&s, 1000, WX_SCAN_MOD_OOK, true, 0xFFFFFC00UL);

    /* 0xFFFFFC00 + 0x600 = wraps to 0x200 -> elapsed 0x400 (1024) >= 1000. */
    TEST_ASSERT_TRUE(subghz_weather_scan_tick(&s, 0x00000200UL));
    TEST_ASSERT_EQUAL(WX_SCAN_MOD_FSK, s.mod);
}

/* ------------------------------------------------------------------ */
/* single-modulation mode                                              */
/* ------------------------------------------------------------------ */

void test_single_mode_never_switches(void)
{
    SubGhzWeatherScan s;
    subghz_weather_scan_init(&s, 1000, WX_SCAN_MOD_FSK, false, 0);

    TEST_ASSERT_FALSE(subghz_weather_scan_tick(&s, 1000));
    TEST_ASSERT_FALSE(subghz_weather_scan_tick(&s, 100000));
    TEST_ASSERT_EQUAL(WX_SCAN_MOD_FSK, s.mod);
}

void test_tick_null_safe(void)
{
    TEST_ASSERT_FALSE(subghz_weather_scan_tick(NULL, 1234));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_other_toggles);
    RUN_TEST(test_label_strings);
    RUN_TEST(test_init_sets_fields);
    RUN_TEST(test_init_clamps_zero_dwell);
    RUN_TEST(test_init_null_safe);
    RUN_TEST(test_tick_no_switch_before_dwell);
    RUN_TEST(test_tick_switches_at_dwell_boundary);
    RUN_TEST(test_tick_alternates_over_time);
    RUN_TEST(test_tick_handles_tick_wraparound);
    RUN_TEST(test_single_mode_never_switches);
    RUN_TEST(test_tick_null_safe);
    return UNITY_END();
}
