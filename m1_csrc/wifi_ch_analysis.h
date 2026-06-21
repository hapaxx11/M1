/* wifi_ch_analysis.h — Pure-logic 2.4 GHz channel analysis helpers.
 *
 * Hardware-independent: uses only standard C types, no HAL / RTOS / display.
 * Extracted from wifi_survey_24g() in m1_wifi.c so the analysis logic can be
 * unit-tested on the host.
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef WIFI_CH_ANALYSIS_H
#define WIFI_CH_ANALYSIS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 2.4 GHz band has channels 1–13 (14 is Japan-only and rarely used). */
#define WIFI_CH_24G_MIN  1
#define WIFI_CH_24G_MAX  13
#define WIFI_CH_24G_SLOTS 14   /* index 0 unused; channels 1–13 */

/* Result of a 2.4 GHz channel analysis. */
typedef struct {
    uint8_t  ch_count[WIFI_CH_24G_SLOTS]; /* per-channel AP count (idx 0 unused) */
    uint16_t total_aps;        /* total APs with channel in [1, 13] */
    int8_t   strongest_rssi;   /* strongest RSSI across all APs, -128 if none */
    uint8_t  busiest_ch;       /* channel with the most APs */
    uint8_t  busiest_count;    /* AP count on the busiest channel */
    uint8_t  best_ch;          /* channel with the fewest APs (quietest) */
    uint8_t  best_count;       /* AP count on the quietest channel */
} wifi_ch_analysis_t;

/* Accumulate per-channel counts and track the strongest RSSI.
 *
 * @param channels  Array of channel numbers (one per AP).
 * @param rssis     Array of RSSI values (one per AP, same order).
 * @param n         Number of APs.
 * @param out       Output structure (zeroed, then populated).
 *
 * Channels outside [1, 13] are silently skipped.
 * If n == 0 or both arrays are NULL, out is zeroed with busiest_ch and
 * best_ch set to 1 and strongest_rssi to -128. */
void wifi_ch_analysis_compute(const uint8_t *channels,
                              const int8_t  *rssis,
                              uint16_t       n,
                              wifi_ch_analysis_t *out);

/* Compute a bar height for a chart of given pixel height.
 *
 * @param count     Number of APs on this channel.
 * @param max_count Maximum AP count across all channels (denominator).
 * @param chart_h   Total chart height in pixels.
 * @return          Bar height in pixels (0 if count==0; ≥1 if count>0). */
uint8_t wifi_ch_bar_height(uint8_t count, uint8_t max_count, uint8_t chart_h);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_CH_ANALYSIS_H */
