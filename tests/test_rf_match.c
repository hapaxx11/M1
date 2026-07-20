/* See COPYING.txt for license details. */

/*
 * test_rf_match.c
 *
 * Unit tests for rf_match — the fingerprint × database scoring engine.
 * Verifies the weighted-sum scoring, the band hard-gate, applicable-feature
 * normalization, and the best-match / min-confidence behaviour.
 *
 * Build:
 *   cmake -B build-tests -S tests && cmake --build build-tests
 *   ctest --test-dir build-tests --output-on-failure -R rf_match
 */

#include <string.h>
#include "unity.h"
#include "rf_match.h"

void setUp(void) {}
void tearDown(void) {}

/* A signature the scorer can be tested against directly. */
static rf_protocol_sig_t make_sig(rf_mod_family_t mod, uint16_t bands,
                                  uint16_t te_min, uint16_t te_max,
                                  uint16_t b_min, uint16_t b_max)
{
    rf_protocol_sig_t s;
    memset(&s, 0, sizeof(s));
    s.name = "test"; s.device_note = ""; s.security_note = "";
    s.mod = mod; s.bands = bands;
    s.te_min_us = te_min; s.te_max_us = te_max;
    s.bits_min = b_min; s.bits_max = b_max;
    return s;
}

static rf_fingerprint_t make_fp(uint16_t band, rf_mod_family_t mod,
                                uint16_t te, uint16_t bits)
{
    rf_fingerprint_t fp;
    memset(&fp, 0, sizeof(fp));
    fp.band = band; fp.mod = mod; fp.te_us = te; fp.est_bits = bits;
    return fp;
}

/*============================================================================*/
/* Scoring                                                                    */
/*============================================================================*/

void test_null_inputs_score_zero(void)
{
    rf_protocol_sig_t s = make_sig(RF_MOD_OOK, RF_BAND_433, 100, 500, 0, 0);
    rf_fingerprint_t  f = make_fp(RF_BAND_433, RF_MOD_OOK, 300, 0);
    TEST_ASSERT_EQUAL_UINT8(0, rf_match_score(NULL, &s));
    TEST_ASSERT_EQUAL_UINT8(0, rf_match_score(&f, NULL));
}

void test_full_agreement_scores_100(void)
{
    rf_protocol_sig_t s = make_sig(RF_MOD_OOK, RF_BAND_433, 100, 500, 40, 0);
    rf_fingerprint_t  f = make_fp(RF_BAND_433, RF_MOD_OOK, 300, 64);
    TEST_ASSERT_EQUAL_UINT8(100, rf_match_score(&f, &s));
}

void test_band_contradiction_hard_fails(void)
{
    rf_protocol_sig_t s = make_sig(RF_MOD_OOK, RF_BAND_868, 100, 500, 0, 0);
    rf_fingerprint_t  f = make_fp(RF_BAND_433, RF_MOD_OOK, 300, 0);
    TEST_ASSERT_EQUAL_UINT8(0, rf_match_score(&f, &s));
}

void test_mod_mismatch_lowers_score(void)
{
    /* Band + te + bits agree, modulation does not: earned = 35+0+25+15 = 75
     * of possible 100 => 75. */
    rf_protocol_sig_t s = make_sig(RF_MOD_FSK, RF_BAND_433, 100, 500, 40, 100);
    rf_fingerprint_t  f = make_fp(RF_BAND_433, RF_MOD_OOK, 300, 64);
    TEST_ASSERT_EQUAL_UINT8(75, rf_match_score(&f, &s));
}

void test_timing_out_of_window_lowers_score(void)
{
    /* te 900 outside [100,500]: earned = 35+25+0 = 60 of possible 85 (no bits
     * bounds on sig) => 70. */
    rf_protocol_sig_t s = make_sig(RF_MOD_OOK, RF_BAND_433, 100, 500, 0, 0);
    rf_fingerprint_t  f = make_fp(RF_BAND_433, RF_MOD_OOK, 900, 0);
    TEST_ASSERT_EQUAL_UINT8(70, rf_match_score(&f, &s));
}

void test_data_poor_2p4_band_only(void)
{
    /* 2.4 GHz fingerprint with only band+mod known vs a band+mod-only sig. */
    rf_protocol_sig_t s = make_sig(RF_MOD_FSK, RF_BAND_2400, 0, 0, 0, 0);
    rf_fingerprint_t  f = make_fp(RF_BAND_2400, RF_MOD_FSK, 0, 0);
    TEST_ASSERT_EQUAL_UINT8(100, rf_match_score(&f, &s));
}

void test_no_comparable_features_scores_zero(void)
{
    rf_protocol_sig_t s = make_sig(RF_MOD_UNKNOWN, 0, 0, 0, 0, 0);
    rf_fingerprint_t  f = make_fp(0, RF_MOD_UNKNOWN, 0, 0);
    TEST_ASSERT_EQUAL_UINT8(0, rf_match_score(&f, &s));
}

/*============================================================================*/
/* Best-match against the real database                                       */
/*============================================================================*/

void test_best_match_identifies_car_fob(void)
{
    /* 433 MHz FSK, timing 200us, 64 bits => rolling car fob territory. */
    rf_fingerprint_t f = make_fp(RF_BAND_433, RF_MOD_FSK, 200, 64);
    rf_match_result_t r = rf_match_best(&f);
    TEST_ASSERT_TRUE(r.index >= 0);
    TEST_ASSERT_NOT_NULL(r.sig);
    TEST_ASSERT_TRUE(r.confidence >= RF_MATCH_MIN_CONFIDENCE);
    TEST_ASSERT_TRUE(r.sig->category == RF_CAT_AUTOMOTIVE ||
                     r.sig->category == RF_CAT_INDUSTRIAL);
}

void test_best_match_identifies_ble(void)
{
    rf_fingerprint_t f = make_fp(RF_BAND_2400, RF_MOD_FSK, 0, 0);
    f.sensor = RF_SENSOR_BLE;
    rf_match_result_t r = rf_match_best(&f);
    TEST_ASSERT_TRUE(r.index >= 0);
    TEST_ASSERT_NOT_NULL(r.sig);
    TEST_ASSERT_EQUAL_UINT16(RF_BAND_2400, (uint16_t)(r.sig->bands & RF_BAND_2400));
}

void test_best_match_null_fp(void)
{
    rf_match_result_t r = rf_match_best(NULL);
    TEST_ASSERT_EQUAL_INT(-1, r.index);
    TEST_ASSERT_NULL(r.sig);
}

void test_best_match_no_confident_result(void)
{
    /* An out-of-any-band low-frequency fingerprint should not confidently
     * match anything. */
    rf_fingerprint_t f = make_fp(0, RF_MOD_UNKNOWN, 0, 0);
    rf_match_result_t r = rf_match_best(&f);
    TEST_ASSERT_EQUAL_INT(-1, r.index);
    TEST_ASSERT_NULL(r.sig);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_null_inputs_score_zero);
    RUN_TEST(test_full_agreement_scores_100);
    RUN_TEST(test_band_contradiction_hard_fails);
    RUN_TEST(test_mod_mismatch_lowers_score);
    RUN_TEST(test_timing_out_of_window_lowers_score);
    RUN_TEST(test_data_poor_2p4_band_only);
    RUN_TEST(test_no_comparable_features_scores_zero);

    RUN_TEST(test_best_match_identifies_car_fob);
    RUN_TEST(test_best_match_identifies_ble);
    RUN_TEST(test_best_match_null_fp);
    RUN_TEST(test_best_match_no_confident_result);

    return UNITY_END();
}
