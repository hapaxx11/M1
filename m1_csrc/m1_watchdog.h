/* See COPYING.txt for license details. */

/*
*
* m1_watchdog.h
*
* Watchdog functions
*
* M1 Project
*
*/

#ifndef M1_WATCHDOG_H_
#define M1_WATCHDOG_H_

#include "FreeRTOS.h"
#include "m1_wdt_hw.h"

typedef enum
{
	M1_REPORT_ID_BUTTONS_HANDLER_TASK = 0,
	/* m1_rpc_task: supervised background task. It loops on a 100ms notify-wait,
	 * so a healthy loop reports frequently; the only legitimate multi-second
	 * block is a deferred ESP32 flash op (connect/erase/verify), around which
	 * it suspends its own report (m1_wdt_suspend_task/m1_wdt_resume_task).
	 * Registered inactive at boot; the task activates itself when it starts
	 * looping. */
	M1_REPORT_ID_RPC_TASK,
	// More tasks here if needed
	M1_REPORT_ID_END_OF_LIST
} S_M1_WDT_Report_ID;

typedef struct
{
	S_M1_WDT_Report_ID report_id;
	bool inactive;
	uint32_t report_period;
	uint32_t min_rpt_percent, max_rpt_percent;
	uint32_t run_time;
} S_M1_WDT_Report;

void m1_wdt_init(void);
void m1_wdt_report_init(void);
void m1_wdt_send_report(S_M1_WDT_Report_ID rpt_id, uint32_t time);
void m1_wdt_send_report_ex(S_M1_WDT_Report_ID rpt_id, TickType_t start_time);
void m1_wdt_send_delayed_report(S_M1_WDT_Report_ID rpt_id, uint32_t delay_ms, uint8_t repeat);
void m1_wdt_suspend_task(S_M1_WDT_Report_ID rpt_id);
void m1_wdt_resume_task(S_M1_WDT_Report_ID rpt_id);

#endif /* M1_WATCHDOG_H_ */
