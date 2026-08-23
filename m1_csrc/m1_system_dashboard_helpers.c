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
#include <string.h>
#include <ctype.h>


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


void dashboard_split_rpc_wallclock_suffix(const char *line,
                                          char *base, size_t base_len,
                                          char *suffix, size_t suffix_len)
{
    const char *last_space;
    const char *tok;
    size_t base_copy_len = 0;
    int is_wallclock_tok = 0;

    if (suffix != NULL && suffix_len > 0U)
    {
        suffix[0] = '\0';
    }
    if (base == NULL || base_len == 0U)
    {
        return;
    }
    if (line == NULL)
    {
        base[0] = '\0';
        return;
    }

    /* The wall-clock suffix, when present, is always the last space-
     * separated token and matches "t<digits>s" (see
     * m1_esp32_rpc_call_diag_format()). */
    last_space = strrchr(line, ' ');
    if (last_space != NULL)
    {
        tok = last_space + 1;
        if (tok[0] == 't' && tok[1] != '\0')
        {
            size_t tok_len = strlen(tok);
            if (tok[tok_len - 1U] == 's')
            {
                size_t i;
                is_wallclock_tok = 1;
                for (i = 1U; i + 1U < tok_len; i++)
                {
                    if (!isdigit((unsigned char)tok[i]))
                    {
                        is_wallclock_tok = 0;
                        break;
                    }
                }
                if (tok_len < 3U) /* need at least "tNs" */
                {
                    is_wallclock_tok = 0;
                }
            }
        }
    }

    if (is_wallclock_tok)
    {
        base_copy_len = (size_t)(last_space - line);
        if (suffix != NULL && suffix_len > 0U)
        {
            snprintf(suffix, suffix_len, "%s", last_space + 1);
        }
    }
    else
    {
        base_copy_len = strlen(line);
    }

    if (base_copy_len >= base_len)
    {
        base_copy_len = base_len - 1U;
    }
    memcpy(base, line, base_copy_len);
    base[base_copy_len] = '\0';
}
