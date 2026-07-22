/* See COPYING.txt for license details. */

/*
 * rf_smart_scan.c
 *
 * Pure-logic helpers for Smart ID.  See rf_smart_scan.h.
 * No hardware dependencies.
 *
 * M1 Project — Hapax fork
 */

#include "rf_smart_scan.h"

#include <stdio.h>
#include <stddef.h>

bool rf_smart_scan_freq_valid(uint32_t freq_hz)
{
    return (freq_hz >= RF_SMART_SCAN_FREQ_MIN_HZ) &&
           (freq_hz <= RF_SMART_SCAN_FREQ_MAX_HZ);
}

uint8_t rf_smart_scan_build_plan(const uint32_t *freq_hz, uint8_t count,
                                 rf_scan_point_t *out, uint8_t max_out)
{
    if (freq_hz == NULL || out == NULL || max_out == 0)
        return 0;

    uint8_t n = 0;

    for (uint8_t i = 0; i < count && n < max_out; i++) {
        uint32_t f = freq_hz[i];

        /* Range check */
        if (!rf_smart_scan_freq_valid(f))
            continue;

        /* Dedup: skip if within DEDUP radius of an already-added entry */
        bool dup = false;
        for (uint8_t j = 0; j < n; j++) {
            uint32_t existing = out[j].freq_hz;
            uint32_t diff = (f > existing) ? (f - existing) : (existing - f);
            if (diff < RF_SMART_SCAN_DEDUP_HZ) {
                dup = true;
                break;
            }
        }
        if (dup)
            continue;

        /* Build the label: "NNN.NN" (MHz, 2 decimal places) */
        uint32_t mhz_int  = f / 1000000UL;
        uint32_t mhz_frac = (f % 1000000UL) / 10000UL;   /* 2 digits */

        /* label is stored in the struct as a const char * — we borrow the
         * pattern used by rf_scan_plan.c and produce a literal via snprintf
         * into a small per-entry static buffer.  Because this function is
         * called once at startup (before the blocking loop), the storage is
         * fine as a module-level array. */
        static char s_labels[RF_SMART_SCAN_MAX_FREQS][8];
        if (n < RF_SMART_SCAN_MAX_FREQS) {
            snprintf(s_labels[n], sizeof(s_labels[n]),
                     "%lu.%02lu", (unsigned long)mhz_int, (unsigned long)mhz_frac);
        }

        out[n].freq_hz = f;
        out[n].use_915 = (f >= RF_SCAN_915_BOUNDARY_HZ);
        out[n].label   = (n < RF_SMART_SCAN_MAX_FREQS) ? s_labels[n] : "?.??";
        n++;
    }

    return n;
}
