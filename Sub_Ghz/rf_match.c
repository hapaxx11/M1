/* See COPYING.txt for license details. */

/*
 * rf_match.c
 *
 * Implementation of the fingerprint × database scoring engine.  See
 * rf_match.h.  Pure logic, zero hardware deps.
 *
 * Scoring model — a normalized weighted sum of *applicable* feature
 * agreements:
 *
 *   - Band     (weight 35): the captured band must intersect the signature's
 *                           band set.  A hard contradiction (both known, no
 *                           intersection) fails the match outright (score 0),
 *                           because a 433 MHz capture simply is not an 868 MHz
 *                           protocol.
 *   - Mod      (weight 25): modulation family agreement.
 *   - Timing   (weight 25): the estimated timing element lies inside the
 *                           signature's [te_min, te_max] window.
 *   - Bits     (weight 15): the estimated payload size lies inside the
 *                           signature's [bits_min, bits_max] range.
 *
 * A feature only participates when *both* the fingerprint and the signature
 * carry the data for it; the final confidence is earned / applicable · 100.
 * This lets a data-poor 2.4 GHz fingerprint (band + modulation only) be scored
 * fairly against a data-poor 2.4 GHz signature, while a rich Sub-GHz RAW
 * capture is judged on all four features.
 *
 * M1 Project — Hapax fork
 */

#include "rf_match.h"

#include <stddef.h>   /* NULL */

#define W_BAND   35U
#define W_MOD    25U
#define W_TE     25U
#define W_BITS   15U

uint8_t rf_match_score(const rf_fingerprint_t  *fp,
                       const rf_protocol_sig_t *sig)
{
    if (fp == NULL || sig == NULL)
        return 0U;

    uint32_t earned   = 0U;
    uint32_t possible = 0U;

    /* ---- Band (also a hard gate) ------------------------------------- */
    if (fp->band != 0U && sig->bands != 0U)
    {
        if ((fp->band & sig->bands) == 0U)
            return 0U;                      /* hard contradiction */
        possible += W_BAND;
        earned   += W_BAND;
    }

    /* ---- Modulation family ------------------------------------------- */
    if (fp->mod != RF_MOD_UNKNOWN && sig->mod != RF_MOD_UNKNOWN)
    {
        possible += W_MOD;
        if (fp->mod == sig->mod)
            earned += W_MOD;
    }

    /* ---- Timing element window --------------------------------------- */
    if (fp->te_us != 0U && (sig->te_min_us != 0U || sig->te_max_us != 0U))
    {
        possible += W_TE;
        bool ge_min = (sig->te_min_us == 0U) || (fp->te_us >= sig->te_min_us);
        bool le_max = (sig->te_max_us == 0U) || (fp->te_us <= sig->te_max_us);
        if (ge_min && le_max)
            earned += W_TE;
    }

    /* ---- Payload size range ------------------------------------------ */
    if (fp->est_bits != 0U && (sig->bits_min != 0U || sig->bits_max != 0U))
    {
        possible += W_BITS;
        bool ge_min = (sig->bits_min == 0U) || (fp->est_bits >= sig->bits_min);
        bool le_max = (sig->bits_max == 0U) || (fp->est_bits <= sig->bits_max);
        if (ge_min && le_max)
            earned += W_BITS;
    }

    if (possible == 0U)
        return 0U;                          /* nothing comparable */

    uint32_t conf = (earned * 100U) / possible;
    if (conf > 100U)
        conf = 100U;
    return (uint8_t)conf;
}

rf_match_result_t rf_match_best(const rf_fingerprint_t *fp)
{
    rf_match_result_t best;
    best.index      = -1;
    best.confidence = 0U;
    best.sig        = NULL;

    if (fp == NULL)
        return best;

    uint16_t n = rf_protocol_db_count();
    for (uint16_t i = 0; i < n; i++)
    {
        const rf_protocol_sig_t *sig = rf_protocol_db_get(i);
        uint8_t s = rf_match_score(fp, sig);
        if (s > best.confidence)
        {
            best.confidence = s;
            best.index      = (int)i;
            best.sig        = sig;
        }
    }

    if (best.confidence < RF_MATCH_MIN_CONFIDENCE)
    {
        best.index = -1;
        best.sig   = NULL;
        /* keep best.confidence so callers can show "best guess X%" if desired */
    }

    return best;
}
