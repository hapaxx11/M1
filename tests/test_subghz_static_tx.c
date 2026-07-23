/* See COPYING.txt for license details. */

/**
 * @file   test_subghz_static_tx.c
 * @brief  Host-side unit tests for the blocking static-code TX burst/wait policy.
 *
 * Regression coverage for issue #650 ("Subghz send broken again"): sending a
 * captured static-code signal (e.g. a Princeton remote) sat on the "Sending…"
 * overlay far longer than the frame takes because the completion wait polled a
 * counter that was never decremented on the direct-buffer path, always running
 * the full fixed safety timeout.  These tests pin the two pure decisions that
 * now drive the loop: total burst count and per-burst wait outcome.
 */

#include "unity.h"
#include "subghz_static_tx.h"

void setUp(void) { }
void tearDown(void) { }

/*============================================================================*/
/* Total burst count                                                          */
/*============================================================================*/

static void test_total_bursts_is_one_plus_extra(void)
{
    TEST_ASSERT_EQUAL_UINT16(1, subghz_static_tx_total_bursts(0));
    TEST_ASSERT_EQUAL_UINT16(5, subghz_static_tx_total_bursts(4));
    TEST_ASSERT_EQUAL_UINT16(256, subghz_static_tx_total_bursts(255));
}

/*============================================================================*/
/* Per-burst wait decision                                                    */
/*============================================================================*/

static void test_wait_continues_before_timeout(void)
{
    TEST_ASSERT_EQUAL(SUBGHZ_STATIC_TX_WAIT,
                      subghz_static_tx_wait_step(false, 0, 400));
    TEST_ASSERT_EQUAL(SUBGHZ_STATIC_TX_WAIT,
                      subghz_static_tx_wait_step(false, 398, 400));
}

static void test_completion_reports_done(void)
{
    TEST_ASSERT_EQUAL(SUBGHZ_STATIC_TX_DONE,
                      subghz_static_tx_wait_step(true, 0, 400));
    TEST_ASSERT_EQUAL(SUBGHZ_STATIC_TX_DONE,
                      subghz_static_tx_wait_step(true, 10, 400));
}

static void test_timeout_reported_when_window_elapses(void)
{
    TEST_ASSERT_EQUAL(SUBGHZ_STATIC_TX_TIMEOUT,
                      subghz_static_tx_wait_step(false, 400, 400));
    TEST_ASSERT_EQUAL(SUBGHZ_STATIC_TX_TIMEOUT,
                      subghz_static_tx_wait_step(false, 401, 400));
}

static void test_completion_wins_over_simultaneous_timeout(void)
{
    /* A burst that finishes exactly as the safety window expires must be
     * reported DONE, not TIMEOUT — otherwise a good send is misclassified. */
    TEST_ASSERT_EQUAL(SUBGHZ_STATIC_TX_DONE,
                      subghz_static_tx_wait_step(true, 400, 400));
    TEST_ASSERT_EQUAL(SUBGHZ_STATIC_TX_DONE,
                      subghz_static_tx_wait_step(true, 9999, 400));
}

static void test_zero_timeout_never_times_out(void)
{
    /* timeout_ms == 0 disables the safety cap: wait indefinitely for the DMA
     * completion regardless of how much time has elapsed. */
    TEST_ASSERT_EQUAL(SUBGHZ_STATIC_TX_WAIT,
                      subghz_static_tx_wait_step(false, 0, 0));
    TEST_ASSERT_EQUAL(SUBGHZ_STATIC_TX_WAIT,
                      subghz_static_tx_wait_step(false, 1000000, 0));
    TEST_ASSERT_EQUAL(SUBGHZ_STATIC_TX_DONE,
                      subghz_static_tx_wait_step(true, 1000000, 0));
}

/*============================================================================*/
/* Realistic loop simulation                                                  */
/*============================================================================*/

/**
 * Drive a full static-code send exactly as m1_sub_ghz.c's blocking loop does,
 * using a simulated per-burst DMA completion latency, and assert that it emits
 * the intended number of frames and terminates promptly (i.e. never falls back
 * to the old fixed 2-second dead-wait, which would show up here as a per-burst
 * TIMEOUT or an inflated elapsed time).
 */
static void run_send(uint8_t extra_repeats,
                     uint32_t burst_latency_ms,
                     uint32_t poll_ms,
                     uint32_t timeout_ms,
                     uint16_t *out_bursts,
                     uint32_t *out_total_ms,
                     int *out_timed_out)
{
    const uint16_t total = subghz_static_tx_total_bursts(extra_repeats);
    uint16_t emitted = 0;
    uint32_t total_ms = 0;
    int timed_out = 0;

    for (uint16_t burst = 0; burst < total; burst++)
    {
        emitted++;                 /* frame armed */
        uint32_t elapsed = 0;
        for (;;)
        {
            int done = (elapsed >= burst_latency_ms);
            subghz_static_tx_wait_t w =
                subghz_static_tx_wait_step((bool)done, elapsed, timeout_ms);
            if (w == SUBGHZ_STATIC_TX_DONE)
                break;
            if (w == SUBGHZ_STATIC_TX_TIMEOUT)
            {
                timed_out = 1;
                break;
            }
            elapsed  += poll_ms;
            total_ms += poll_ms;
        }
        if (timed_out)
            break;
    }

    if (out_bursts)   *out_bursts   = emitted;
    if (out_total_ms) *out_total_ms = total_ms;
    if (out_timed_out) *out_timed_out = timed_out;
}

static void test_send_emits_all_bursts_and_finishes_fast(void)
{
    uint16_t bursts = 0;
    uint32_t total_ms = 0;
    int timed_out = -1;

    /* 5 frames, each completing after ~30 ms, polled every 2 ms, 400 ms cap. */
    run_send(4, 30, 2, 400, &bursts, &total_ms, &timed_out);

    TEST_ASSERT_EQUAL_UINT16(5, bursts);      /* Princeton: 1 + 4 repeats */
    TEST_ASSERT_EQUAL_INT(0, timed_out);      /* no dead-wait / no stall   */
    /* ~5 * 30 ms waiting; must be nowhere near the old fixed 2 s hang. */
    TEST_ASSERT_TRUE(total_ms <= 200);
}

static void test_send_bails_out_when_dma_wedges(void)
{
    uint16_t bursts = 0;
    int timed_out = -1;

    /* Burst never completes (latency beyond any wait) — must time out on the
     * very first frame rather than looping forever. */
    run_send(4, 100000, 2, 40, &bursts, NULL, &timed_out);

    TEST_ASSERT_EQUAL_INT(1, timed_out);
    TEST_ASSERT_EQUAL_UINT16(1, bursts);      /* stopped on first burst */
}

/*============================================================================*/
/* Runner                                                                     */
/*============================================================================*/

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_total_bursts_is_one_plus_extra);
    RUN_TEST(test_wait_continues_before_timeout);
    RUN_TEST(test_completion_reports_done);
    RUN_TEST(test_timeout_reported_when_window_elapses);
    RUN_TEST(test_completion_wins_over_simultaneous_timeout);
    RUN_TEST(test_zero_timeout_never_times_out);
    RUN_TEST(test_send_emits_all_bursts_and_finishes_fast);
    RUN_TEST(test_send_bails_out_when_dma_wedges);
    return UNITY_END();
}
