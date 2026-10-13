/* See COPYING.txt for license details. */

#include "subghz_bruteforce_job.h"

#include <stddef.h>

/* TODO: connect a future generic worker adapter here; protocol-specific search
 * and verification remain outside this lifecycle module. */

void subghz_bruteforce_job_init(subghz_bruteforce_job_t *job)
{
    if (job == NULL)
        return;

    job->progress_current = 0;
    job->progress_total = 0;
    job->result = 0;
    job->cancel = false;
    job->status = SUBGHZ_BRUTEFORCE_JOB_IDLE;
}

bool subghz_bruteforce_job_begin(subghz_bruteforce_job_t *job, uint32_t candidate_count)
{
    if (job == NULL)
        return false;

    job->progress_current = 0;
    job->progress_total = candidate_count;
    job->result = 0;
    job->cancel = false;
    job->status = candidate_count == 0
                      ? SUBGHZ_BRUTEFORCE_JOB_NOT_FOUND
                      : SUBGHZ_BRUTEFORCE_JOB_RUNNING;
    return true;
}

bool subghz_bruteforce_job_advance(subghz_bruteforce_job_t *job, bool matched,
                                   uint32_t result)
{
    if (job == NULL || job->status != SUBGHZ_BRUTEFORCE_JOB_RUNNING)
        return false;

    if (job->cancel) {
        job->status = SUBGHZ_BRUTEFORCE_JOB_CANCELLED;
        return true;
    }

    if (job->progress_current >= job->progress_total) {
        job->status = SUBGHZ_BRUTEFORCE_JOB_NOT_FOUND;
        return false;
    }

    job->progress_current++;
    if (matched) {
        job->result = result;
        job->status = SUBGHZ_BRUTEFORCE_JOB_FOUND;
    } else if (job->progress_current == job->progress_total) {
        job->status = SUBGHZ_BRUTEFORCE_JOB_NOT_FOUND;
    }
    return true;
}

void subghz_bruteforce_job_cancel(subghz_bruteforce_job_t *job)
{
    if (job != NULL && job->status == SUBGHZ_BRUTEFORCE_JOB_RUNNING)
        job->cancel = true;
}
