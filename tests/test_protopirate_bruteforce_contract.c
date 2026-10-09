/* See COPYING.txt for license details. */

/*
 * Synthetic lifecycle contract for future ProtoPirate brute-force cores.
 * This test exercises only fake candidates and contains no protocol frames,
 * key material, or cryptographic search logic.
 */

#include <stdbool.h>
#include <stdint.h>
#include "unity.h"

typedef enum {
    SYNTHETIC_JOB_IDLE,
    SYNTHETIC_JOB_RUNNING,
    SYNTHETIC_JOB_FOUND,
    SYNTHETIC_JOB_NOT_FOUND,
    SYNTHETIC_JOB_CANCELLED,
} SyntheticJobStatus;

typedef struct {
    uint32_t progress_current;
    uint32_t progress_total;
    uint32_t result;
    bool cancel;
    SyntheticJobStatus status;
} SyntheticJob;

static void run_synthetic_job(SyntheticJob *job, const uint32_t *candidates,
                              uint32_t candidate_count, uint32_t wanted)
{
    job->progress_current = 0;
    job->progress_total = candidate_count;
    job->result = 0;
    job->status = SYNTHETIC_JOB_RUNNING;

    for (uint32_t i = 0; i < candidate_count; i++) {
        if (job->cancel) {
            job->status = SYNTHETIC_JOB_CANCELLED;
            return;
        }
        if (candidates[i] == wanted) {
            job->result = candidates[i];
            job->progress_current = i + 1u;
            job->status = SYNTHETIC_JOB_FOUND;
            return;
        }
        job->progress_current = i + 1u;
    }

    job->status = SYNTHETIC_JOB_NOT_FOUND;
}

void setUp(void) {}
void tearDown(void) {}

void test_synthetic_job_reports_match_and_bounded_progress(void)
{
    const uint32_t candidates[] = {0x13579u, 0x24680u, 0xABCDEu};
    SyntheticJob job = {0};

    run_synthetic_job(&job, candidates, 3u, 0x24680u);

    TEST_ASSERT_EQUAL(SYNTHETIC_JOB_FOUND, job.status);
    TEST_ASSERT_EQUAL_HEX32(0x24680u, job.result);
    TEST_ASSERT_EQUAL_UINT32(2u, job.progress_current);
    TEST_ASSERT_EQUAL_UINT32(3u, job.progress_total);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(job.progress_total, job.progress_current);
}

void test_synthetic_job_reports_exhausted_range(void)
{
    const uint32_t candidates[] = {0x13579u, 0x24680u, 0xABCDEu};
    SyntheticJob job = {0};

    run_synthetic_job(&job, candidates, 3u, 0xFFFFFu);

    TEST_ASSERT_EQUAL(SYNTHETIC_JOB_NOT_FOUND, job.status);
    TEST_ASSERT_EQUAL_UINT32(0u, job.result);
    TEST_ASSERT_EQUAL_UINT32(job.progress_total, job.progress_current);
}

void test_synthetic_job_honors_cancellation(void)
{
    const uint32_t candidates[] = {0x13579u, 0x24680u, 0xABCDEu};
    SyntheticJob job = {.cancel = true};

    run_synthetic_job(&job, candidates, 3u, 0xABCDEu);

    TEST_ASSERT_EQUAL(SYNTHETIC_JOB_CANCELLED, job.status);
    TEST_ASSERT_EQUAL_UINT32(0u, job.progress_current);
    TEST_ASSERT_EQUAL_UINT32(3u, job.progress_total);
    TEST_ASSERT_EQUAL_UINT32(0u, job.result);
}

void test_synthetic_job_handles_empty_range(void)
{
    SyntheticJob job = {0};

    run_synthetic_job(&job, NULL, 0u, 0u);

    TEST_ASSERT_EQUAL(SYNTHETIC_JOB_NOT_FOUND, job.status);
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
