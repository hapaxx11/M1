/* See COPYING.txt for license details. */

#ifndef SUBGHZ_BRUTEFORCE_JOB_H
#define SUBGHZ_BRUTEFORCE_JOB_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    SUBGHZ_BRUTEFORCE_JOB_IDLE,
    SUBGHZ_BRUTEFORCE_JOB_RUNNING,
    SUBGHZ_BRUTEFORCE_JOB_FOUND,
    SUBGHZ_BRUTEFORCE_JOB_NOT_FOUND,
    SUBGHZ_BRUTEFORCE_JOB_CANCELLED,
} subghz_bruteforce_job_status_t;

typedef struct {
    uint32_t progress_current;
    uint32_t progress_total;
    uint32_t result;
    bool cancel;
    subghz_bruteforce_job_status_t status;
} subghz_bruteforce_job_t;

void subghz_bruteforce_job_init(subghz_bruteforce_job_t *job);
bool subghz_bruteforce_job_begin(subghz_bruteforce_job_t *job, uint32_t candidate_count);
bool subghz_bruteforce_job_advance(subghz_bruteforce_job_t *job, bool matched,
                                   uint32_t result);
void subghz_bruteforce_job_cancel(subghz_bruteforce_job_t *job);

#endif
