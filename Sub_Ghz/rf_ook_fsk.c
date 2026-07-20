/* See COPYING.txt for license details. */

/*
 * rf_ook_fsk.c
 *
 * Pure-logic OOK-vs-FSK classifier from an RSSI burst.  See rf_ook_fsk.h.
 *
 * M1 Project — Hapax fork
 */

#include "rf_ook_fsk.h"

#include <stddef.h>   /* NULL */

rf_ook_fsk_result_t rf_ook_fsk_classify(const int16_t *rssi, uint16_t n)
{
    rf_ook_fsk_result_t r = { RF_MOD_UNKNOWN, 0U, 0 };

    if (rssi == NULL || n < RF_OOK_FSK_MIN_SAMPLES)
        return r;

    /* Pass 1: min / max / mean. */
    int16_t lo = rssi[0];
    int16_t hi = rssi[0];
    int32_t sum = 0;
    for (uint16_t i = 0; i < n; i++) {
        if (rssi[i] < lo) lo = rssi[i];
        if (rssi[i] > hi) hi = rssi[i];
        sum += rssi[i];
    }

    int16_t spread = (int16_t)(hi - lo);
    r.spread_db = spread;

    /* Steady carrier → FSK/FM. */
    if (spread <= RF_OOK_FSK_FSK_SPREAD_DB) {
        r.mod = RF_MOD_FSK;
        /* A very flat trace is a stronger FSK/carrier signal. */
        r.confidence = (spread <= (RF_OOK_FSK_FSK_SPREAD_DB / 2)) ? 3U : 2U;
        return r;
    }

    /* Large spread → candidate OOK; confirm the trace is bimodal (samples
     * cluster near both the floor and the ceiling) rather than a smooth ramp,
     * which distinguishes amplitude keying from a slowly drifting carrier. */
    if (spread >= RF_OOK_FSK_OOK_SPREAD_DB) {
        int16_t lo_band = (int16_t)(lo + spread / 4);   /* bottom quartile */
        int16_t hi_band = (int16_t)(hi - spread / 4);   /* top quartile */
        uint16_t low_cnt = 0, high_cnt = 0;
        for (uint16_t i = 0; i < n; i++) {
            if (rssi[i] <= lo_band) low_cnt++;
            else if (rssi[i] >= hi_band) high_cnt++;
        }

        /* True amplitude keying leaves the mid-band sparse: both clusters must
         * be present (>= 1/5 each) AND together dominate the burst (>= 3/5),
         * which rejects a smooth ramp whose samples spread evenly across the
         * whole range. */
        uint16_t clustered = (uint16_t)(low_cnt + high_cnt);
        bool bimodal = (low_cnt * 5U >= n) &&
                       (high_cnt * 5U >= n) &&
                       (clustered * 5U >= n * 3U);
        if (bimodal) {
            r.mod = RF_MOD_OOK;
            /* Fuller separation into the two clusters ⇒ higher confidence. */
            r.confidence = (clustered * 4U >= n * 3U) ? 3U : 2U;
            return r;
        }
    }

    /* Intermediate / ambiguous spread — do not guess. */
    return r;
}
