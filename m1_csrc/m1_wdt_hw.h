/* See COPYING.txt for license details. */

/*
 * m1_wdt_hw.h
 *
 * Lightweight header for the hardware watchdog kick function.
 * Declares only m1_wdt_reset(), which has no FreeRTOS type dependencies,
 * so it can be included by low-level drivers (e.g. m1_bq27421.c) that
 * must not pull in the full m1_watchdog.h / FreeRTOS type tree.
 *
 * M1 Project
 */

#ifndef M1_WDT_HW_H_
#define M1_WDT_HW_H_

void m1_wdt_reset(void);

#endif /* M1_WDT_HW_H_ */
