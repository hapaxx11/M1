/* See COPYING.txt for license details. */

/*
 * test_subghz_mod_suggest.c
 *
 * Unit tests for subghz_mod_suggest — the waveform → modulation
 * suggestion heuristic (issue #616 "suggest modulation based on waveform").
 *
 * The analyzer inspects a signed RAW timing-sample array (mark/space, µs)
 * and reports whether the waveform looks like clean OOK/AM keying or the
 * non-quantized jitter an FSK/FM signal produces under an OOK detector.
 *
 * Build:
 *   cmake -B build-tests -S tests && cmake --build build-tests
 *   ctest --test-dir build-tests --output-on-failure -R subghz_mod_suggest
 */

#include <string.h>
#include "unity.h"
#include "subghz_mod_suggest.h"

void setUp(void) {}
void tearDown(void) {}

/*============================================================================*/
/* Waveform builders                                                          */
/*============================================================================*/

/* Deterministic LCG so tests are reproducible without <random>. */
static uint32_t rng_state;
static void rng_seed(uint32_t s) { rng_state = s ? s : 1u; }
static uint32_t rng_next(void)
{
    rng_state = rng_state * 1103515245u + 12345u;
    return rng_state;
}
/* Uniform in [lo, hi]. */
static int rng_range(int lo, int hi)
{
    return lo + (int)(rng_next() % (uint32_t)(hi - lo + 1));
}

/* Append a mark/space pair with alternating sign, jittered by +/- jit percent
 * of the nominal value. */
static uint16_t push_pair(int16_t *buf, uint16_t idx,
                          int mark_us, int space_us, int jit_pct)
{
    int mj = (mark_us  * jit_pct) / 100;
    int sj = (space_us * jit_pct) / 100;
    int m  = mark_us  + (mj ? rng_range(-mj, mj) : 0);
    int s  = space_us + (sj ? rng_range(-sj, sj) : 0);
    buf[idx++] = (int16_t)m;         /* mark (positive) */
    buf[idx++] = (int16_t)(-s);      /* space (negative) */
    return idx;
}

/*============================================================================*/
/* Guard / edge cases                                                         */
/*============================================================================*/

void test_null_input_is_unknown(void)
{
    SubGhzModSuggestResult r = subghz_mod_suggest(NULL, 128);
    TEST_ASSERT_EQUAL(SUBGHZ_MOD_SUGGEST_UNKNOWN, r.type);
    TEST_ASSERT_EQUAL(SUBGHZ_MOD_SUGGEST_CONF_NONE, r.confidence);
    TEST_ASSERT_EQUAL_UINT16(0, r.pulse_count);
}

void test_zero_count_is_unknown(void)
{
    int16_t buf[4] = { 300, -300, 300, -300 };
    SubGhzModSuggestResult r = subghz_mod_suggest(buf, 0);
    TEST_ASSERT_EQUAL(SUBGHZ_MOD_SUGGEST_UNKNOWN, r.type);
    TEST_ASSERT_EQUAL(SUBGHZ_MOD_SUGGEST_CONF_NONE, r.confidence);
}

void test_too_few_pulses_is_none(void)
{
    /* 10 pulses < SUBGHZ_MOD_SUGGEST_MIN_PULSES (16) */
    int16_t buf[10];
    for (int i = 0; i < 10; i++)
        buf[i] = (int16_t)((i & 1) ? -320 : 320);
    SubGhzModSuggestResult r = subghz_mod_suggest(buf, 10);
    TEST_ASSERT_EQUAL(SUBGHZ_MOD_SUGGEST_UNKNOWN, r.type);
    TEST_ASSERT_EQUAL(SUBGHZ_MOD_SUGGEST_CONF_NONE, r.confidence);
    TEST_ASSERT_EQUAL_UINT16(10, r.pulse_count);
}

void test_all_noise_glitches_below_floor_is_none(void)
{
    /* Every sample is below the noise floor → no in-band pulses. */
    int16_t buf[64];
    for (int i = 0; i < 64; i++)
        buf[i] = (int16_t)((i & 1) ? -10 : 10);
    SubGhzModSuggestResult r = subghz_mod_suggest(buf, 64);
    TEST_ASSERT_EQUAL(SUBGHZ_MOD_SUGGEST_UNKNOWN, r.type);
    TEST_ASSERT_EQUAL(SUBGHZ_MOD_SUGGEST_CONF_NONE, r.confidence);
    TEST_ASSERT_EQUAL_UINT16(0, r.pulse_count);
}

/*============================================================================*/
/* OOK / AM waveforms                                                         */
/*============================================================================*/

/* Clean CAME-style 1:2 OOK PWM: te=320.  Bit0 = short mark / long space,
 * bit1 = long mark / short space. */
void test_clean_ook_pwm_came_is_ook(void)
{
    int16_t buf[512];
    uint16_t idx = 0;
    rng_seed(1);
    const int te = 320;
    for (int i = 0; i < 60; i++)
    {
        if (i & 1)
            idx = push_pair(buf, idx, te, 2 * te, 8);   /* short/long */
        else
            idx = push_pair(buf, idx, 2 * te, te, 8);   /* long/short */
    }
    SubGhzModSuggestResult r = subghz_mod_suggest(buf, idx);
    TEST_ASSERT_EQUAL(SUBGHZ_MOD_SUGGEST_OOK, r.type);
    TEST_ASSERT_TRUE(r.confidence >= SUBGHZ_MOD_SUGGEST_CONF_MEDIUM);
    TEST_ASSERT_TRUE(r.quant_pct >= 80);
    /* te estimate should land near the true short pulse. */
    TEST_ASSERT_UINT16_WITHIN(80, te, r.te);
}

/* Princeton 1:3 OOK PWM: te=350. */
void test_clean_ook_pwm_princeton_is_ook(void)
{
    int16_t buf[512];
    uint16_t idx = 0;
    rng_seed(7);
    const int te = 350;
    for (int i = 0; i < 60; i++)
    {
        if (i % 3 == 0)
            idx = push_pair(buf, idx, te, 3 * te, 10);
        else
            idx = push_pair(buf, idx, 3 * te, te, 10);
    }
    SubGhzModSuggestResult r = subghz_mod_suggest(buf, idx);
    TEST_ASSERT_EQUAL(SUBGHZ_MOD_SUGGEST_OOK, r.type);
    TEST_ASSERT_TRUE(r.confidence >= SUBGHZ_MOD_SUGGEST_CONF_MEDIUM);
}

/* A high-count clean OOK capture should reach HIGH confidence. */
void test_large_clean_ook_is_high_confidence(void)
{
    int16_t buf[512];
    uint16_t idx = 0;
    rng_seed(3);
    const int te = 260;
    for (int i = 0; i < 120 && idx < 508; i++)
    {
        if (i & 1)
            idx = push_pair(buf, idx, te, 2 * te, 6);
        else
            idx = push_pair(buf, idx, 2 * te, te, 6);
    }
    SubGhzModSuggestResult r = subghz_mod_suggest(buf, idx);
    TEST_ASSERT_EQUAL(SUBGHZ_MOD_SUGGEST_OOK, r.type);
    TEST_ASSERT_EQUAL(SUBGHZ_MOD_SUGGEST_CONF_HIGH, r.confidence);
}

/* Negative-leading samples must be handled by absolute value. */
void test_sign_is_ignored(void)
{
    int16_t buf[512];
    uint16_t idx = 0;
    rng_seed(5);
    const int te = 400;
    for (int i = 0; i < 60; i++)
        idx = push_pair(buf, idx, te, 2 * te, 8);
    /* Flip every sign — same absolute waveform. */
    for (uint16_t i = 0; i < idx; i++)
        buf[i] = (int16_t)(-buf[i]);
    SubGhzModSuggestResult r = subghz_mod_suggest(buf, idx);
    TEST_ASSERT_EQUAL(SUBGHZ_MOD_SUGGEST_OOK, r.type);
}

/*============================================================================*/
/* FSK / FM (noise-like under OOK detector)                                   */
/*============================================================================*/

/* Irregular, non-quantized jitter spread continuously across a wide range —
 * what an FSK/FM signal looks like to an OOK edge detector.  Should NOT be
 * classified as OOK. */
void test_random_jitter_is_fsk(void)
{
    int16_t buf[512];
    uint16_t idx = 0;
    rng_seed(42);
    for (int i = 0; i < 200 && idx < 510; i++)
    {
        int m = rng_range(50, 900);
        int s = rng_range(50, 900);
        buf[idx++] = (int16_t)m;
        buf[idx++] = (int16_t)(-s);
    }
    SubGhzModSuggestResult r = subghz_mod_suggest(buf, idx);
    TEST_ASSERT_EQUAL(SUBGHZ_MOD_SUGGEST_FSK, r.type);
    TEST_ASSERT_TRUE(r.quant_pct < 55);
}

/* Very short, dense, random edges (classic OOK-demodulated FSK carrier). */
void test_dense_short_noise_is_fsk(void)
{
    int16_t buf[512];
    uint16_t idx = 0;
    rng_seed(99);
    for (int i = 0; i < 250 && idx < 510; i++)
    {
        int m = rng_range(45, 260);
        int s = rng_range(45, 260);
        buf[idx++] = (int16_t)m;
        buf[idx++] = (int16_t)(-s);
    }
    SubGhzModSuggestResult r = subghz_mod_suggest(buf, idx);
    TEST_ASSERT_EQUAL(SUBGHZ_MOD_SUGGEST_FSK, r.type);
}

/*============================================================================*/
/* Gap / mixed handling                                                       */
/*============================================================================*/

/* Long inter-packet gaps must be excluded and must not break the OOK verdict. */
void test_ook_with_interpacket_gaps_is_ook(void)
{
    int16_t buf[512];
    uint16_t idx = 0;
    rng_seed(11);
    const int te = 300;
    for (int pkt = 0; pkt < 4; pkt++)
    {
        for (int i = 0; i < 12; i++)
        {
            if (i & 1)
                idx = push_pair(buf, idx, te, 2 * te, 8);
            else
                idx = push_pair(buf, idx, 2 * te, te, 8);
        }
        /* Big gap between packets (> GAP_CEIL) — excluded from analysis. */
        buf[idx++] = (int16_t)-20000; /* clamped/handled as > ceil via abs */
    }
    SubGhzModSuggestResult r = subghz_mod_suggest(buf, idx);
    TEST_ASSERT_EQUAL(SUBGHZ_MOD_SUGGEST_OOK, r.type);
}

/*============================================================================*/
/* String helpers                                                             */
/*============================================================================*/

void test_type_strings(void)
{
    TEST_ASSERT_EQUAL_STRING("OOK/AM", subghz_mod_suggest_type_str(SUBGHZ_MOD_SUGGEST_OOK));
    TEST_ASSERT_EQUAL_STRING("FSK/FM", subghz_mod_suggest_type_str(SUBGHZ_MOD_SUGGEST_FSK));
    TEST_ASSERT_EQUAL_STRING("?",      subghz_mod_suggest_type_str(SUBGHZ_MOD_SUGGEST_UNKNOWN));
}

void test_confidence_strings(void)
{
    TEST_ASSERT_EQUAL_STRING("-",    subghz_mod_suggest_confidence_str(SUBGHZ_MOD_SUGGEST_CONF_NONE));
    TEST_ASSERT_EQUAL_STRING("low",  subghz_mod_suggest_confidence_str(SUBGHZ_MOD_SUGGEST_CONF_LOW));
    TEST_ASSERT_EQUAL_STRING("med",  subghz_mod_suggest_confidence_str(SUBGHZ_MOD_SUGGEST_CONF_MEDIUM));
    TEST_ASSERT_EQUAL_STRING("high", subghz_mod_suggest_confidence_str(SUBGHZ_MOD_SUGGEST_CONF_HIGH));
}

/*============================================================================*/
/* Runner                                                                     */
/*============================================================================*/

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_null_input_is_unknown);
    RUN_TEST(test_zero_count_is_unknown);
    RUN_TEST(test_too_few_pulses_is_none);
    RUN_TEST(test_all_noise_glitches_below_floor_is_none);

    RUN_TEST(test_clean_ook_pwm_came_is_ook);
    RUN_TEST(test_clean_ook_pwm_princeton_is_ook);
    RUN_TEST(test_large_clean_ook_is_high_confidence);
    RUN_TEST(test_sign_is_ignored);

    RUN_TEST(test_random_jitter_is_fsk);
    RUN_TEST(test_dense_short_noise_is_fsk);

    RUN_TEST(test_ook_with_interpacket_gaps_is_ook);

    RUN_TEST(test_type_strings);
    RUN_TEST(test_confidence_strings);

    return UNITY_END();
}
