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
#include "m1_watchdog_decision.h"

void m1_wdt_init(void);
void m1_wdt_report_init(void);
void m1_wdt_send_report(S_M1_WDT_Report_ID rpt_id, uint32_t time);
void m1_wdt_send_report_ex(S_M1_WDT_Report_ID rpt_id, TickType_t start_time);
void m1_wdt_send_delayed_report(S_M1_WDT_Report_ID rpt_id, uint32_t delay_ms, uint8_t repeat);
void m1_wdt_suspend_task(S_M1_WDT_Report_ID rpt_id);
void m1_wdt_resume_task(S_M1_WDT_Report_ID rpt_id);

#endif /* M1_WATCHDOG_H_ */
