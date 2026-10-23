/* See COPYING.txt for license details. */

/*
 * test_remote_analyzer.c
 *
 * Host tests for the pure Remote Analyzer detection logic
 * (Sub_Ghz/subghz_remote_analyzer.c).
 */

#include "unity.h"
#include "subghz_remote_analyzer.h"

void setUp(void) { }
void tearDown(void) { }

#define BASE_FREQ 433920000u
#define POINTS    SUBGHZ_RA_SWEEP_POINTS /* 31 */

static void build_sweep(int16_t *sweep, uint8_t count, int16_t floor_dbm) {
    for(uint8_t i = 0; i < count; i++) sweep[i] = floor_dbm;
}

/* ------------------------------------------------------------------ */
/* Coarse stage                                                        */
/* ------------------------------------------------------------------ */

void test_coarse_feed_tracks_max(void) {
    subghz_ra_state_t st;
    subghz_ra_reset(&st);
    TEST_ASSERT_EQUAL_INT16(INT16_MIN, st.coarse.best_rssi);

    subghz_ra_coarse_feed(&st, 300000000u, -80);
    subghz_ra_coarse_feed(&st, 315000000u, -75);
    subghz_ra_coarse_feed(&st, 433920000u, -90); /* lower: must not overwrite */
    TEST_ASSERT_EQUAL_INT16(-75, st.coarse.best_rssi);
    TEST_ASSERT_EQUAL_UINT32(315000000u, st.coarse.best_freq);
}

void test_coarse_feed_tie_keeps_first(void) {
    subghz_ra_state_t st;
    subghz_ra_reset(&st);

    subghz_ra_coarse_feed(&st, 315000000u, -75);
    subghz_ra_coarse_feed(&st, 433920000u, -75); /* tie: first wins */
    TEST_ASSERT_EQUAL_INT16(-75, st.coarse.best_rssi);
    TEST_ASSERT_EQUAL_UINT32(315000000u, st.coarse.best_freq);
}

void test_coarse_feed_after_reset(void) {
    subghz_ra_state_t st;
    subghz_ra_reset(&st);
    subghz_ra_coarse_feed(&st, 315000000u, -75);
    subghz_ra_reset(&st);
    TEST_ASSERT_EQUAL_INT16(INT16_MIN, st.coarse.best_rssi);
    TEST_ASSERT_EQUAL_UINT32(0u, st.coarse.best_freq);

    subghz_ra_coarse_feed(&st, 868350000u, -100);
    TEST_ASSERT_EQUAL_INT16(-100, st.coarse.best_rssi);
    TEST_ASSERT_EQUAL_UINT32(868350000u, st.coarse.best_freq);
}

void test_trigger_adjustment_clamps_to_limits(void) {
    TEST_ASSERT_EQUAL_INT16(SUBGHZ_RA_TRIGGER_MIN_DBM,
                            subghz_ra_adjust_trigger(-109, false));
    TEST_ASSERT_EQUAL_INT16(SUBGHZ_RA_TRIGGER_MAX_DBM,
                            subghz_ra_adjust_trigger(-59, true));
    TEST_ASSERT_EQUAL_INT16(-107, subghz_ra_adjust_trigger(-110, true));
    TEST_ASSERT_EQUAL_INT16(-63, subghz_ra_adjust_trigger(-60, false));
}

/* ------------------------------------------------------------------ */
/* Fine stage / AM classification                                      */
/* ------------------------------------------------------------------ */

void test_fine_analyze_sharp_peak_is_am(void) {
    subghz_ra_state_t st;
    subghz_ra_reset(&st);
    int16_t sweep[POINTS];
    build_sweep(sweep, POINTS, -90);
    sweep[14] = -80;
    sweep[15] = -70; /* peak at center, 10 dB above both neighbors */
    sweep[16] = -80;

    bool is_am = false;
    TEST_ASSERT_TRUE(subghz_ra_fine_analyze(sweep, POINTS, BASE_FREQ, &st, &is_am));
    TEST_ASSERT_TRUE(is_am);
    TEST_ASSERT_EQUAL_INT16(-70, st.fine.best_rssi);
    TEST_ASSERT_EQUAL_UINT32(BASE_FREQ, st.fine.best_freq); /* idx 15 = center */
}

void test_fine_analyze_broad_plateau_not_am(void) {
    subghz_ra_state_t st;
    subghz_ra_reset(&st);
    int16_t sweep[POINTS];
    build_sweep(sweep, POINTS, -80);
    sweep[15] = -70;
    sweep[14] = -75; /* only 5 dB below peak */
    sweep[16] = -75;

    bool is_am = true;
    TEST_ASSERT_TRUE(subghz_ra_fine_analyze(sweep, POINTS, BASE_FREQ, &st, &is_am));
    TEST_ASSERT_FALSE(is_am);
    /* st->fine is set regardless of the AM verdict */
    TEST_ASSERT_EQUAL_INT16(-70, st.fine.best_rssi);
    TEST_ASSERT_EQUAL_UINT32(BASE_FREQ, st.fine.best_freq);
}

void test_fine_analyze_exactly_6db_not_am(void) {
    subghz_ra_state_t st;
    subghz_ra_reset(&st);
    int16_t sweep[POINTS];
    build_sweep(sweep, POINTS, -90);
    sweep[15] = -70;
    sweep[14] = -76; /* exactly 6 dB: rule is strictly '>' */
    sweep[16] = -76;

    bool is_am = true;
    TEST_ASSERT_TRUE(subghz_ra_fine_analyze(sweep, POINTS, BASE_FREQ, &st, &is_am));
    TEST_ASSERT_FALSE(is_am);
}

void test_fine_analyze_edge_peak_never_am(void) {
    subghz_ra_state_t st;
    subghz_ra_reset(&st);
    int16_t sweep[POINTS];
    bool is_am;

    /* Peak at index 0 */
    build_sweep(sweep, POINTS, -90);
    sweep[0] = -70;
    sweep[1] = -85;
    is_am = true;
    TEST_ASSERT_TRUE(subghz_ra_fine_analyze(sweep, POINTS, BASE_FREQ, &st, &is_am));
    TEST_ASSERT_FALSE(is_am);
    TEST_ASSERT_EQUAL_UINT32(BASE_FREQ - SUBGHZ_RA_SWEEP_HALF_SPAN_HZ, st.fine.best_freq);

    /* Peak at last index */
    build_sweep(sweep, POINTS, -90);
    sweep[POINTS - 1] = -70;
    sweep[POINTS - 2] = -85;
    is_am = true;
    TEST_ASSERT_TRUE(subghz_ra_fine_analyze(sweep, POINTS, BASE_FREQ, &st, &is_am));
    TEST_ASSERT_FALSE(is_am);
    TEST_ASSERT_EQUAL_UINT32(
        BASE_FREQ - SUBGHZ_RA_SWEEP_HALF_SPAN_HZ + (POINTS - 1) * SUBGHZ_RA_SWEEP_STEP_HZ,
        st.fine.best_freq);
}

void test_fine_analyze_too_few_samples(void) {
    subghz_ra_state_t st;
    subghz_ra_reset(&st);
    int16_t sweep[POINTS];
    build_sweep(sweep, POINTS, -70);

    bool is_am = true;
    TEST_ASSERT_FALSE(subghz_ra_fine_analyze(sweep, 2, BASE_FREQ, &st, &is_am));
    TEST_ASSERT_FALSE(subghz_ra_fine_analyze(sweep, 0, BASE_FREQ, &st, &is_am));
    /* state untouched */
    TEST_ASSERT_EQUAL_INT16(INT16_MIN, st.fine.best_rssi);
}

void test_refinement_rejects_peak_at_or_below_detection_threshold(void) {
    TEST_ASSERT_FALSE(subghz_ra_refinement_signal_valid(-80, -80));
    TEST_ASSERT_FALSE(subghz_ra_refinement_signal_valid(-90, -80));
    TEST_ASSERT_TRUE(subghz_ra_refinement_signal_valid(-79, -80));
}

/* ------------------------------------------------------------------ */
/* Adaptive smoothing                                                  */
/* ------------------------------------------------------------------ */

void test_smooth_first_sample_initializes(void) {
    subghz_ra_state_t st;
    subghz_ra_reset(&st);
    TEST_ASSERT_EQUAL_UINT32(0u, st.fil_val);

    /* No undershoot: the first sample is returned as-is and sets the state. */
    TEST_ASSERT_EQUAL_UINT32(433920000u, subghz_ra_smooth(&st, 433920000u));
    TEST_ASSERT_EQUAL_UINT32(433920000u, st.fil_val);
}

void test_smooth_big_jump_fast_convergence(void) {
    subghz_ra_state_t st;
    subghz_ra_reset(&st);
    subghz_ra_smooth(&st, 300000000u);

    /* 133.92 MHz jump > 500 kHz -> k = 0.9 */
    TEST_ASSERT_EQUAL_UINT32(420528000u, subghz_ra_smooth(&st, 433920000u));
    TEST_ASSERT_EQUAL_UINT32(432580800u, subghz_ra_smooth(&st, 433920000u));
    /* third step lands within 200 kHz of the target; once the remaining
     * error drops below 500 kHz the adaptive coefficient switches back to
     * k = 0.03, so full convergence is slow by design (upstream behavior). */
    uint32_t v = subghz_ra_smooth(&st, 433920000u);
    TEST_ASSERT_UINT32_WITHIN(200000u, 433920000u, v);
}

void test_smooth_small_drift_barely_moves(void) {
    subghz_ra_state_t st;
    subghz_ra_reset(&st);
    subghz_ra_smooth(&st, 433920000u);

    /* 5 kHz drift <= 500 kHz -> k = 0.03 */
    TEST_ASSERT_EQUAL_UINT32(433920150u, subghz_ra_smooth(&st, 433925000u));
    TEST_ASSERT_EQUAL_UINT32(433920296u, subghz_ra_smooth(&st, 433925000u));
}

void test_smooth_jump_boundary_is_strictly_greater(void) {
    subghz_ra_state_t st;
    subghz_ra_reset(&st);
    subghz_ra_smooth(&st, 433920000u);

    /* exactly 500 kHz: slow k = 0.03 (upstream comparison is '>') */
    TEST_ASSERT_EQUAL_UINT32(433935000u, subghz_ra_smooth(&st, 434420000u));
}

void test_smooth_negative_jump(void) {
    subghz_ra_state_t st;
    subghz_ra_reset(&st);
    subghz_ra_smooth(&st, 433920000u);

    TEST_ASSERT_EQUAL_UINT32(313392000u, subghz_ra_smooth(&st, 300000000u));
}

/* ------------------------------------------------------------------ */
/* Delivery policy                                                     */
/* ------------------------------------------------------------------ */

/* Full lifecycle: silence -> fine lock -> smoothing on re-lock -> hold
 * decay (true, false at 3, true, true) -> signal lost + smoother reset. */
void test_deliver_full_lifecycle(void) {
    subghz_ra_state_t st;
    subghz_ra_reset(&st);
    int16_t sweep[POINTS];
    bool is_am;
    subghz_ra_frame_t f;

    /* Cycle 0: nothing above the trigger -> no signal. */
    subghz_ra_coarse_feed(&st, BASE_FREQ, -110);
    f = subghz_ra_deliver(&st, SUBGHZ_RA_TRIGGER_DBM);
    TEST_ASSERT_EQUAL_UINT32(0u, f.frequency);
    TEST_ASSERT_EQUAL_INT16(0, f.rssi);
    TEST_ASSERT_FALSE(f.signal_present);
    TEST_ASSERT_EQUAL_UINT8(0, st.sample_hold);

    /* Cycle 1: fine peak above trigger -> fine frame, hold = 5.
     * First delivery initializes the smoother (frequency passes through). */
    subghz_ra_coarse_feed(&st, BASE_FREQ, -80);
    build_sweep(sweep, POINTS, -90);
    sweep[14] = -80;
    sweep[15] = -70;
    sweep[16] = -80;
    TEST_ASSERT_TRUE(subghz_ra_fine_analyze(sweep, POINTS, BASE_FREQ, &st, &is_am));
    f = subghz_ra_deliver(&st, SUBGHZ_RA_TRIGGER_DBM);
    TEST_ASSERT_TRUE(f.signal_present);
    TEST_ASSERT_TRUE(f.is_am);
    TEST_ASSERT_EQUAL_INT16(-70, f.rssi);
    TEST_ASSERT_EQUAL_UINT32(BASE_FREQ, f.frequency);
    TEST_ASSERT_EQUAL_UINT8(5, st.sample_hold);
    TEST_ASSERT_EQUAL_UINT32(BASE_FREQ, st.fil_val);

    /* Cycle 2: fine peak 20 kHz up -> smoothed delivery (k = 0.03),
     * remembered frame keeps the RAW peak frequency. */
    subghz_ra_coarse_feed(&st, BASE_FREQ, -80);
    build_sweep(sweep, POINTS, -90);
    sweep[15] = -82;
    sweep[16] = -72; /* idx 16 -> BASE_FREQ + 20 kHz */
    sweep[17] = -82;
    TEST_ASSERT_TRUE(subghz_ra_fine_analyze(sweep, POINTS, BASE_FREQ, &st, &is_am));
    f = subghz_ra_deliver(&st, SUBGHZ_RA_TRIGGER_DBM);
    TEST_ASSERT_TRUE(f.signal_present);
    TEST_ASSERT_EQUAL_INT16(-72, f.rssi);
    TEST_ASSERT_EQUAL_UINT32(BASE_FREQ + 600u, f.frequency); /* 20000 * 0.03 */
    TEST_ASSERT_EQUAL_UINT32(BASE_FREQ + 20000u, st.last_shown_freq);
    TEST_ASSERT_EQUAL_UINT8(5, st.sample_hold);

    /* Cycle 3: below trigger -> hold 5->4, remembered frame, still present. */
    subghz_ra_coarse_feed(&st, BASE_FREQ, -110);
    f = subghz_ra_deliver(&st, SUBGHZ_RA_TRIGGER_DBM);
    TEST_ASSERT_EQUAL_UINT8(4, st.sample_hold);
    TEST_ASSERT_TRUE(f.signal_present);
    TEST_ASSERT_EQUAL_UINT32(BASE_FREQ + 20000u, f.frequency);
    TEST_ASSERT_EQUAL_INT16(-72, f.rssi);
    TEST_ASSERT_TRUE(f.is_am);

    /* Cycle 4: hold 4->3 -> remembered frame with signal_present=false. */
    subghz_ra_coarse_feed(&st, BASE_FREQ, -110);
    f = subghz_ra_deliver(&st, SUBGHZ_RA_TRIGGER_DBM);
    TEST_ASSERT_EQUAL_UINT8(3, st.sample_hold);
    TEST_ASSERT_FALSE(f.signal_present);
    TEST_ASSERT_EQUAL_UINT32(BASE_FREQ + 20000u, f.frequency);
    TEST_ASSERT_EQUAL_INT16(-72, f.rssi);

    /* Cycle 5: hold 3->2 -> remembered frame, present again (upstream does
     * not call back here; the display keeps the held value). */
    subghz_ra_coarse_feed(&st, BASE_FREQ, -110);
    f = subghz_ra_deliver(&st, SUBGHZ_RA_TRIGGER_DBM);
    TEST_ASSERT_EQUAL_UINT8(2, st.sample_hold);
    TEST_ASSERT_TRUE(f.signal_present);
    TEST_ASSERT_EQUAL_UINT32(BASE_FREQ + 20000u, f.frequency);

    /* Cycle 6: hold 2->1 -> remembered frame, present. */
    subghz_ra_coarse_feed(&st, BASE_FREQ, -110);
    f = subghz_ra_deliver(&st, SUBGHZ_RA_TRIGGER_DBM);
    TEST_ASSERT_EQUAL_UINT8(1, st.sample_hold);
    TEST_ASSERT_TRUE(f.signal_present);

    /* Cycle 7: hold 1->0 -> signal lost, smoother reset. */
    subghz_ra_coarse_feed(&st, BASE_FREQ, -110);
    f = subghz_ra_deliver(&st, SUBGHZ_RA_TRIGGER_DBM);
    TEST_ASSERT_EQUAL_UINT8(0, st.sample_hold);
    TEST_ASSERT_FALSE(f.signal_present);
    TEST_ASSERT_EQUAL_UINT32(0u, f.frequency);
    TEST_ASSERT_EQUAL_INT16(0, f.rssi);
    TEST_ASSERT_EQUAL_UINT32(0u, st.fil_val);

    /* Cycle 8: still nothing -> stays lost. */
    subghz_ra_coarse_feed(&st, BASE_FREQ, -110);
    f = subghz_ra_deliver(&st, SUBGHZ_RA_TRIGGER_DBM);
    TEST_ASSERT_FALSE(f.signal_present);
    TEST_ASSERT_EQUAL_UINT32(0u, f.frequency);
}

void test_deliver_coarse_fallback_when_hold_low(void) {
    subghz_ra_state_t st;
    subghz_ra_reset(&st);

    /* Coarse above trigger, no fine sweep, hold = 0 (< 2) -> coarse frame. */
    subghz_ra_coarse_feed(&st, 315000000u, -90);
    subghz_ra_frame_t f = subghz_ra_deliver(&st, SUBGHZ_RA_TRIGGER_DBM);
    TEST_ASSERT_TRUE(f.signal_present);
    TEST_ASSERT_FALSE(f.is_am);
    TEST_ASSERT_EQUAL_INT16(-90, f.rssi);
    TEST_ASSERT_EQUAL_UINT32(315000000u, f.frequency);
    TEST_ASSERT_EQUAL_UINT8(5, st.sample_hold);
}

void test_deliver_coarse_blocked_while_hold_high(void) {
    subghz_ra_state_t st;
    subghz_ra_reset(&st);
    int16_t sweep[POINTS];
    bool is_am;

    /* Lock onto a fine signal first (hold = 5). */
    subghz_ra_coarse_feed(&st, BASE_FREQ, -80);
    build_sweep(sweep, POINTS, -90);
    sweep[14] = -80;
    sweep[15] = -70;
    sweep[16] = -80;
    TEST_ASSERT_TRUE(subghz_ra_fine_analyze(sweep, POINTS, BASE_FREQ, &st, &is_am));
    subghz_ra_frame_t f = subghz_ra_deliver(&st, SUBGHZ_RA_TRIGGER_DBM);
    TEST_ASSERT_TRUE(f.signal_present);
    TEST_ASSERT_EQUAL_UINT8(5, st.sample_hold);

    /* Next cycle: fine sweep ran but its peak is below the trigger, while a
     * DIFFERENT coarse frequency is above it.  With hold = 5 (>= 2) the
     * coarse branch must NOT fire: the held fine frame decays instead. */
    subghz_ra_coarse_feed(&st, 315000000u, -90);
    build_sweep(sweep, POINTS, -110);
    sweep[15] = -100; /* below trigger */
    TEST_ASSERT_TRUE(subghz_ra_fine_analyze(sweep, POINTS, BASE_FREQ, &st, &is_am));
    f = subghz_ra_deliver(&st, SUBGHZ_RA_TRIGGER_DBM);
    TEST_ASSERT_EQUAL_UINT8(4, st.sample_hold);
    TEST_ASSERT_TRUE(f.signal_present);
    TEST_ASSERT_EQUAL_UINT32(BASE_FREQ, f.frequency); /* held fine, not 315 MHz */
    TEST_ASSERT_EQUAL_INT16(-70, f.rssi);
}

void test_rejected_flat_sweep_preserves_held_am_classification(void) {
    subghz_ra_state_t st;
    subghz_ra_reset(&st);
    int16_t sweep[POINTS];
    bool is_am = false;

    subghz_ra_coarse_feed(&st, BASE_FREQ, -80);
    build_sweep(sweep, POINTS, -90);
    sweep[14] = -80;
    sweep[15] = -70;
    sweep[16] = -80;
    TEST_ASSERT_TRUE(subghz_ra_fine_analyze(sweep, POINTS, BASE_FREQ, &st, &is_am));
    subghz_ra_frame_t frame = subghz_ra_deliver(&st, SUBGHZ_RA_TRIGGER_DBM);
    TEST_ASSERT_TRUE(frame.is_am);

    build_sweep(sweep, POINTS, -110);
    TEST_ASSERT_TRUE(subghz_ra_fine_analyze(sweep, POINTS, BASE_FREQ, &st, &is_am));
    TEST_ASSERT_FALSE(is_am);
    frame = subghz_ra_deliver(&st, SUBGHZ_RA_TRIGGER_DBM);
    TEST_ASSERT_EQUAL_UINT32(BASE_FREQ, frame.frequency);
    TEST_ASSERT_EQUAL_INT16(-70, frame.rssi);
    TEST_ASSERT_TRUE(frame.is_am);
}

void test_deliver_custom_trigger(void) {
    subghz_ra_state_t st;
    subghz_ra_reset(&st);
    int16_t sweep[POINTS];
    bool is_am;
    subghz_ra_frame_t f;

    /* Fine peak at -90: below a -85 trigger -> no signal. */
    subghz_ra_coarse_feed(&st, BASE_FREQ, -91);
    build_sweep(sweep, POINTS, -100);
    sweep[15] = -90;
    TEST_ASSERT_TRUE(subghz_ra_fine_analyze(sweep, POINTS, BASE_FREQ, &st, &is_am));
    f = subghz_ra_deliver(&st, -85);
    TEST_ASSERT_FALSE(f.signal_present);
    TEST_ASSERT_EQUAL_UINT32(0u, f.frequency);

    /* Same peak passes a -95 trigger. */
    subghz_ra_coarse_feed(&st, BASE_FREQ, -91);
    TEST_ASSERT_TRUE(subghz_ra_fine_analyze(sweep, POINTS, BASE_FREQ, &st, &is_am));
    f = subghz_ra_deliver(&st, -95);
    TEST_ASSERT_TRUE(f.signal_present);
    TEST_ASSERT_EQUAL_UINT32(BASE_FREQ, f.frequency);
    TEST_ASSERT_EQUAL_INT16(-90, f.rssi);
}

/* ------------------------------------------------------------------ */
/* Nearest frequency                                                   */
/* ------------------------------------------------------------------ */

static const uint32_t FREQ_LIST[] = {300000000u, 315000000u, 433920000u, 868350000u};

void test_nearest_frequency_exact_match(void) {
    TEST_ASSERT_EQUAL_UINT32(315000000u,
                             subghz_ra_nearest_frequency(FREQ_LIST, 4, 315000000u));
}

void test_nearest_frequency_midpoint_tie_picks_lower(void) {
    TEST_ASSERT_EQUAL_UINT32(300000000u,
                             subghz_ra_nearest_frequency(FREQ_LIST, 4, 307500000u));
}

void test_nearest_frequency_between_values(void) {
    TEST_ASSERT_EQUAL_UINT32(315000000u,
                             subghz_ra_nearest_frequency(FREQ_LIST, 4, 310000000u));
    TEST_ASSERT_EQUAL_UINT32(433920000u,
                             subghz_ra_nearest_frequency(FREQ_LIST, 4, 500000000u));
}

void test_nearest_frequency_below_and_above_range(void) {
    /* Below all: rounds up to the first entry. */
    TEST_ASSERT_EQUAL_UINT32(300000000u,
                             subghz_ra_nearest_frequency(FREQ_LIST, 4, 200000000u));
    /* Above all: upstream returns 0 (no candidate bracket found). */
    TEST_ASSERT_EQUAL_UINT32(0u,
                             subghz_ra_nearest_frequency(FREQ_LIST, 4, 900000000u));
}

void test_nearest_frequency_skips_zeros(void) {
    static const uint32_t with_zeros[] = {0u, 0u, 433920000u, 0u};
    TEST_ASSERT_EQUAL_UINT32(433920000u,
                             subghz_ra_nearest_frequency(with_zeros, 4, 433920000u));
    TEST_ASSERT_EQUAL_UINT32(433920000u,
                             subghz_ra_nearest_frequency(with_zeros, 4, 400000000u));
}

void test_nearest_frequency_empty_or_all_zero(void) {
    static const uint32_t zeros[] = {0u, 0u, 0u};
    TEST_ASSERT_EQUAL_UINT32(0u, subghz_ra_nearest_frequency(FREQ_LIST, 0, 433920000u));
    TEST_ASSERT_EQUAL_UINT32(0u, subghz_ra_nearest_frequency(zeros, 3, 433920000u));
}

/* ------------------------------------------------------------------ */
/* Refinement plan builder                                             */
/* ------------------------------------------------------------------ */

#define BAND_MIN 250000000u
#define BAND_MAX 960000000u

void test_refine_plan_strongest_first_order(void) {
    static const subghz_ra_candidate_t cands[] = {
        {315000000u, -90},
        {433920000u, -60}, /* strongest */
        {868350000u, -75},
    };
    uint32_t out[4] = {0};
    TEST_ASSERT_EQUAL_UINT8(3, subghz_ra_build_refine_plan(
        cands, 3, BAND_MIN, BAND_MAX, out, 4));
    TEST_ASSERT_EQUAL_UINT32(433920000u, out[0]);
    TEST_ASSERT_EQUAL_UINT32(868350000u, out[1]);
    TEST_ASSERT_EQUAL_UINT32(315000000u, out[2]);
}

void test_refine_plan_dedup_keeps_stronger(void) {
    static const subghz_ra_candidate_t cands[] = {
        {433920000u, -80}, /* within 500 kHz of the stronger entry below */
        {434100000u, -65}, /* 180 kHz away — dedups the -80 entry */
        {315000000u, -85},
    };
    uint32_t out[4] = {0};
    TEST_ASSERT_EQUAL_UINT8(2, subghz_ra_build_refine_plan(
        cands, 3, BAND_MIN, BAND_MAX, out, 4));
    TEST_ASSERT_EQUAL_UINT32(434100000u, out[0]);
    TEST_ASSERT_EQUAL_UINT32(315000000u, out[1]);
}

void test_refine_plan_dedup_boundary_500khz(void) {
    static const subghz_ra_candidate_t cands[] = {
        {433920000u, -70},
        {434420000u, -65}, /* exactly 500 kHz away: NOT a dup (strictly '<') */
    };
    uint32_t out[4] = {0};
    TEST_ASSERT_EQUAL_UINT8(2, subghz_ra_build_refine_plan(
        cands, 2, BAND_MIN, BAND_MAX, out, 4));
}

void test_refine_plan_skips_out_of_band(void) {
    static const subghz_ra_candidate_t cands[] = {
        {100000000u, -50}, /* below band — skipped despite being strongest */
        {433920000u, -80},
        {970000000u, -60}, /* above band — skipped */
    };
    uint32_t out[4] = {0};
    TEST_ASSERT_EQUAL_UINT8(1, subghz_ra_build_refine_plan(
        cands, 3, BAND_MIN, BAND_MAX, out, 4));
    TEST_ASSERT_EQUAL_UINT32(433920000u, out[0]);
}

void test_refine_plan_respects_max_out(void) {
    static const subghz_ra_candidate_t cands[] = {
        {300000000u, -90},
        {315000000u, -60},
        {433920000u, -70},
        {868350000u, -80},
    };
    uint32_t out[2] = {0};
    TEST_ASSERT_EQUAL_UINT8(2, subghz_ra_build_refine_plan(
        cands, 4, BAND_MIN, BAND_MAX, out, 2));
    /* Only the two strongest survive the cap. */
    TEST_ASSERT_EQUAL_UINT32(315000000u, out[0]);
    TEST_ASSERT_EQUAL_UINT32(433920000u, out[1]);
}

void test_refine_plan_tie_keeps_first(void) {
    static const subghz_ra_candidate_t cands[] = {
        {315000000u, -75},
        {433920000u, -75}, /* tie: first candidate wins (strict '>') */
    };
    uint32_t out[4] = {0};
    TEST_ASSERT_EQUAL_UINT8(2, subghz_ra_build_refine_plan(
        cands, 2, BAND_MIN, BAND_MAX, out, 4));
    TEST_ASSERT_EQUAL_UINT32(315000000u, out[0]);
    TEST_ASSERT_EQUAL_UINT32(433920000u, out[1]);
}

void test_refine_plan_null_and_empty(void) {
    uint32_t out[2] = {0};
    static const subghz_ra_candidate_t cands[] = {{433920000u, -70}};
    TEST_ASSERT_EQUAL_UINT8(0, subghz_ra_build_refine_plan(
        NULL, 1, BAND_MIN, BAND_MAX, out, 2));
    TEST_ASSERT_EQUAL_UINT8(0, subghz_ra_build_refine_plan(
        cands, 0, BAND_MIN, BAND_MAX, out, 2));
    TEST_ASSERT_EQUAL_UINT8(0, subghz_ra_build_refine_plan(
        cands, 1, BAND_MIN, BAND_MAX, NULL, 2));
    TEST_ASSERT_EQUAL_UINT8(0, subghz_ra_build_refine_plan(
        cands, 1, BAND_MIN, BAND_MAX, out, 0));
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_coarse_feed_tracks_max);
    RUN_TEST(test_coarse_feed_tie_keeps_first);
    RUN_TEST(test_coarse_feed_after_reset);
    RUN_TEST(test_trigger_adjustment_clamps_to_limits);

    RUN_TEST(test_fine_analyze_sharp_peak_is_am);
    RUN_TEST(test_fine_analyze_broad_plateau_not_am);
    RUN_TEST(test_fine_analyze_exactly_6db_not_am);
    RUN_TEST(test_fine_analyze_edge_peak_never_am);
    RUN_TEST(test_fine_analyze_too_few_samples);
    RUN_TEST(test_refinement_rejects_peak_at_or_below_detection_threshold);

    RUN_TEST(test_smooth_first_sample_initializes);
    RUN_TEST(test_smooth_big_jump_fast_convergence);
    RUN_TEST(test_smooth_small_drift_barely_moves);
    RUN_TEST(test_smooth_jump_boundary_is_strictly_greater);
    RUN_TEST(test_smooth_negative_jump);

    RUN_TEST(test_deliver_full_lifecycle);
    RUN_TEST(test_deliver_coarse_fallback_when_hold_low);
    RUN_TEST(test_deliver_coarse_blocked_while_hold_high);
    RUN_TEST(test_rejected_flat_sweep_preserves_held_am_classification);
    RUN_TEST(test_deliver_custom_trigger);

    RUN_TEST(test_nearest_frequency_exact_match);
    RUN_TEST(test_nearest_frequency_midpoint_tie_picks_lower);
    RUN_TEST(test_nearest_frequency_between_values);
    RUN_TEST(test_nearest_frequency_below_and_above_range);
    RUN_TEST(test_nearest_frequency_skips_zeros);
    RUN_TEST(test_nearest_frequency_empty_or_all_zero);

    RUN_TEST(test_refine_plan_strongest_first_order);
    RUN_TEST(test_refine_plan_dedup_keeps_stronger);
    RUN_TEST(test_refine_plan_dedup_boundary_500khz);
    RUN_TEST(test_refine_plan_skips_out_of_band);
    RUN_TEST(test_refine_plan_respects_max_out);
    RUN_TEST(test_refine_plan_tie_keeps_first);
    RUN_TEST(test_refine_plan_null_and_empty);

    return UNITY_END();
}
