/* See COPYING.txt for license details. */

/**
 * @file   test_proto_pirate_timing.c
 * @brief  Unit tests for the Proto Pirate timing-analysis pure-logic module.
 *
 * Tests pptime_analyze() and pptime_match() against synthetic pulse arrays
 * so the timing logic can be verified without hardware.
 */

#include "unity.h"
#include "subghz_proto_pirate_timing.h"
#include <string.h>
#include <stdint.h>

void setUp(void)    { }
void tearDown(void) { }

/* ---- Helpers ------------------------------------------------------------- */

/** Build a synthetic pulse array with `n_short` short pulses and
 *  `n_long` long pulses, alternating, each exactly the given duration. */
static void make_pulses(uint16_t *buf, uint16_t n_short, uint16_t dur_short,
                        uint16_t n_long, uint16_t dur_long)
{
    uint16_t idx = 0;
    uint16_t si = 0, li = 0;
    while (si < n_short || li < n_long)
    {
        if (si < n_short) { buf[idx++] = dur_short; si++; }
        if (li < n_long)  { buf[idx++] = dur_long;  li++; }
    }
}

/* ---- pptime_analyze tests ------------------------------------------------ */

void test_analyze_empty_returns_zeros(void)
{
    pptime_stats_t stats;
    pptime_analyze(NULL, 0, NULL, &stats);
    TEST_ASSERT_EQUAL(0, (int)stats.n_short);
    TEST_ASSERT_EQUAL(0, (int)stats.n_long);
    TEST_ASSERT_EQUAL(0, (int)stats.n_total);
}

void test_analyze_null_out_no_crash(void)
{
    uint16_t buf[4] = { 400, 800, 400, 800 };
    pptime_analyze(buf, 4, NULL, NULL);  /* must not crash */
}

void test_analyze_no_ref_uses_defaults(void)
{
    /* 6 short pulses at 400 µs, 6 long pulses at 800 µs — fall within default
     * valid range (50–1250 µs) and threshold (≈600 µs) */
    uint16_t buf[12];
    make_pulses(buf, 6, 400, 6, 800);

    pptime_stats_t stats;
    pptime_analyze(buf, 12, NULL, &stats);

    TEST_ASSERT_EQUAL(6, (int)stats.n_short);
    TEST_ASSERT_EQUAL(6, (int)stats.n_long);
    TEST_ASSERT_EQUAL(400, (int)stats.avg_short);
    TEST_ASSERT_EQUAL(800, (int)stats.avg_long);
}

void test_analyze_with_keeloq_ref(void)
{
    /* KeeLoq: te_short=400, te_long=800, te_delta=100 */
    pptime_proto_ref_t ref = { "KeeLoq", 400, 800, 100 };

    uint16_t buf[20];
    make_pulses(buf, 10, 400, 10, 800);

    pptime_stats_t stats;
    pptime_analyze(buf, 20, &ref, &stats);

    TEST_ASSERT_EQUAL(10, (int)stats.n_short);
    TEST_ASSERT_EQUAL(10, (int)stats.n_long);
    TEST_ASSERT_EQUAL(400, (int)stats.avg_short);
    TEST_ASSERT_EQUAL(800, (int)stats.avg_long);
    TEST_ASSERT_EQUAL(400, (int)stats.min_short);
    TEST_ASSERT_EQUAL(400, (int)stats.max_short);
    TEST_ASSERT_EQUAL(800, (int)stats.min_long);
    TEST_ASSERT_EQUAL(800, (int)stats.max_long);
}

void test_analyze_filters_noise_below_min_valid(void)
{
    /* Noise pulses at 10 µs — well below min_valid for any ref */
    pptime_proto_ref_t ref = { "Test", 400, 800, 100 };
    uint16_t buf[4] = { 10, 10, 10, 10 };

    pptime_stats_t stats;
    pptime_analyze(buf, 4, &ref, &stats);

    TEST_ASSERT_EQUAL(0, (int)stats.n_total);
}

void test_analyze_filters_noise_above_max_valid(void)
{
    /* Gaps at 5000 µs — above max_valid (800 + 2*100 = 1000) */
    pptime_proto_ref_t ref = { "Test", 400, 800, 100 };
    uint16_t buf[4] = { 5000, 5000, 5000, 5000 };

    pptime_stats_t stats;
    pptime_analyze(buf, 4, &ref, &stats);

    TEST_ASSERT_EQUAL(0, (int)stats.n_total);
}

void test_analyze_min_max_tracking(void)
{
    pptime_proto_ref_t ref = { "KeeLoq", 400, 800, 150 };

    /* short: 350, 380, 420 — long: 750, 800, 850 */
    uint16_t buf[] = { 350, 750, 380, 800, 420, 850 };

    pptime_stats_t stats;
    pptime_analyze(buf, 6, &ref, &stats);

    TEST_ASSERT_EQUAL(3, (int)stats.n_short);
    TEST_ASSERT_EQUAL(3, (int)stats.n_long);
    TEST_ASSERT_EQUAL(350, (int)stats.min_short);
    TEST_ASSERT_EQUAL(420, (int)stats.max_short);
    TEST_ASSERT_EQUAL(750, (int)stats.min_long);
    TEST_ASSERT_EQUAL(850, (int)stats.max_long);

    /* avg_short = (350+380+420)/3 = 383 */
    TEST_ASSERT_INT_WITHIN(1, 383, (int)stats.avg_short);
    /* avg_long  = (750+800+850)/3 = 800 */
    TEST_ASSERT_EQUAL(800, (int)stats.avg_long);
}

/* ---- pptime_match tests -------------------------------------------------- */

void test_match_no_data_when_empty_stats(void)
{
    pptime_proto_ref_t ref = { "KeeLoq", 400, 800, 100 };
    pptime_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    pptime_match_result_t r = pptime_match(&stats, &ref);
    TEST_ASSERT_EQUAL(PPTIME_MATCH_NO_DATA, r);
}

void test_match_no_data_when_few_samples(void)
{
    pptime_proto_ref_t ref = { "KeeLoq", 400, 800, 100 };
    pptime_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    stats.n_short = 2;
    stats.n_long  = 2;
    stats.avg_short = 400;
    stats.avg_long  = 800;

    pptime_match_result_t r = pptime_match(&stats, &ref);
    TEST_ASSERT_EQUAL(PPTIME_MATCH_NO_DATA, r);
}

void test_match_ok_within_tolerance(void)
{
    pptime_proto_ref_t ref = { "KeeLoq", 400, 800, 100 };
    pptime_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    stats.n_short = 10;  stats.avg_short = 410;
    stats.n_long  = 10;  stats.avg_long  = 790;

    pptime_match_result_t r = pptime_match(&stats, &ref);
    TEST_ASSERT_EQUAL(PPTIME_MATCH_OK, r);
}

void test_match_short_hi(void)
{
    pptime_proto_ref_t ref = { "KeeLoq", 400, 800, 50 };
    pptime_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    stats.n_short = 10;  stats.avg_short = 480;  /* too high */
    stats.n_long  = 10;  stats.avg_long  = 800;  /* ok */

    pptime_match_result_t r = pptime_match(&stats, &ref);
    TEST_ASSERT_EQUAL(PPTIME_MATCH_SHORT_HI, r);
}

void test_match_short_lo(void)
{
    pptime_proto_ref_t ref = { "KeeLoq", 400, 800, 50 };
    pptime_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    stats.n_short = 10;  stats.avg_short = 320;  /* too low */
    stats.n_long  = 10;  stats.avg_long  = 800;  /* ok */

    pptime_match_result_t r = pptime_match(&stats, &ref);
    TEST_ASSERT_EQUAL(PPTIME_MATCH_SHORT_LO, r);
}

void test_match_long_hi(void)
{
    pptime_proto_ref_t ref = { "KeeLoq", 400, 800, 50 };
    pptime_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    stats.n_short = 10;  stats.avg_short = 400;  /* ok */
    stats.n_long  = 10;  stats.avg_long  = 900;  /* too high */

    pptime_match_result_t r = pptime_match(&stats, &ref);
    TEST_ASSERT_EQUAL(PPTIME_MATCH_LONG_HI, r);
}

void test_match_long_lo(void)
{
    pptime_proto_ref_t ref = { "KeeLoq", 400, 800, 50 };
    pptime_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    stats.n_short = 10;  stats.avg_short = 400;  /* ok */
    stats.n_long  = 10;  stats.avg_long  = 700;  /* too low */

    pptime_match_result_t r = pptime_match(&stats, &ref);
    TEST_ASSERT_EQUAL(PPTIME_MATCH_LONG_LO, r);
}

void test_match_mismatch_both_bad(void)
{
    pptime_proto_ref_t ref = { "KeeLoq", 400, 800, 50 };
    pptime_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    stats.n_short = 10;  stats.avg_short = 200;  /* too low */
    stats.n_long  = 10;  stats.avg_long  = 1200; /* too high */

    pptime_match_result_t r = pptime_match(&stats, &ref);
    TEST_ASSERT_EQUAL(PPTIME_MATCH_MISMATCH, r);
}

void test_match_null_ref_returns_no_data(void)
{
    pptime_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    stats.n_short = 10;
    stats.n_long  = 10;

    pptime_match_result_t r = pptime_match(&stats, NULL);
    TEST_ASSERT_EQUAL(PPTIME_MATCH_NO_DATA, r);
}

/* ---- pptime_match_str tests ---------------------------------------------- */

void test_match_str_never_null(void)
{
    for (int v = PPTIME_MATCH_NO_DATA; v <= PPTIME_MATCH_MISMATCH; v++)
        TEST_ASSERT_NOT_NULL(pptime_match_str((pptime_match_result_t)v));
}

void test_match_str_ok_label(void)
{
    const char *s = pptime_match_str(PPTIME_MATCH_OK);
    TEST_ASSERT_EQUAL_STRING("MATCH", s);
}

/* ---- Proto table sanity -------------------------------------------------- */

void test_proto_table_non_empty(void)
{
    TEST_ASSERT_GREATER_THAN(0, (int)pptime_proto_table_count);
}

void test_proto_table_all_have_names(void)
{
    for (uint8_t i = 0; i < pptime_proto_table_count; i++)
        TEST_ASSERT_NOT_NULL(pptime_proto_table[i].name);
}

void test_proto_table_short_less_than_long(void)
{
    for (uint8_t i = 0; i < pptime_proto_table_count; i++)
        TEST_ASSERT_LESS_THAN(pptime_proto_table[i].te_long,
                              pptime_proto_table[i].te_short);
}

/* ---- End-to-end: analyze + match ----------------------------------------- */

void test_e2e_keeloq_match(void)
{
    pptime_proto_ref_t ref = { "KeeLoq", 400, 800, 100 };

    /* 16 clean short + 16 clean long pulses */
    uint16_t buf[32];
    make_pulses(buf, 16, 400, 16, 800);

    pptime_stats_t stats;
    pptime_analyze(buf, 32, &ref, &stats);

    pptime_match_result_t r = pptime_match(&stats, &ref);
    TEST_ASSERT_EQUAL(PPTIME_MATCH_OK, r);
}

void test_e2e_keeloq_short_hi_fails(void)
{
    pptime_proto_ref_t ref = { "KeeLoq", 400, 800, 50 };

    /* Short pulses at 480 µs — 80 µs above te_short+delta */
    uint16_t buf[32];
    make_pulses(buf, 16, 480, 16, 800);

    pptime_stats_t stats;
    pptime_analyze(buf, 32, &ref, &stats);

    pptime_match_result_t r = pptime_match(&stats, &ref);
    TEST_ASSERT_EQUAL(PPTIME_MATCH_SHORT_HI, r);
}

/* -------------------------------------------------------------------------- */

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_analyze_empty_returns_zeros);
    RUN_TEST(test_analyze_null_out_no_crash);
    RUN_TEST(test_analyze_no_ref_uses_defaults);
    RUN_TEST(test_analyze_with_keeloq_ref);
    RUN_TEST(test_analyze_filters_noise_below_min_valid);
    RUN_TEST(test_analyze_filters_noise_above_max_valid);
    RUN_TEST(test_analyze_min_max_tracking);

    RUN_TEST(test_match_no_data_when_empty_stats);
    RUN_TEST(test_match_no_data_when_few_samples);
    RUN_TEST(test_match_ok_within_tolerance);
    RUN_TEST(test_match_short_hi);
    RUN_TEST(test_match_short_lo);
    RUN_TEST(test_match_long_hi);
    RUN_TEST(test_match_long_lo);
    RUN_TEST(test_match_mismatch_both_bad);
    RUN_TEST(test_match_null_ref_returns_no_data);

    RUN_TEST(test_match_str_never_null);
    RUN_TEST(test_match_str_ok_label);

    RUN_TEST(test_proto_table_non_empty);
    RUN_TEST(test_proto_table_all_have_names);
    RUN_TEST(test_proto_table_short_less_than_long);

    RUN_TEST(test_e2e_keeloq_match);
    RUN_TEST(test_e2e_keeloq_short_hi_fails);

    return UNITY_END();
}
