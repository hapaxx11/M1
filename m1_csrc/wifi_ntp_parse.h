/* See COPYING.txt for license details. */

/**
 * @file   wifi_ntp_parse.h
 * @brief  Parse ESP-AT +CIPSNTPTIME response into structured time.
 *
 * Pure-logic module — no HAL, RTOS, or display dependencies.
 * Uses clock_time_t (identical layout to m1_time_t) so the caller can
 * directly memcpy into m1_time_t for RTC set.
 *
 * M1 Project
 */

#ifndef WIFI_NTP_PARSE_H_
#define WIFI_NTP_PARSE_H_

#include <stdbool.h>
#include "m1_clock_util.h"   /* clock_time_t */

/**
 * Parse an ESP-AT +CIPSNTPTIME response into a clock_time_t.
 *
 * The expected time string format is asctime:
 *   "Thu Jun 29 01:43:16 2026"
 *
 * The "+CIPSNTPTIME:" prefix is optional — if present in @p response,
 * parsing starts after it.  The function searches for the prefix first;
 * if not found, the entire string is treated as the time portion.
 *
 * @param response   Full AT response buffer (may contain other output).
 * @param out        Parsed time output (weekday: 1=Monday..7=Sunday).
 * @return true on success, false if the time string cannot be parsed
 *         or contains an epoch/1970 date (indicating SNTP not yet synced).
 */
bool wifi_ntp_parse_time(const char *response, clock_time_t *out);

#endif /* WIFI_NTP_PARSE_H_ */
