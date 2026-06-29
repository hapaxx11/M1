/* See COPYING.txt for license details. */

/**
 * @file   wifi_ntp_parse.c
 * @brief  Parse ESP-AT +CIPSNTPTIME response into structured time.
 *
 * Hardware-independent — testable on the host.
 *
 * M1 Project
 */

#include "wifi_ntp_parse.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* =========================================================================
 * Month / weekday lookup tables
 * =========================================================================*/

static const char * const s_months[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

/* asctime day abbreviations → ISO 8601 weekday (1=Monday..7=Sunday) */
static const struct { const char *abbr; uint8_t iso; } s_weekdays[] = {
    { "Mon", 1 }, { "Tue", 2 }, { "Wed", 3 }, { "Thu", 4 },
    { "Fri", 5 }, { "Sat", 6 }, { "Sun", 7 }
};

/* =========================================================================
 * Public API
 * =========================================================================*/

bool wifi_ntp_parse_time(const char *response, clock_time_t *out)
{
    const char *p;
    char day_name[4];
    char mon_name[4];
    int day, hour, minute, second, year;
    uint8_t month = 0;
    uint8_t weekday = 0;

    if (!response || !out)
        return false;

    /* Locate the time string.  If "+CIPSNTPTIME:" is present, start after it;
     * otherwise parse from the beginning. */
    p = strstr(response, "+CIPSNTPTIME:");
    if (p)
        p += 13;  /* strlen("+CIPSNTPTIME:") */
    else
        p = response;

    /* Skip leading whitespace */
    while (*p == ' ' || *p == '\t')
        p++;

    /* Epoch/unsynced (year 1970) is rejected by the year range check below. */

    /* Parse asctime format: "Day Mon DD HH:MM:SS YYYY"
     * sscanf with %3s captures exactly 3 chars for day/month name. */
    if (sscanf(p, "%3s %3s %d %d:%d:%d %d",       /* NOLINT */
               day_name, mon_name, &day,
               &hour, &minute, &second, &year) != 7)
        return false;

    /* Validate ranges */
    if (year < 2000 || year > 2099)
        return false;
    if (day < 1 || day > 31)
        return false;
    if (hour < 0 || hour > 23)
        return false;
    if (minute < 0 || minute > 59)
        return false;
    if (second < 0 || second > 59)
        return false;

    /* Look up month */
    for (uint8_t i = 0; i < 12; i++)
    {
        if (strncmp(mon_name, s_months[i], 3) == 0)
        {
            month = i + 1;
            break;
        }
    }
    if (month == 0)
        return false;

    /* Look up weekday */
    for (uint8_t i = 0; i < 7; i++)
    {
        if (strncmp(day_name, s_weekdays[i].abbr, 3) == 0)
        {
            weekday = s_weekdays[i].iso;
            break;
        }
    }
    if (weekday == 0)
        return false;

    memset(out, 0, sizeof(*out));
    out->year    = (uint16_t)year;
    out->month   = month;
    out->day     = (uint8_t)day;
    out->hour    = (uint8_t)hour;
    out->minute  = (uint8_t)minute;
    out->second  = (uint8_t)second;
    out->weekday = weekday;

    return true;
}
