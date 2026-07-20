/* See COPYING.txt for license details. */

/*
 * test_rf_sweep.c
 *
 * Unit tests for rf_sweep — the ranked sweep-report aggregator that turns a
 * stream of per-detection rf_match_best() results into the RF Rosetta Signal
 * Identifier's live report.  Verifies reset, counting, dedup/merge, ranking,
 * capacity eviction, and the top accessor.
 *
 * Build:
 *   cmake -B build-tests -S tests && cmake --build build-tests
 *   ctest --test-dir build-tests --output-on-failure -R rf_sweep
 */

#include <string.h>
#include "unity.h"
#include "rf_sweep.h"

void setUp(void) {}
void tearDown(void) {}

/* A stable set of signatures to point matches at (pointer identity is the
 * dedup key, so distinct objects model distinct protocols). */
static rf_protocol_sig_t make_sig(const char *name, rf_category_t cat,
                                  rf_security_t sec)
{
    rf_protocol_sig_t s;
    memset(&s, 0, sizeof(s));
    s.name = name; s.device_note = ""; s.security_note = "";
    s.category = cat; s.security = sec;
    return s;
}

static rf_fingerprint_t make_fp(uint16_t band, uint32_t freq_hz, int16_t rssi)
{
    rf_fingerprint_t fp;
    memset(&fp, 0, sizeof(fp));
    fp.band = band; fp.freq_hz = freq_hz; fp.rssi_dbm = rssi;
    return fp;
}

static rf_match_result_t make_match(const rf_protocol_sig_t *sig, uint8_t conf)
{
    rf_match_result_t m;
    m.index = (sig != NULL) ? 0 : -1;
    m.confidence = conf;
    m.sig = sig;
    return m;
}

/*============================================================================*/
/* Reset / empty                                                              */
/*============================================================================*/

void test_reset_empties_report(void)
{
    rf_sweep_report_t rep;
    memset(&rep, 0xAA, sizeof(rep));
    rf_sweep_report_reset(&rep);
    TEST_ASSERT_EQUAL_UINT8(0, rep.count);
    TEST_ASSERT_EQUAL_UINT32(0, rep.total_detections);
    TEST_ASSERT_EQUAL_UINT32(0, rep.identified_detections);
    TEST_ASSERT_NULL(rf_sweep_report_top(&rep));
}

void test_null_safe(void)
{
    rf_fingerprint_t fp = make_fp(RF_BAND_433, 433920000, -60);
    rf_match_result_t m = make_match(NULL, 0);
    rf_sweep_report_reset(NULL);                      /* no crash */
    TEST_ASSERT_FALSE(rf_sweep_report_add(NULL, &fp, &m));
    TEST_ASSERT_NULL(rf_sweep_report_top(NULL));
}

/*============================================================================*/
/* Counting                                                                   */
/*============================================================================*/

void test_unidentified_counts_but_no_slot(void)
{
    rf_sweep_report_t rep; rf_sweep_report_reset(&rep);
    rf_fingerprint_t fp = make_fp(RF_BAND_433, 433920000, -70);
    rf_match_result_t none = make_match(NULL, 0);

    TEST_ASSERT_FALSE(rf_sweep_report_add(&rep, &fp, &none));
    TEST_ASSERT_FALSE(rf_sweep_report_add(&rep, &fp, NULL));
    TEST_ASSERT_EQUAL_UINT32(2, rep.total_detections);
    TEST_ASSERT_EQUAL_UINT32(0, rep.identified_detections);
    TEST_ASSERT_EQUAL_UINT8(0, rep.count);
}

void test_identified_takes_slot(void)
{
    rf_sweep_report_t rep; rf_sweep_report_reset(&rep);
    rf_protocol_sig_t sig = make_sig("Fob", RF_CAT_AUTOMOTIVE, RF_SEC_ROLLING);
    rf_fingerprint_t fp = make_fp(RF_BAND_433, 433920000, -55);
    rf_match_result_t m = make_match(&sig, 87);

    TEST_ASSERT_TRUE(rf_sweep_report_add(&rep, &fp, &m));
    TEST_ASSERT_EQUAL_UINT8(1, rep.count);
    TEST_ASSERT_EQUAL_UINT32(1, rep.identified_detections);

    const rf_sweep_hit_t *top = rf_sweep_report_top(&rep);
    TEST_ASSERT_NOT_NULL(top);
    TEST_ASSERT_EQUAL_PTR(&sig, top->sig);
    TEST_ASSERT_EQUAL_UINT8(87, top->confidence);
    TEST_ASSERT_EQUAL_UINT16(1, top->hits);
    TEST_ASSERT_EQUAL_UINT32(433920000, top->freq_hz);
    TEST_ASSERT_EQUAL_INT(RF_CAT_AUTOMOTIVE, top->category);
    TEST_ASSERT_EQUAL_INT(RF_SEC_ROLLING, top->security);
}

/*============================================================================*/
/* Merge / dedup                                                              */
/*============================================================================*/

void test_same_sig_same_band_merges(void)
{
    rf_sweep_report_t rep; rf_sweep_report_reset(&rep);
    rf_protocol_sig_t sig = make_sig("Fob", RF_CAT_AUTOMOTIVE, RF_SEC_ROLLING);

    rf_fingerprint_t f1 = make_fp(RF_BAND_433, 433920000, -80);
    rf_fingerprint_t f2 = make_fp(RF_BAND_433, 433880000, -50); /* stronger */
    rf_sweep_report_add(&rep, &f1, &(rf_match_result_t){0, 60, &sig});
    rf_sweep_report_add(&rep, &f2, &(rf_match_result_t){0, 91, &sig});

    TEST_ASSERT_EQUAL_UINT8(1, rep.count);              /* merged, not two */
    const rf_sweep_hit_t *top = rf_sweep_report_top(&rep);
    TEST_ASSERT_EQUAL_UINT16(2, top->hits);
    TEST_ASSERT_EQUAL_UINT8(91, top->confidence);       /* max confidence kept */
    TEST_ASSERT_EQUAL_INT16(-50, top->rssi_dbm);        /* strongest RSSI kept */
    TEST_ASSERT_EQUAL_UINT32(433880000, top->freq_hz);  /* strongest sample freq */
    TEST_ASSERT_EQUAL_UINT32(2, rep.identified_detections);
}

void test_same_sig_different_band_separate(void)
{
    rf_sweep_report_t rep; rf_sweep_report_reset(&rep);
    rf_protocol_sig_t sig = make_sig("X", RF_CAT_IOT, RF_SEC_FIXED);

    rf_fingerprint_t f433 = make_fp(RF_BAND_433, 433920000, -60);
    rf_fingerprint_t f868 = make_fp(RF_BAND_868, 868350000, -60);
    rf_sweep_report_add(&rep, &f433, &(rf_match_result_t){0, 70, &sig});
    rf_sweep_report_add(&rep, &f868, &(rf_match_result_t){0, 70, &sig});

    TEST_ASSERT_EQUAL_UINT8(2, rep.count);              /* distinct bands */
}

/*============================================================================*/
/* Ranking                                                                    */
/*============================================================================*/

void test_ranked_by_confidence_then_rssi(void)
{
    rf_sweep_report_t rep; rf_sweep_report_reset(&rep);
    rf_protocol_sig_t a = make_sig("A", RF_CAT_HOME, RF_SEC_FIXED);
    rf_protocol_sig_t b = make_sig("B", RF_CAT_HOME, RF_SEC_FIXED);
    rf_protocol_sig_t c = make_sig("C", RF_CAT_HOME, RF_SEC_FIXED);

    /* Insert out of order. */
    rf_sweep_report_add(&rep, &(rf_fingerprint_t){.band=RF_BAND_433,.rssi_dbm=-70},
                        &(rf_match_result_t){0, 55, &a});
    rf_sweep_report_add(&rep, &(rf_fingerprint_t){.band=RF_BAND_433,.rssi_dbm=-40},
                        &(rf_match_result_t){0, 90, &b});
    /* Same confidence as b but weaker signal → ranks below b. */
    rf_sweep_report_add(&rep, &(rf_fingerprint_t){.band=RF_BAND_433,.rssi_dbm=-80},
                        &(rf_match_result_t){0, 90, &c});

    TEST_ASSERT_EQUAL_UINT8(3, rep.count);
    TEST_ASSERT_EQUAL_PTR(&b, rep.hits[0].sig);   /* 90 / -40 */
    TEST_ASSERT_EQUAL_PTR(&c, rep.hits[1].sig);   /* 90 / -80 */
    TEST_ASSERT_EQUAL_PTR(&a, rep.hits[2].sig);   /* 55 */
    TEST_ASSERT_EQUAL_PTR(&b, rf_sweep_report_top(&rep)->sig);
}

/*============================================================================*/
/* Capacity                                                                   */
/*============================================================================*/

void test_full_report_evicts_weakest_only_if_better(void)
{
    rf_sweep_report_t rep; rf_sweep_report_reset(&rep);
    static rf_protocol_sig_t sigs[RF_SWEEP_MAX_HITS];

    /* Fill every slot with ascending confidence 10,20,...  (distinct sigs). */
    for (uint8_t i = 0; i < RF_SWEEP_MAX_HITS; i++) {
        sigs[i] = make_sig("s", RF_CAT_MISC, RF_SEC_UNKNOWN);
        rf_fingerprint_t fp = make_fp(RF_BAND_433, 0, 0);
        rf_match_result_t m = make_match(&sigs[i], (uint8_t)(10 * (i + 1)));
        TEST_ASSERT_TRUE(rf_sweep_report_add(&rep, &fp, &m));
    }
    TEST_ASSERT_EQUAL_UINT8(RF_SWEEP_MAX_HITS, rep.count);

    /* A weak newcomer (below the current weakest = 10) is rejected. */
    rf_protocol_sig_t weak = make_sig("weak", RF_CAT_MISC, RF_SEC_UNKNOWN);
    rf_fingerprint_t fpw = make_fp(RF_BAND_433, 0, 0);
    TEST_ASSERT_FALSE(rf_sweep_report_add(&rep, &fpw,
                                          &(rf_match_result_t){0, 5, &weak}));
    TEST_ASSERT_EQUAL_UINT8(RF_SWEEP_MAX_HITS, rep.count);

    /* A strong newcomer evicts the weakest and lands at the top. */
    rf_protocol_sig_t strong = make_sig("strong", RF_CAT_MISC, RF_SEC_UNKNOWN);
    rf_fingerprint_t fps = make_fp(RF_BAND_433, 0, -30);
    TEST_ASSERT_TRUE(rf_sweep_report_add(&rep, &fps,
                                         &(rf_match_result_t){0, 99, &strong}));
    TEST_ASSERT_EQUAL_UINT8(RF_SWEEP_MAX_HITS, rep.count);
    TEST_ASSERT_EQUAL_PTR(&strong, rf_sweep_report_top(&rep)->sig);

    /* The evicted one (confidence 10) must be gone. */
    for (uint8_t i = 0; i < rep.count; i++)
        TEST_ASSERT_NOT_EQUAL(10, rep.hits[i].confidence);
}

/*============================================================================*/
/* Runner                                                                     */
/*============================================================================*/

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_reset_empties_report);
    RUN_TEST(test_null_safe);
    RUN_TEST(test_unidentified_counts_but_no_slot);
    RUN_TEST(test_identified_takes_slot);
    RUN_TEST(test_same_sig_same_band_merges);
    RUN_TEST(test_same_sig_different_band_separate);
    RUN_TEST(test_ranked_by_confidence_then_rssi);
    RUN_TEST(test_full_report_evicts_weakest_only_if_better);
    return UNITY_END();
}
