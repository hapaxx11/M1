/* wifi_ch_analysis.c — Pure-logic 2.4 GHz channel analysis helpers.
 *
 * Extracted from wifi_survey_24g() in m1_wifi.c.  Zero hardware dependencies.
 * See wifi_ch_analysis.h for the public interface.
 *
 * SPDX-License-Identifier: MIT
 */

#include "wifi_ch_analysis.h"
#include <string.h>  /* memset */

void wifi_ch_analysis_compute(const uint8_t *channels,
                              const int8_t  *rssis,
                              uint16_t       n,
                              wifi_ch_analysis_t *out)
{
    if (!out)
        return;

    memset(out, 0, sizeof(*out));
    out->strongest_rssi = -128;
    out->busiest_ch     = 1;
    out->best_ch        = 1;

    if (!channels || !rssis || n == 0)
        return;

    /* Accumulate per-channel counts and track strongest RSSI */
    for (uint16_t i = 0; i < n; i++)
    {
        uint8_t ch = channels[i];
        if (ch >= WIFI_CH_24G_MIN && ch <= WIFI_CH_24G_MAX)
        {
            out->ch_count[ch]++;
            out->total_aps++;
        }
        if (rssis[i] > out->strongest_rssi)
            out->strongest_rssi = rssis[i];
    }

    /* Find busiest and quietest channels */
    uint8_t max_count = 0;
    uint8_t min_count = 255;

    for (uint8_t ch = WIFI_CH_24G_MIN; ch <= WIFI_CH_24G_MAX; ch++)
    {
        if (out->ch_count[ch] > max_count)
        {
            max_count        = out->ch_count[ch];
            out->busiest_ch  = ch;
        }
        if (out->ch_count[ch] < min_count)
        {
            min_count     = out->ch_count[ch];
            out->best_ch  = ch;
        }
    }

    out->busiest_count = max_count;
    out->best_count    = min_count;
}

uint8_t wifi_ch_bar_height(uint8_t count, uint8_t max_count, uint8_t chart_h)
{
    if (count == 0)
        return 0;
    if (max_count == 0)
        max_count = 1;

    uint8_t h = (uint8_t)((uint16_t)count * chart_h / max_count);
    if (h == 0)
        h = 1; /* at least 1 px for non-zero counts */
    return h;
}
