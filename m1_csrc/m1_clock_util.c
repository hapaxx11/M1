/* See COPYING.txt for license details. */

/**
 * @file   m1_clock_util.c
 * @brief  Pure-logic clock utility functions — timezone offset, date math.
 *
 * Hardware-independent.  No HAL, RTOS, or display dependencies.
 */

#include "m1_clock_util.h"
#include <stddef.h>

/*============================================================================*/
/* Leap year                                                                  */
/*============================================================================*/

bool clock_is_leap_year(uint16_t year)
{
    return ((year % 4U) == 0U && ((year % 100U) != 0U || (year % 400U) == 0U));
}

/*============================================================================*/
/* Days in month                                                              */
/*============================================================================*/

uint8_t clock_days_in_month(uint16_t year, uint8_t month)
{
    static const uint8_t month_days[] = {
        31U, 28U, 31U, 30U, 31U, 30U, 31U, 31U, 30U, 31U, 30U, 31U
    };

    if (month == 2U && clock_is_leap_year(year))
        return 29U;

    if (month >= 1U && month <= 12U)
        return month_days[month - 1U];

    return 30U;  /* fallback for invalid month */
}

/*============================================================================*/
/* Day adjustment (forward / backward)                                        */
/*============================================================================*/

void clock_adjust_days(clock_time_t *dt, int8_t delta_days)
{
    if (dt == NULL || delta_days == 0)
        return;

    while (delta_days > 0)
    {
        uint8_t dim = clock_days_in_month(dt->year, dt->month);
        if (dt->day < dim)
        {
            dt->day++;
        }
        else
        {
            dt->day = 1U;
            if (dt->month < 12U)
                dt->month++;
            else
            {
                dt->month = 1U;
                dt->year++;
            }
        }

        if (dt->weekday >= 1U && dt->weekday <= 7U)
            dt->weekday = (dt->weekday == 7U) ? 1U : (uint8_t)(dt->weekday + 1U);

        delta_days--;
    }

    while (delta_days < 0)
    {
        if (dt->day > 1U)
        {
            dt->day--;
        }
        else
        {
            if (dt->month > 1U)
            {
                dt->month--;
                dt->day = clock_days_in_month(dt->year, dt->month);
            }
            else if (dt->year > 2000U)
            {
                dt->month = 12U;
                dt->year--;
                dt->day = clock_days_in_month(dt->year, dt->month);
            }
            else
            {
                /* Clamp at the minimum supported date: 2000-01-01. */
                break;
            }
        }

        if (dt->weekday >= 1U && dt->weekday <= 7U)
            dt->weekday = (dt->weekday == 1U) ? 7U : (uint8_t)(dt->weekday - 1U);

        delta_days++;
    }
}

/*============================================================================*/
/* Apply timezone offset                                                      */
/*============================================================================*/

void clock_apply_offset(const clock_time_t *src, int8_t offset_hours,
                        clock_time_t *dst)
{
    int16_t hour;

    if (src == NULL || dst == NULL)
        return;

    *dst = *src;
    hour = (int16_t)src->hour + (int16_t)offset_hours;

    while (hour < 0)
    {
        hour += 24;
        clock_adjust_days(dst, -1);
    }

    while (hour >= 24)
    {
        hour -= 24;
        clock_adjust_days(dst, 1);
    }

    dst->hour = (uint8_t)hour;
}
