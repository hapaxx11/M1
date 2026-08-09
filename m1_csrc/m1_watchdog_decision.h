/* See COPYING.txt for license details. */

#ifndef M1_WATCHDOG_DECISION_H_
#define M1_WATCHDOG_DECISION_H_

#include <stdint.h>
#include <stdbool.h>

/* Outcome of a single per-task watchdog report check. The caller decides this
 * value while holding a critical section (a pure, atomic read-modify of the
 * report fields), then performs any logging/failure-handling AFTER releasing
 * the critical section — M1_LOG_* takes a mutex + queue, which must never be
 * invoked with the scheduler suspended. */
typedef enum
{
    M1_WDT_ACTION_OK = 0,
    M1_WDT_ACTION_SUSPENDED,
    M1_WDT_ACTION_FAILURE
} m1_wdt_action_t;

/* Pure decision: given a task's accumulated run_time this check period, its
 * inactive flag, and the min/max acceptable run_time bounds, decide whether
 * the task is healthy, intentionally suspended, or has failed to report. */
static inline m1_wdt_action_t m1_wdt_decide_action(
    uint32_t run_time,
    uint32_t min_run_time,
    uint32_t max_run_time,
    bool inactive)
{
    if ((run_time >= min_run_time) && (run_time <= max_run_time))
    {
        return M1_WDT_ACTION_OK;
    }
    if (inactive)
    {
        return M1_WDT_ACTION_SUSPENDED;
    }
    return M1_WDT_ACTION_FAILURE;
}

#endif /* M1_WATCHDOG_DECISION_H_ */
