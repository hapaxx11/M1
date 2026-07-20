/* See COPYING.txt for license details. */

/*
 * rf_repetition.c
 *
 * Implementation of the repeated-burst detector.  See rf_repetition.h for
 * the rationale and method.  Pure timing analysis, zero hardware deps.
 *
 * M1 Project — Hapax fork
 */

#include "rf_repetition.h"

#include <stdlib.h>

/* Bound the number of bursts and per-burst analysis so stack usage stays
 * small regardless of how long the capture is. */
#define RF_REPETITION_MAX_BURSTS     64U
#define RF_REPETITION_MAX_SAMPLES    4096U

static inline uint16_t abs_dur(int16_t v)
{
    return (uint16_t)((v < 0) ? -(int32_t)v : (int32_t)v);
}

static int cmp_u16(const void *a, const void *b)
{
    uint16_t x = *(const uint16_t *)a;
    uint16_t y = *(const uint16_t *)b;
    if (x < y) return -1;
    if (x > y) return 1;
    return 0;
}

rf_repetition_t rf_repetition_detect(const int16_t *raw_data,
                                     uint16_t raw_count)
{
    rf_repetition_t r;
    r.count        = 1;
    r.burst_pulses = 0;
    r.burst_total  = 0;
    r.confidence   = 0;

    if (raw_data == NULL || raw_count < RF_REPETITION_MIN_BURST_PULSES)
        return r;

    uint16_t scan = (raw_count < RF_REPETITION_MAX_SAMPLES)
                        ? raw_count : (uint16_t)RF_REPETITION_MAX_SAMPLES;

    /* 1. Median in-band pulse duration → adaptive gap threshold. */
    uint16_t durs[RF_REPETITION_MAX_SAMPLES];
    uint16_t nd = 0;
    for (uint16_t i = 0; i < scan; i++)
    {
        uint16_t d = abs_dur(raw_data[i]);
        if (d >= RF_REPETITION_NOISE_FLOOR)
            durs[nd++] = d;
    }
    if (nd < RF_REPETITION_MIN_BURST_PULSES)
        return r;

    qsort(durs, nd, sizeof(durs[0]), cmp_u16);
    uint32_t median = durs[nd / 2U];

    uint32_t gap_thresh = median * RF_REPETITION_GAP_MULT;
    if (gap_thresh < RF_REPETITION_GAP_FLOOR)
        gap_thresh = RF_REPETITION_GAP_FLOOR;

    /* 2. Split into bursts at inter-packet gaps.  Only a space (negative
     *    sample) may act as a gap; a long mark is still part of the burst. */
    uint16_t burst_lens[RF_REPETITION_MAX_BURSTS];
    uint16_t nb = 0;
    uint16_t cur = 0;   /* pulses in the current burst */

    for (uint16_t i = 0; i < scan; i++)
    {
        int16_t  v = raw_data[i];
        uint16_t d = abs_dur(v);

        if (d < RF_REPETITION_NOISE_FLOOR)
            continue;               /* glitch — ignore, does not split */

        bool is_gap = (v < 0) && ((uint32_t)d >= gap_thresh);
        if (is_gap)
        {
            if (cur >= RF_REPETITION_MIN_BURST_PULSES &&
                nb < RF_REPETITION_MAX_BURSTS)
                burst_lens[nb++] = cur;
            cur = 0;
        }
        else
        {
            cur++;
        }
    }
    /* Trailing burst (no gap terminates the last one). */
    if (cur >= RF_REPETITION_MIN_BURST_PULSES && nb < RF_REPETITION_MAX_BURSTS)
        burst_lens[nb++] = cur;

    r.burst_total = (uint8_t)nb;

    if (nb == 0)
        return r;               /* nothing usable */
    if (nb == 1)
    {
        r.burst_pulses = burst_lens[0];
        r.confidence   = 40;    /* one clean burst, but no repeat evidence */
        return r;
    }

    /* 3. Group bursts by pulse-count similarity; largest group wins. */
    uint8_t  best_group   = 0;
    uint16_t best_pulses  = burst_lens[0];
    for (uint16_t i = 0; i < nb; i++)
    {
        uint16_t ref = burst_lens[i];
        uint16_t tol = (uint16_t)(((uint32_t)ref * RF_REPETITION_BURST_TOL_PCT) / 100U);
        if (tol < 1U)
            tol = 1U;

        uint8_t  group = 0;
        uint32_t sum   = 0;
        for (uint16_t j = 0; j < nb; j++)
        {
            uint16_t d = (burst_lens[j] > ref)
                            ? (uint16_t)(burst_lens[j] - ref)
                            : (uint16_t)(ref - burst_lens[j]);
            if (d <= tol)
            {
                group++;
                sum += burst_lens[j];
            }
        }
        if (group > best_group)
        {
            best_group  = group;
            best_pulses = (uint16_t)(sum / group);
        }
    }

    r.count        = best_group;
    r.burst_pulses = best_pulses;

    /* 4. Confidence: more agreeing bursts + a higher agreeing fraction =
     *    higher confidence.  A single agreeing burst caps low. */
    if (best_group <= 1)
    {
        r.confidence = 40;
    }
    else
    {
        uint32_t frac  = ((uint32_t)best_group * 100U) / nb;   /* 0..100 */
        uint32_t bonus = (best_group >= 3U) ? 30U : 15U;
        uint32_t conf  = (frac / 2U) + bonus;                  /* up to ~80 */
        if (conf > 100U) conf = 100U;
        r.confidence = (uint8_t)conf;
    }

    return r;
}
