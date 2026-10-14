/* See COPYING.txt for license details. */

/*
 * Synthetic lifecycle contract for future ProtoPirate brute-force cores.
 * This test exercises only fake candidates and contains no protocol frames,
 * key material, or cryptographic search logic.
 */

#include <stdbool.h>
#include <stdint.h>
#include "unity.h"
#include "subghz_bruteforce_job.h"

static void run_synthetic_job(subghz_bruteforce_job_t *job, const uint32_t *candidates,
                              uint32_t candidate_count, uint32_t wanted)
{
    TEST_ASSERT_TRUE(subghz_bruteforce_job_begin(job, candidate_count));

    for (uint32_t i = 0; i < candidate_count; i++) {
        bool matched = candidates[i] == wanted;
        TEST_ASSERT_TRUE(subghz_bruteforce_job_advance(
            job, matched, matched ? candidates[i] : 0u));
        if (job->status != SUBGHZ_BRUTEFORCE_JOB_RUNNING)
            return;
    }
}

void setUp(void) {}
void tearDown(void) {}

void test_synthetic_job_reports_match_and_bounded_progress(void)
{
    const uint32_t candidates[] = {0x13579u, 0x24680u, 0xABCDEu};
    subghz_bruteforce_job_t job;
    subghz_bruteforce_job_init(&job);

    run_synthetic_job(&job, candidates, 3u, 0x24680u);

    TEST_ASSERT_EQUAL(SUBGHZ_BRUTEFORCE_JOB_FOUND, job.status);
    TEST_ASSERT_EQUAL_HEX32(0x24680u, job.result);
    TEST_ASSERT_EQUAL_UINT32(2u, job.progress_current);
    TEST_ASSERT_EQUAL_UINT32(3u, job.progress_total);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(job.progress_total, job.progress_current);
}

void test_synthetic_job_reports_exhausted_range(void)
{
    const uint32_t candidates[] = {0x13579u, 0x24680u, 0xABCDEu};
    subghz_bruteforce_job_t job;
    subghz_bruteforce_job_init(&job);

    run_synthetic_job(&job, candidates, 3u, 0xFFFFFu);

    TEST_ASSERT_EQUAL(SUBGHZ_BRUTEFORCE_JOB_NOT_FOUND, job.status);
    TEST_ASSERT_EQUAL_UINT32(0u, job.result);
    TEST_ASSERT_EQUAL_UINT32(job.progress_total, job.progress_current);
}

void test_synthetic_job_honors_cancellation(void)
{
    subghz_bruteforce_job_t job;
    subghz_bruteforce_job_init(&job);
    TEST_ASSERT_TRUE(subghz_bruteforce_job_begin(&job, 3u));
    subghz_bruteforce_job_cancel(&job);

    TEST_ASSERT_TRUE(subghz_bruteforce_job_advance(&job, false, 0u));

    TEST_ASSERT_EQUAL(SUBGHZ_BRUTEFORCE_JOB_CANCELLED, job.status);
    TEST_ASSERT_EQUAL_UINT32(0u, job.progress_current);
    TEST_ASSERT_EQUAL_UINT32(3u, job.progress_total);
    TEST_ASSERT_EQUAL_UINT32(0u, job.result);
}

void test_synthetic_job_handles_empty_range(void)
{
    subghz_bruteforce_job_t job;
    subghz_bruteforce_job_init(&job);

    run_synthetic_job(&job, NULL, 0u, 0u);

    TEST_ASSERT_EQUAL(SUBGHZ_BRUTEFORCE_JOB_NOT_FOUND, job.status);
    TEST_ASSERT_EQUAL_UINT32(0u, job.progress_current);
    TEST_ASSERT_EQUAL_UINT32(0u, job.progress_total);
}

void test_job_init_clears_every_field(void)
{
    subghz_bruteforce_job_t job = {
        .progress_current = 8u,
        .progress_total = 9u,
        .result = 0x1234u,
        .cancel = true,
        .status = SUBGHZ_BRUTEFORCE_JOB_FOUND,
    };

    subghz_bruteforce_job_init(&job);

    TEST_ASSERT_EQUAL(SUBGHZ_BRUTEFORCE_JOB_IDLE, job.status);
    TEST_ASSERT_EQUAL_UINT32(0u, job.progress_current);
    TEST_ASSERT_EQUAL_UINT32(0u, job.progress_total);
    TEST_ASSERT_EQUAL_UINT32(0u, job.result);
    TEST_ASSERT_FALSE(job.cancel);
}

void test_begin_restarts_terminal_job_and_clears_cancel(void)
{
    subghz_bruteforce_job_t job = {
        .progress_current = 3u,
        .progress_total = 3u,
        .result = 0xABCDEu,
        .cancel = true,
        .status = SUBGHZ_BRUTEFORCE_JOB_FOUND,
    };

    TEST_ASSERT_TRUE(subghz_bruteforce_job_begin(&job, 2u));

    TEST_ASSERT_EQUAL(SUBGHZ_BRUTEFORCE_JOB_RUNNING, job.status);
    TEST_ASSERT_EQUAL_UINT32(0u, job.progress_current);
    TEST_ASSERT_EQUAL_UINT32(2u, job.progress_total);
    TEST_ASSERT_EQUAL_UINT32(0u, job.result);
    TEST_ASSERT_FALSE(job.cancel);
}

void test_zero_result_can_be_a_match_and_terminal_state_is_unchanged(void)
{
    subghz_bruteforce_job_t job;
    subghz_bruteforce_job_init(&job);
    TEST_ASSERT_TRUE(subghz_bruteforce_job_begin(&job, 2u));
    TEST_ASSERT_TRUE(subghz_bruteforce_job_advance(&job, false, 0u));
    TEST_ASSERT_TRUE(subghz_bruteforce_job_advance(&job, true, 0u));

    TEST_ASSERT_EQUAL(SUBGHZ_BRUTEFORCE_JOB_FOUND, job.status);
    TEST_ASSERT_EQUAL_UINT32(0u, job.result);
    TEST_ASSERT_EQUAL_UINT32(2u, job.progress_current);
    TEST_ASSERT_FALSE(subghz_bruteforce_job_advance(&job, false, 0u));
    TEST_ASSERT_EQUAL(SUBGHZ_BRUTEFORCE_JOB_FOUND, job.status);
    TEST_ASSERT_EQUAL_UINT32(2u, job.progress_current);
}

void test_cancellation_is_observed_on_next_advance_and_wins_match(void)
{
    subghz_bruteforce_job_t job;
    subghz_bruteforce_job_init(&job);
    TEST_ASSERT_TRUE(subghz_bruteforce_job_begin(&job, 3u));
    TEST_ASSERT_TRUE(subghz_bruteforce_job_advance(&job, false, 0u));
    subghz_bruteforce_job_cancel(&job);

    TEST_ASSERT_EQUAL(SUBGHZ_BRUTEFORCE_JOB_RUNNING, job.status);
    TEST_ASSERT_EQUAL_UINT32(1u, job.progress_current);
    TEST_ASSERT_TRUE(job.cancel);
    TEST_ASSERT_TRUE(subghz_bruteforce_job_advance(&job, true, 0xABCDEu));

    TEST_ASSERT_EQUAL(SUBGHZ_BRUTEFORCE_JOB_CANCELLED, job.status);
    TEST_ASSERT_EQUAL_UINT32(1u, job.progress_current);
    TEST_ASSERT_EQUAL_UINT32(0u, job.result);
    TEST_ASSERT_FALSE(subghz_bruteforce_job_advance(&job, false, 0u));
}

void test_progress_snapshot_reports_current_and_total(void)
{
    subghz_bruteforce_job_t job;
    uint32_t current;
    uint32_t total;

    subghz_bruteforce_job_init(&job);
    TEST_ASSERT_TRUE(subghz_bruteforce_job_begin(&job, 3u));
    TEST_ASSERT_TRUE(subghz_bruteforce_job_advance(&job, false, 0u));

    subghz_bruteforce_job_progress_snapshot(&job, &current, &total);

    TEST_ASSERT_EQUAL_UINT32(1u, current);
    TEST_ASSERT_EQUAL_UINT32(3u, total);
}

void test_null_job_arguments_are_safe(void)
{
    subghz_bruteforce_job_init(NULL);
    subghz_bruteforce_job_cancel(NULL);
    TEST_ASSERT_FALSE(subghz_bruteforce_job_begin(NULL, 1u));
    TEST_ASSERT_FALSE(subghz_bruteforce_job_advance(NULL, false, 0u));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_synthetic_job_reports_match_and_bounded_progress);
    RUN_TEST(test_synthetic_job_reports_exhausted_range);
    RUN_TEST(test_synthetic_job_honors_cancellation);
    RUN_TEST(test_synthetic_job_handles_empty_range);
    RUN_TEST(test_job_init_clears_every_field);
    RUN_TEST(test_begin_restarts_terminal_job_and_clears_cancel);
    RUN_TEST(test_zero_result_can_be_a_match_and_terminal_state_is_unchanged);
    RUN_TEST(test_cancellation_is_observed_on_next_advance_and_wins_match);
    RUN_TEST(test_progress_snapshot_reports_current_and_total);
    RUN_TEST(test_null_job_arguments_are_safe);
    return UNITY_END();
}
