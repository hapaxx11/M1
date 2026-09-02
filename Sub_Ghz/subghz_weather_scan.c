/* See COPYING.txt for license details. */

/*
 * subghz_weather_scan.c
 *
 * Pure dwell state machine for the Momentum-style Weather Station RX scan
 * loop.  See subghz_weather_scan.h for the rationale.  No hardware, HAL or
 * RTOS dependencies — fully host-testable.
 */

#include "subghz_weather_scan.h"
#include <stddef.h>

SubGhzWeatherScanMod subghz_weather_scan_other(SubGhzWeatherScanMod m)
{
    return (m == WX_SCAN_MOD_OOK) ? WX_SCAN_MOD_FSK : WX_SCAN_MOD_OOK;
}

void subghz_weather_scan_init(SubGhzWeatherScan *s, uint32_t dwell_ms,
                              SubGhzWeatherScanMod start, bool dual,
                              uint32_t now_ms)
{
    if (s == NULL) {
        return;
    }
    s->dwell_ms       = (dwell_ms == 0U) ? 1U : dwell_ms;
    s->last_switch_ms = now_ms;
    s->mod            = start;
    s->dual           = dual;
}

bool subghz_weather_scan_tick(SubGhzWeatherScan *s, uint32_t now_ms)
{
    if (s == NULL || !s->dual) {
        return false;
    }

    /* Unsigned subtraction handles monotonic-tick wrap-around correctly. */
    uint32_t elapsed = now_ms - s->last_switch_ms;
    if (elapsed < s->dwell_ms) {
        return false;
    }

    s->mod            = subghz_weather_scan_other(s->mod);
    s->last_switch_ms = now_ms;
    return true;
}

const char *subghz_weather_scan_label(SubGhzWeatherScanMod m)
{
    return (m == WX_SCAN_MOD_FSK) ? "FSK" : "AM";
}
