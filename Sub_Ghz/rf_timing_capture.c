/* See COPYING.txt for license details. */

/*
 * rf_timing_capture.c
 *
 * Implementation of the RSSI-burst → timing-array converter.
 * See rf_timing_capture.h for the full rationale.  Pure logic, no hardware
 * dependencies.
 *
 * M1 Project — Hapax fork
 */

#include "rf_timing_capture.h"

#include <stddef.h>

uint16_t rf_timing_from_rssi_burst(
    const int16_t *rssi_dbm,
    uint16_t       n,
    int16_t        threshold_dbm,
    uint32_t       sample_period_us,
    int16_t       *out,
    uint16_t       out_max)
{
    if (rssi_dbm == NULL || n == 0 || out == NULL || out_max == 0)
        return 0;

    /* Guard against degenerate period so we never multiply by 0. */
    if (sample_period_us == 0)
        sample_period_us = 1;

    uint16_t out_count = 0;

    /* Current polarity: true = mark (above threshold). */
    bool is_mark = (rssi_dbm[0] > threshold_dbm);

    /* Accumulated duration for the current run, in µs. */
    uint32_t run_us = sample_period_us;

    for (uint16_t i = 1; i < n; i++)
    {
        bool sample_mark = (rssi_dbm[i] > threshold_dbm);

        if (sample_mark == is_mark)
        {
            /* Same polarity — extend the current run. */
            run_us += sample_period_us;

            /* Cap to avoid overflow when the run is extremely long. */
            if (run_us > RF_TIMING_CAPTURE_MAX_US)
                run_us = RF_TIMING_CAPTURE_MAX_US;
        }
        else
        {
            /* Polarity change — flush the current run element. */
            if (out_count >= out_max)
                break;

            int16_t val = (int16_t)(run_us <= RF_TIMING_CAPTURE_MAX_US
                                    ? run_us : RF_TIMING_CAPTURE_MAX_US);
            out[out_count++] = is_mark ? val : (int16_t)(-val);

            /* Start the new run. */
            is_mark = sample_mark;
            run_us  = sample_period_us;
        }
    }

    /* Flush the final run. */
    if (out_count < out_max)
    {
        int16_t val = (int16_t)(run_us <= RF_TIMING_CAPTURE_MAX_US
                                ? run_us : RF_TIMING_CAPTURE_MAX_US);
        out[out_count++] = is_mark ? val : (int16_t)(-val);
    }

    return out_count;
}
