/* See COPYING.txt for license details. */

#ifndef M1_WATCHDOG_DECISION_H_
#define M1_WATCHDOG_DECISION_H_

#include <stdint.h>
#include <stdbool.h>

typedef enum
{
    M1_REPORT_ID_BUTTONS_HANDLER_TASK = 0,
    M1_REPORT_ID_RPC_TASK,
    M1_REPORT_ID_END_OF_LIST
} S_M1_WDT_Report_ID;

typedef struct
{
    S_M1_WDT_Report_ID report_id;
    bool inactive;
    bool resume_grace;
    uint32_t report_period;
    uint32_t min_rpt_percent, max_rpt_percent;
    uint32_t run_time;
} S_M1_WDT_Report;

static inline void m1_wdt_set_task_inactive(S_M1_WDT_Report *report,
                                            bool inactive)
{
    report->inactive = inactive;
    report->resume_grace = !inactive;
}

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

static inline m1_wdt_action_t m1_wdt_decide_action_with_grace(
    uint32_t run_time,
    uint32_t min_run_time,
    uint32_t max_run_time,
    bool inactive,
    bool resume_grace)
{
    if (resume_grace && (run_time < min_run_time))
        return M1_WDT_ACTION_SUSPENDED;
    return m1_wdt_decide_action(run_time, min_run_time, max_run_time,
                                inactive);
}

static inline uint32_t m1_wdt_heartbeat_run_time(uint32_t elapsed_ms)
{
    return elapsed_ms;
}

#endif /* M1_WATCHDOG_DECISION_H_ */
