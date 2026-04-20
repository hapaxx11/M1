/* See COPYING.txt for license details. */

/**
 * @file   m1_clock_util.h
 * @brief  Pure-logic clock utility functions — timezone offset, date math.
 *
 * Hardware-independent module.  All functions are pure (no side effects,
 * no HAL/RTOS dependencies) and can be tested on the host.
 */

#ifndef M1_CLOCK_UTIL_H_
#define M1_CLOCK_UTIL_H_

#include <stdint.h>
#include <stdbool.h>

/*============================================================================*/
/* Timezone descriptor                                                        */
/*============================================================================*/

typedef struct {
    const char *label;
    int8_t      offset_hours;
} clock_zone_t;

/*============================================================================*/
/* Minimal time struct for clock math (mirrors m1_time_t layout)              */
/*============================================================================*/

typedef struct {
    uint16_t year;    /* 2000..2099 */
    uint8_t  month;   /* 1..12     */
    uint8_t  day;     /* 1..31     */
    uint8_t  hour;    /* 0..23     */
    uint8_t  minute;  /* 0..59     */
    uint8_t  second;  /* 0..59     */
    uint8_t  weekday; /* 1..7 (Monday=1) */
} clock_time_t;

/*============================================================================*/
/* Pure functions                                                             */
/*============================================================================*/

/** Return true if @p year is a leap year (Gregorian). */
bool clock_is_leap_year(uint16_t year);

/** Return the number of days in @p month of @p year (1-based month). */
uint8_t clock_days_in_month(uint16_t year, uint8_t month);

/**
 * @brief  Adjust the date by @p delta_days (positive = forward, negative = back).
 *         Also adjusts weekday if it is in the valid 1..7 range.
 */
void clock_adjust_days(clock_time_t *dt, int8_t delta_days);

/**
 * @brief  Apply an hour offset to @p src and write the result to @p dst.
 *         Handles day rollover in both directions.
 */
void clock_apply_offset(const clock_time_t *src, int8_t offset_hours,
                        clock_time_t *dst);

/**
 * @brief  Format a UTC offset as "UTC", "UTC+N", or "UTC-N" into @p buf.
 * @param  offset_hours  UTC offset in whole hours (e.g. -12..+14)
 * @param  buf           Output buffer (at least 8 bytes recommended)
 * @param  buf_size      Size of @p buf
 */
void clock_tz_label(int8_t offset_hours, char *buf, uint8_t buf_size);

#endif /* M1_CLOCK_UTIL_H_ */
