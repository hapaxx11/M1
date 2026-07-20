/* See COPYING.txt for license details. */

/*
 * test_rf_ook_fsk.c
 *
 * Unit tests for rf_ook_fsk — the OOK-vs-FSK classifier from an RSSI burst.
 * Verifies bimodal amplitude-keyed traces read as OOK, flat carriers read as
 * FSK, ambiguous / short / null inputs stay UNKNOWN, and a smooth ramp (not
 * bimodal) is not mistaken for OOK.
 *
 * Build:
 *   cmake -B build-tests -S tests && cmake --build build-tests
 *   ctest --test-dir build-tests --output-on-failure -R rf_ook_fsk
 */

#include "unity.h"
#include "rf_ook_fsk.h"

void setUp(void) {}
void tearDown(void) {}

void test_null_and_short_are_unknown(void)
{
    int16_t few[3] = { -60, -90, -60 };
    rf_ook_fsk_result_t a = rf_ook_fsk_classify(NULL, 32);
    rf_ook_fsk_result_t b = rf_ook_fsk_classify(few, 3);
    TEST_ASSERT_EQUAL_INT(RF_MOD_UNKNOWN, a.mod);
    TEST_ASSERT_EQUAL_UINT8(0, a.confidence);
    TEST_ASSERT_EQUAL_INT(RF_MOD_UNKNOWN, b.mod);
    TEST_ASSERT_EQUAL_UINT8(0, b.confidence);
}

void test_flat_carrier_is_fsk(void)
{
    int16_t flat[16];
    for (int i = 0; i < 16; i++) flat[i] = (int16_t)(-58 + (i & 1)); /* spread 1 */
    rf_ook_fsk_result_t r = rf_ook_fsk_classify(flat, 16);
    TEST_ASSERT_EQUAL_INT(RF_MOD_FSK, r.mod);
    TEST_ASSERT_TRUE(r.confidence >= 2);
    TEST_ASSERT_TRUE(r.spread_db <= RF_OOK_FSK_FSK_SPREAD_DB);
}

void test_bimodal_burst_is_ook(void)
{
    /* Alternating floor / ceiling — classic amplitude keying. */
    int16_t burst[32];
    for (int i = 0; i < 32; i++) burst[i] = (i & 1) ? (int16_t)-45 : (int16_t)-95;
    rf_ook_fsk_result_t r = rf_ook_fsk_classify(burst, 32);
    TEST_ASSERT_EQUAL_INT(RF_MOD_OOK, r.mod);
    TEST_ASSERT_EQUAL_UINT8(3, r.confidence);          /* fully clustered */
    TEST_ASSERT_TRUE(r.spread_db >= RF_OOK_FSK_OOK_SPREAD_DB);
}

void test_smooth_ramp_not_ook(void)
{
    /* Large spread but a monotonic ramp is NOT bimodal → must not be OOK. */
    int16_t ramp[32];
    for (int i = 0; i < 32; i++) ramp[i] = (int16_t)(-95 + i * 2); /* -95..-33 */
    rf_ook_fsk_result_t r = rf_ook_fsk_classify(ramp, 32);
    TEST_ASSERT_NOT_EQUAL_INT(RF_MOD_OOK, r.mod);
}

void test_intermediate_spread_is_unknown(void)
{
    /* Spread ~8 dB: above FSK ceiling, below OOK floor → unknown. */
    int16_t mid[16];
    for (int i = 0; i < 16; i++) mid[i] = (int16_t)(-70 + (i % 4) * 2); /* 0..6 */
    /* Force spread to ~8 without a clean bimodal shape. */
    mid[0] = -74; mid[1] = -66;
    rf_ook_fsk_result_t r = rf_ook_fsk_classify(mid, 16);
    TEST_ASSERT_EQUAL_INT(RF_MOD_UNKNOWN, r.mod);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_null_and_short_are_unknown);
    RUN_TEST(test_flat_carrier_is_fsk);
    RUN_TEST(test_bimodal_burst_is_ook);
    RUN_TEST(test_smooth_ramp_not_ook);
    RUN_TEST(test_intermediate_spread_is_unknown);
    return UNITY_END();
}
