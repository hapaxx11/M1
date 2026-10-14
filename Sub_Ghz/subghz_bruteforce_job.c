/* See COPYING.txt for license details. */

#include "subghz_bruteforce_job.h"

#include <stddef.h>

/* TODO: connect a future generic worker adapter here; protocol-specific search
 * and verification remain outside this lifecycle module. */

void subghz_bruteforce_job_init(subghz_bruteforce_job_t *job)
{
    if (job == NULL)
        return;

    atomic_init(&job->progress_current, 0);
    atomic_init(&job->progress_total, 0);
    atomic_init(&job->result, 0);
    atomic_init(&job->cancel, false);
    atomic_init(&job->status, SUBGHZ_BRUTEFORCE_JOB_IDLE);
}

bool subghz_bruteforce_job_begin(subghz_bruteforce_job_t *job, uint32_t candidate_count)
{
    if (job == NULL)
        return false;

    atomic_store(&job->progress_current, 0);
    atomic_store(&job->progress_total, candidate_count);
    atomic_store(&job->result, 0);
    atomic_store(&job->cancel, false);
    atomic_store(&job->status, candidate_count == 0
                                   ? SUBGHZ_BRUTEFORCE_JOB_NOT_FOUND
                                   : SUBGHZ_BRUTEFORCE_JOB_RUNNING);
    return true;
}

bool subghz_bruteforce_job_advance(subghz_bruteforce_job_t *job, bool matched,
                                   uint32_t result)
{
    if (job == NULL ||
        atomic_load(&job->status) != SUBGHZ_BRUTEFORCE_JOB_RUNNING)
        return false;

    if (atomic_load(&job->cancel)) {
        atomic_store(&job->status, SUBGHZ_BRUTEFORCE_JOB_CANCELLED);
        return true;
    }

    const uint32_t current = atomic_load(&job->progress_current);
    const uint32_t total = atomic_load(&job->progress_total);
    if (current >= total) {
        atomic_store(&job->status, SUBGHZ_BRUTEFORCE_JOB_NOT_FOUND);
        return false;
    }

    atomic_store(&job->progress_current, current + 1U);
    if (matched) {
        atomic_store(&job->result, result);
        atomic_store(&job->status, SUBGHZ_BRUTEFORCE_JOB_FOUND);
    } else if (current + 1U == total) {
        atomic_store(&job->status, SUBGHZ_BRUTEFORCE_JOB_NOT_FOUND);
    }
    return true;
}

void subghz_bruteforce_job_cancel(subghz_bruteforce_job_t *job)
{
    if (job != NULL &&
        atomic_load(&job->status) == SUBGHZ_BRUTEFORCE_JOB_RUNNING)
        atomic_store(&job->cancel, true);
}

void subghz_bruteforce_job_progress_snapshot(const subghz_bruteforce_job_t *job,
                                             uint32_t *current,
                                             uint32_t *total)
{
    if (current == NULL || total == NULL)
        return;

    *current = 0;
    *total = 0;
    if (job == NULL)
        return;

    *total = atomic_load(&job->progress_total);
    *current = atomic_load(&job->progress_current);
}
