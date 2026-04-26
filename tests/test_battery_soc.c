/*
 * test_battery_soc.c — Unit tests for battery_voltage_to_soc()
 *
 * Pure-logic function: piecewise-linear voltage → SoC estimation.
 * No hardware dependencies.
 *
 * Regression context: "Asian M1" hardware variant reports 0% battery even
 * while the device is still fully functional.  The voltage-based correction
 * prevents a spuriously low SoC from blocking firmware updates.
 */

#include "unity.h"
#include "battery_soc_estimate.h"

void setUp(void)  {}
void tearDown(void) {}

/* ---- Boundary / clamp tests ---- */

void test_soc_at_full_voltage_returns_100(void)
{
    TEST_ASSERT_EQUAL_UINT8(100u, battery_voltage_to_soc(4200u));
}

void test_soc_above_full_voltage_clamps_to_100(void)
{
    TEST_ASSERT_EQUAL_UINT8(100u, battery_voltage_to_soc(4300u));
    TEST_ASSERT_EQUAL_UINT8(100u, battery_voltage_to_soc(5000u));
}

void test_soc_at_terminate_voltage_returns_0(void)
{
    TEST_ASSERT_EQUAL_UINT8(0u, battery_voltage_to_soc(3200u));
}

void test_soc_below_terminate_voltage_clamps_to_0(void)
{
    TEST_ASSERT_EQUAL_UINT8(0u, battery_voltage_to_soc(3100u));
    TEST_ASSERT_EQUAL_UINT8(0u, battery_voltage_to_soc(0u));
}

/* ---- Breakpoint exact-match tests ---- */

void test_soc_at_4000mv_returns_85(void)
{
    TEST_ASSERT_EQUAL_UINT8(85u, battery_voltage_to_soc(4000u));
}

void test_soc_at_3800mv_returns_60(void)
{
    TEST_ASSERT_EQUAL_UINT8(60u, battery_voltage_to_soc(3800u));
}

void test_soc_at_3600mv_returns_35(void)
{
    TEST_ASSERT_EQUAL_UINT8(35u, battery_voltage_to_soc(3600u));
}

void test_soc_at_3400mv_returns_15(void)
{
    TEST_ASSERT_EQUAL_UINT8(15u, battery_voltage_to_soc(3400u));
}

/* ---- Interpolation tests ---- */

void test_soc_midpoint_4000_to_4200mv(void)
{
    /* Midpoint between 4000 (85%) and 4200 (100%) → 4100 mV
     * expected = 85 + (4100-4000)*(100-85)/(4200-4000) = 85 + 100*15/200 = 85+7 = 92 */
    TEST_ASSERT_EQUAL_UINT8(92u, battery_voltage_to_soc(4100u));
}

void test_soc_midpoint_3800_to_4000mv(void)
{
    /* Midpoint between 3800 (60%) and 4000 (85%) → 3900 mV
     * expected = 60 + (3900-3800)*(85-60)/(4000-3800) = 60 + 100*25/200 = 60+12 = 72 */
    TEST_ASSERT_EQUAL_UINT8(72u, battery_voltage_to_soc(3900u));
}

void test_soc_midpoint_3600_to_3800mv(void)
{
    /* Midpoint between 3600 (35%) and 3800 (60%) → 3700 mV
     * expected = 35 + (3700-3600)*(60-35)/(3800-3600) = 35 + 100*25/200 = 35+12 = 47 */
    TEST_ASSERT_EQUAL_UINT8(47u, battery_voltage_to_soc(3700u));
}

void test_soc_midpoint_3400_to_3600mv(void)
{
    /* Midpoint between 3400 (15%) and 3600 (35%) → 3500 mV
     * expected = 15 + (3500-3400)*(35-15)/(3600-3400) = 15 + 100*20/200 = 15+10 = 25 */
    TEST_ASSERT_EQUAL_UINT8(25u, battery_voltage_to_soc(3500u));
}

void test_soc_midpoint_3200_to_3400mv(void)
{
    /* Midpoint between 3200 (0%) and 3400 (15%) → 3300 mV
     * expected = 0 + (3300-3200)*(15-0)/(3400-3200) = 0 + 100*15/200 = 7 */
    TEST_ASSERT_EQUAL_UINT8(7u, battery_voltage_to_soc(3300u));
}

/* ---- Monotonicity: higher voltage → higher SoC ---- */

void test_soc_monotonically_increasing(void)
{
    uint8_t prev = battery_voltage_to_soc(3200u);
    for (uint16_t v = 3250u; v <= 4200u; v += 50u) {
        uint8_t cur = battery_voltage_to_soc(v);
        TEST_ASSERT_TRUE_MESSAGE(cur >= prev, "SoC not monotonically increasing with voltage");
        prev = cur;
    }
}

/* ---- Key use-case: device at 0% gauge but high voltage ---- */

void test_soc_3500mv_well_above_zero(void)
{
    /* A device reporting 3500 mV should not be at 0%.
     * The voltage-based estimate must exceed BATT_SOC_VOLTAGE_FALLBACK_THRESHOLD. */
    uint8_t soc = battery_voltage_to_soc(3500u);
    TEST_ASSERT_TRUE(soc > BATT_SOC_VOLTAGE_FALLBACK_THRESHOLD);
}

void test_soc_3700mv_above_firmware_update_threshold(void)
{
    const uint8_t fw_update_min_pct = 25u;

    /* 3700 mV is a healthy battery; the estimated SoC must clear the minimum
     * needed for firmware updates. */
    uint8_t soc = battery_voltage_to_soc(3700u);
    TEST_ASSERT_TRUE(soc >= fw_update_min_pct);
}

/* ---- battery_soc_smooth() — rate-limiter tests ---- */

void test_smooth_no_change(void)
{
    /* Same value → no movement */
    TEST_ASSERT_EQUAL_UINT8(50u, battery_soc_smooth(50u, 50u));
}

void test_smooth_increase_within_limit(void)
{
    /* raw exactly at limit above displayed → snap to raw */
    TEST_ASSERT_EQUAL_UINT8(51u, battery_soc_smooth(50u, 51u));
}

void test_smooth_increase_exceeds_limit(void)
{
    /* raw > displayed + BATT_DISPLAY_MAX_DELTA_PCT → clamp to displayed+1 */
    TEST_ASSERT_EQUAL_UINT8(51u, battery_soc_smooth(50u, 55u));
    TEST_ASSERT_EQUAL_UINT8(21u, battery_soc_smooth(20u, 28u)); /* large jump, e.g. plug-in */
}

void test_smooth_decrease_within_limit(void)
{
    /* raw exactly at limit below displayed → snap to raw */
    TEST_ASSERT_EQUAL_UINT8(49u, battery_soc_smooth(50u, 49u));
}

void test_smooth_decrease_exceeds_limit(void)
{
    /* raw < displayed - BATT_DISPLAY_MAX_DELTA_PCT → clamp to displayed-1 */
    TEST_ASSERT_EQUAL_UINT8(49u, battery_soc_smooth(50u, 45u));
}

void test_smooth_convergence_from_below(void)
{
    /* Simulate gauge jumping from 20 to 28: 8 update calls → displayed reaches 28 */
    uint8_t displayed = 20u;
    const uint8_t raw = 28u;
    for (int i = 0; i < 8; i++)
        displayed = battery_soc_smooth(displayed, raw);
    TEST_ASSERT_EQUAL_UINT8(28u, displayed);
}

void test_smooth_convergence_from_above(void)
{
    /* Simulate gauge dropping from 50 to 45: 5 update calls → displayed reaches 45 */
    uint8_t displayed = 50u;
    const uint8_t raw = 45u;
    for (int i = 0; i < 5; i++)
        displayed = battery_soc_smooth(displayed, raw);
    TEST_ASSERT_EQUAL_UINT8(45u, displayed);
}

void test_smooth_boundary_zero(void)
{
    /* Never goes below 0 */
    TEST_ASSERT_EQUAL_UINT8(0u, battery_soc_smooth(0u, 0u));
    TEST_ASSERT_EQUAL_UINT8(0u, battery_soc_smooth(1u, 0u));
}

void test_smooth_boundary_hundred(void)
{
    /* Never goes above 100 */
    TEST_ASSERT_EQUAL_UINT8(100u, battery_soc_smooth(100u, 100u));
    TEST_ASSERT_EQUAL_UINT8(100u, battery_soc_smooth(99u, 100u));
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_soc_at_full_voltage_returns_100);
    RUN_TEST(test_soc_above_full_voltage_clamps_to_100);
    RUN_TEST(test_soc_at_terminate_voltage_returns_0);
    RUN_TEST(test_soc_below_terminate_voltage_clamps_to_0);
    RUN_TEST(test_soc_at_4000mv_returns_85);
    RUN_TEST(test_soc_at_3800mv_returns_60);
    RUN_TEST(test_soc_at_3600mv_returns_35);
    RUN_TEST(test_soc_at_3400mv_returns_15);
    RUN_TEST(test_soc_midpoint_4000_to_4200mv);
    RUN_TEST(test_soc_midpoint_3800_to_4000mv);
    RUN_TEST(test_soc_midpoint_3600_to_3800mv);
    RUN_TEST(test_soc_midpoint_3400_to_3600mv);
    RUN_TEST(test_soc_midpoint_3200_to_3400mv);
    RUN_TEST(test_soc_monotonically_increasing);
    RUN_TEST(test_soc_3500mv_well_above_zero);
    RUN_TEST(test_soc_3700mv_above_firmware_update_threshold);

    RUN_TEST(test_smooth_no_change);
    RUN_TEST(test_smooth_increase_within_limit);
    RUN_TEST(test_smooth_increase_exceeds_limit);
    RUN_TEST(test_smooth_decrease_within_limit);
    RUN_TEST(test_smooth_decrease_exceeds_limit);
    RUN_TEST(test_smooth_convergence_from_below);
    RUN_TEST(test_smooth_convergence_from_above);
    RUN_TEST(test_smooth_boundary_zero);
    RUN_TEST(test_smooth_boundary_hundred);

    return UNITY_END();
}
