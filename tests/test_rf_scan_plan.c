/* See COPYING.txt for license details. */

/*
 * test_rf_scan_plan.c
 *
 * Unit tests for rf_scan_plan — the Signal Identifier probe plan + cursor.
 * Verifies the plan is non-empty, every point maps to a real band, the
 * 915-style flag matches the boundary rule, and the cursor steps and wraps
 * with a pass counter.
 *
 * Build:
 *   cmake -B build-tests -S tests && cmake --build build-tests
 *   ctest --test-dir build-tests --output-on-failure -R rf_scan_plan
 */

#include "unity.h"
#include "rf_scan_plan.h"
#include "rf_fingerprint.h"   /* rf_band_from_freq */

void setUp(void) {}
void tearDown(void) {}

void test_plan_non_empty(void)
{
    TEST_ASSERT_TRUE(rf_scan_plan_count() > 0);
    TEST_ASSERT_NOT_NULL(rf_scan_plan_point(0));
    TEST_ASSERT_NULL(rf_scan_plan_point(rf_scan_plan_count()));
}

void test_every_point_maps_to_a_band(void)
{
    for (uint16_t i = 0; i < rf_scan_plan_count(); i++) {
        const rf_scan_point_t *p = rf_scan_plan_point(i);
        TEST_ASSERT_NOT_NULL(p);
        TEST_ASSERT_NOT_NULL(p->label);
        /* Every probe frequency must fall inside a known Sub-GHz band. */
        TEST_ASSERT_NOT_EQUAL_UINT16(0, rf_band_from_freq(p->freq_hz));
    }
}

void test_use_915_matches_boundary(void)
{
    for (uint16_t i = 0; i < rf_scan_plan_count(); i++) {
        const rf_scan_point_t *p = rf_scan_plan_point(i);
        bool expect = (p->freq_hz >= RF_SCAN_915_BOUNDARY_HZ);
        TEST_ASSERT_EQUAL_INT(expect, p->use_915);
    }
}

void test_cursor_reset_and_point(void)
{
    rf_scan_cursor_t cur;
    rf_scan_cursor_reset(&cur);
    TEST_ASSERT_EQUAL_UINT16(0, cur.idx);
    TEST_ASSERT_EQUAL_UINT32(0, cur.pass);
    TEST_ASSERT_EQUAL_PTR(rf_scan_plan_point(0), rf_scan_cursor_point(&cur));
}

void test_cursor_advances_and_wraps(void)
{
    rf_scan_cursor_t cur;
    rf_scan_cursor_reset(&cur);
    uint16_t n = rf_scan_plan_count();

    /* Advancing n-1 times stays within the pass. */
    for (uint16_t i = 0; i < n - 1; i++) {
        bool wrapped = rf_scan_cursor_advance(&cur);
        TEST_ASSERT_FALSE(wrapped);
        TEST_ASSERT_EQUAL_UINT16(i + 1, cur.idx);
        TEST_ASSERT_EQUAL_UINT32(0, cur.pass);
    }
    /* The nth advance wraps and bumps the pass counter. */
    bool wrapped = rf_scan_cursor_advance(&cur);
    TEST_ASSERT_TRUE(wrapped);
    TEST_ASSERT_EQUAL_UINT16(0, cur.idx);
    TEST_ASSERT_EQUAL_UINT32(1, cur.pass);
    TEST_ASSERT_EQUAL_PTR(rf_scan_plan_point(0), rf_scan_cursor_point(&cur));
}

void test_cursor_null_safe(void)
{
    rf_scan_cursor_reset(NULL);                       /* no crash */
    TEST_ASSERT_NULL(rf_scan_cursor_point(NULL));
    TEST_ASSERT_FALSE(rf_scan_cursor_advance(NULL));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_plan_non_empty);
    RUN_TEST(test_every_point_maps_to_a_band);
    RUN_TEST(test_use_915_matches_boundary);
    RUN_TEST(test_cursor_reset_and_point);
    RUN_TEST(test_cursor_advances_and_wraps);
    RUN_TEST(test_cursor_null_safe);
    return UNITY_END();
}
