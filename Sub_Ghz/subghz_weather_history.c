/* See COPYING.txt for license details. */

/*
 * subghz_weather_history.c
 *
 * Pure sensor list for the Weather Station scene.  See the header for the
 * rationale.  No hardware, HAL or RTOS dependencies — fully host-testable.
 */

#include "subghz_weather_history.h"
#include <stddef.h>

void subghz_weather_history_reset(SubGhzWeatherHistory *h)
{
    if (h == NULL) {
        return;
    }
    h->count = 0;
}

int subghz_weather_history_add(SubGhzWeatherHistory *h,
                               const SubGhzWeatherSensor *in,
                               uint32_t now_ms)
{
    uint8_t i;
    uint8_t slot;

    if (h == NULL || in == NULL) {
        return -1;
    }

    /* Match on (protocol, serial, channel) — one row per physical sensor. */
    for (i = 0; i < h->count; i++) {
        if (h->items[i].protocol == in->protocol &&
            h->items[i].serial   == in->serial   &&
            h->items[i].channel  == in->channel) {
            uint16_t seen = h->items[i].count;
            h->items[i]              = *in;
            h->items[i].count        = (seen < 0xFFFFU) ? (uint16_t)(seen + 1U)
                                                        : 0xFFFFU;
            h->items[i].last_seen_ms = now_ms;
            return (int)i;
        }
    }

    if (h->count < WX_HISTORY_MAX) {
        slot = h->count++;
    } else {
        /* Table full — evict the least-recently-seen row. */
        uint32_t oldest_age = 0;
        slot = 0;
        for (i = 0; i < h->count; i++) {
            uint32_t age = now_ms - h->items[i].last_seen_ms;
            if (age >= oldest_age) {
                oldest_age = age;
                slot = i;
            }
        }
    }

    h->items[slot]              = *in;
    h->items[slot].count        = 1;
    h->items[slot].last_seen_ms = now_ms;
    return (int)slot;
}

const SubGhzWeatherSensor *subghz_weather_history_get(
    const SubGhzWeatherHistory *h, uint8_t idx)
{
    if (h == NULL || idx >= h->count) {
        return NULL;
    }
    return &h->items[idx];
}

uint8_t subghz_weather_history_age_min(const SubGhzWeatherSensor *s,
                                       uint32_t now_ms)
{
    uint32_t age_min;

    if (s == NULL) {
        return 0;
    }
    /* Unsigned subtraction handles monotonic-tick wrap-around correctly. */
    age_min = (now_ms - s->last_seen_ms) / 60000UL;
    return (age_min > 99UL) ? 99U : (uint8_t)age_min;
}

int16_t subghz_weather_c_to_f_d10(int16_t temp_c_d10)
{
    /* F = C * 9/5 + 32; both sides scaled by 10.  Round half away from zero
     * on the 9/5 conversion so display values match rtl_433 / Flipper. */
    int32_t v = (int32_t)temp_c_d10 * 9;
    v = (v >= 0) ? (v + 2) / 5 : (v - 2) / 5;
    v += 320;
    if (v > 32767) {
        v = 32767;
    }
    if (v < -32768) {
        v = -32768;
    }
    return (int16_t)v;
}
