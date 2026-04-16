/* See COPYING.txt for license details. */

/**
 * @file   m1_system_dashboard_helpers.c
 * @brief  Pure-logic helpers for the system dashboard.
 *
 * Hardware-independent: compiled into both firmware and host-side unit tests.
 */

#include "m1_sdcard.h"
#include "m1_system_dashboard_helpers.h"
#include <stdio.h>


void dashboard_format_uptime(uint32_t uptime_ms, char *out, size_t out_len)
{
    uint32_t total_sec = uptime_ms / 1000U;
    uint32_t hours = total_sec / 3600U;
    uint32_t minutes = (total_sec / 60U) % 60U;
    uint32_t seconds = total_sec % 60U;

    if (hours >= 100U)
    {
        snprintf(out, out_len, "%luh %lum",
                 (unsigned long)hours, (unsigned long)minutes);
    }
    else
    {
        snprintf(out, out_len, "%02lu:%02lu:%02lu",
                 (unsigned long)hours,
                 (unsigned long)minutes,
                 (unsigned long)seconds);
    }
}


const char *dashboard_sd_status_text(S_M1_SDCard_Access_Status status)
{
    switch (status)
    {
        case SD_access_OK:
            return "Ready";
        case SD_access_NoFS:
            return "No FS";
        case SD_access_UnMounted:
            return "Unmounted";
        case SD_access_NotReady:
            return "No Card";
        case SD_access_NotOK:
        default:
            return "Error";
    }
}
