/* See COPYING.txt for license details. */

/*
 * subghz_mod_suggest.c
 *
 * Implementation of the waveform-based modulation suggestion analyzer.
 * See subghz_mod_suggest.h for the rationale and the important caveat that
 * this is a heuristic hint, not an authoritative classification.
 *
 * Method (pure timing analysis):
 *   1. Extract in-band pulse durations (|d| between the noise floor and the
 *      inter-packet gap ceiling) into a bounded local buffer.
 *   2. Estimate the timing element `te` robustly as the mean of the cluster
 *      around the 5th-percentile duration (skips a few glitch outliers).
 *   3. Compute the fraction of in-band pulses that fall within ±20% of an
 *      integer multiple n·te (n in 1..MAX_MULT).  Clean OOK PWM keying is
 *      almost fully quantized; FSK demodulated with an OOK detector is
 *      dominated by non-quantized jitter, so its quantization fraction is
 *      close to the ~40% a uniform-random distribution would score.
 *   4. Map the quantization fraction to an OOK/FSK/UNKNOWN suggestion with a
 *      confidence level that also reflects how many pulses were available.
 *
 * M1 Project — Hapax fork
 */

#include "subghz_mod_suggest.h"

#include <stdlib.h>

/* Largest integer multiple of te that a single pulse may represent and still
 * count as "quantized".  Only bounds the multiple; it does not change the
 * fraction of a uniform-random distribution that scores as quantized. */
#define SUBGHZ_MOD_SUGGEST_MAX_MULT     40U

/* Bound the working buffer so stack usage stays small regardless of how many
 * samples were captured.  The first N in-band pulses are representative. */
#define SUBGHZ_MOD_SUGGEST_MAX_SAMPLES  512U

/* Quantization tolerance: a pulse counts as n·te when within this band. */
#define SUBGHZ_MOD_SUGGEST_QUANT_TOL_PCT   20U
#define SUBGHZ_MOD_SUGGEST_QUANT_TOL_MIN   10U   /* µs floor for the tolerance */

static int cmp_u16(const void *a, const void *b)
{
    uint16_t x = *(const uint16_t *)a;
    uint16_t y = *(const uint16_t *)b;
    if (x < y) return -1;
    if (x > y) return 1;
    return 0;
}

static inline uint16_t abs_dur(int16_t v)
{
    return (uint16_t)((v < 0) ? -(int32_t)v : (int32_t)v);
}

SubGhzModSuggestResult subghz_mod_suggest(const int16_t *raw_data,
                                          uint16_t raw_count)
{
    SubGhzModSuggestResult r;
    r.type        = SUBGHZ_MOD_SUGGEST_UNKNOWN;
    r.confidence  = SUBGHZ_MOD_SUGGEST_CONF_NONE;
    r.te          = 0;
    r.pulse_count = 0;
    r.quant_pct   = 0;

    if (raw_data == NULL || raw_count == 0)
        return r;

    /* 1. Collect in-band pulse durations. */
    uint16_t buf[SUBGHZ_MOD_SUGGEST_MAX_SAMPLES];
    uint16_t n = 0;
    for (uint16_t i = 0; i < raw_count && n < SUBGHZ_MOD_SUGGEST_MAX_SAMPLES; i++)
    {
        uint16_t d = abs_dur(raw_data[i]);
        if (d < SUBGHZ_MOD_SUGGEST_NOISE_FLOOR)
            continue;               /* glitch / noise edge */
        if (d > SUBGHZ_MOD_SUGGEST_GAP_CEIL)
            continue;               /* inter-packet gap, not a symbol */
        buf[n++] = d;
    }

    r.pulse_count = n;
    if (n < SUBGHZ_MOD_SUGGEST_MIN_PULSES)
        return r;                   /* not enough evidence */

    /* 2. Robust timing-element estimate. */
    qsort(buf, n, sizeof(buf[0]), cmp_u16);

    uint32_t te0 = buf[n / 20U];    /* ~5th percentile short pulse */
    if (te0 < SUBGHZ_MOD_SUGGEST_NOISE_FLOOR)
        te0 = SUBGHZ_MOD_SUGGEST_NOISE_FLOOR;

    uint32_t cluster_tol = (te0 * 30U) / 100U;
    if (cluster_tol < 30U)
        cluster_tol = 30U;

    uint32_t sum = 0, cnt = 0;
    for (uint16_t i = 0; i < n; i++)
    {
        uint32_t d   = buf[i];
        uint32_t dif = (d > te0) ? (d - te0) : (te0 - d);
        if (dif <= cluster_tol)
        {
            sum += d;
            cnt++;
        }
    }
    uint32_t te = cnt ? (sum / cnt) : te0;
    if (te < SUBGHZ_MOD_SUGGEST_NOISE_FLOOR)
        te = SUBGHZ_MOD_SUGGEST_NOISE_FLOOR;
    r.te = (uint16_t)te;

    /* 3. Quantization fraction against n·te. */
    uint32_t quant_tol = (te * SUBGHZ_MOD_SUGGEST_QUANT_TOL_PCT) / 100U;
    if (quant_tol < SUBGHZ_MOD_SUGGEST_QUANT_TOL_MIN)
        quant_tol = SUBGHZ_MOD_SUGGEST_QUANT_TOL_MIN;

    uint32_t quantized = 0;
    for (uint16_t i = 0; i < n; i++)
    {
        uint32_t d      = buf[i];
        uint32_t mult   = (d + te / 2U) / te;   /* nearest integer multiple */
        if (mult < 1U)
            mult = 1U;
        if (mult > SUBGHZ_MOD_SUGGEST_MAX_MULT)
            continue;                           /* implausibly long → not a symbol */
        uint32_t ideal  = mult * te;
        uint32_t dif    = (d > ideal) ? (d - ideal) : (ideal - d);
        if (dif <= quant_tol)
            quantized++;
    }
    uint8_t quant_pct = (uint8_t)((quantized * 100U) / n);
    r.quant_pct = quant_pct;

    /* 4. Map to a suggestion.  A larger pulse population upgrades confidence. */
    bool many = (n >= 48U);

    if (quant_pct >= 85U)
    {
        r.type       = SUBGHZ_MOD_SUGGEST_OOK;
        r.confidence = many ? SUBGHZ_MOD_SUGGEST_CONF_HIGH
                            : SUBGHZ_MOD_SUGGEST_CONF_MEDIUM;
    }
    else if (quant_pct >= 70U)
    {
        r.type       = SUBGHZ_MOD_SUGGEST_OOK;
        r.confidence = SUBGHZ_MOD_SUGGEST_CONF_MEDIUM;
    }
    else if (quant_pct >= 65U)
    {
        r.type       = SUBGHZ_MOD_SUGGEST_OOK;
        r.confidence = SUBGHZ_MOD_SUGGEST_CONF_LOW;
    }
    else if (quant_pct <= 35U)
    {
        r.type       = SUBGHZ_MOD_SUGGEST_FSK;
        r.confidence = many ? SUBGHZ_MOD_SUGGEST_CONF_MEDIUM
                            : SUBGHZ_MOD_SUGGEST_CONF_LOW;
    }
    else if (quant_pct <= 50U)
    {
        r.type       = SUBGHZ_MOD_SUGGEST_FSK;
        r.confidence = SUBGHZ_MOD_SUGGEST_CONF_LOW;
    }
    else
    {
        /* 51..64 %: genuinely ambiguous — neither clean keying nor pure jitter. */
        r.type       = SUBGHZ_MOD_SUGGEST_UNKNOWN;
        r.confidence = SUBGHZ_MOD_SUGGEST_CONF_LOW;
    }

    return r;
}

const char *subghz_mod_suggest_type_str(SubGhzModSuggestType type)
{
    switch (type)
    {
        case SUBGHZ_MOD_SUGGEST_OOK: return "OOK/AM";
        case SUBGHZ_MOD_SUGGEST_FSK: return "FSK/FM";
        default:                     return "?";
    }
}

const char *subghz_mod_suggest_confidence_str(SubGhzModSuggestConfidence conf)
{
    switch (conf)
    {
        case SUBGHZ_MOD_SUGGEST_CONF_LOW:    return "low";
        case SUBGHZ_MOD_SUGGEST_CONF_MEDIUM: return "med";
        case SUBGHZ_MOD_SUGGEST_CONF_HIGH:   return "high";
        default:                             return "-";
    }
}
