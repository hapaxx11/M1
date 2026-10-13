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
    const uint32_t candidates[] = {0x13579u, 0x24680u, 0xABCDEu};
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

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_synthetic_job_reports_match_and_bounded_progress);
    RUN_TEST(test_synthetic_job_reports_exhausted_range);
    RUN_TEST(test_synthetic_job_honors_cancellation);
    RUN_TEST(test_synthetic_job_handles_empty_range);
    return UNITY_END();
}
