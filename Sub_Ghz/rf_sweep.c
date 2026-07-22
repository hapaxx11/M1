/* See COPYING.txt for license details. */

/*
 * rf_sweep.c
 *
 * Pure-logic sweep-report aggregator for the RF Rosetta Signal Identifier.
 * See rf_sweep.h for the design rationale.  No hardware dependencies.
 *
 * M1 Project — Hapax fork
 */

#include "rf_sweep.h"

#include <string.h>

/*----------------------------------------------------------------------------*/
/* Helpers                                                                    */
/*----------------------------------------------------------------------------*/

/*
 * A larger (closer to zero / positive) RSSI is stronger.  0 encodes "unknown"
 * and is treated as weaker than any real measurement so a known value always
 * wins over an unknown one.
 */
static bool rssi_stronger(int16_t candidate, int16_t current)
{
    if (candidate == 0)
        return false;
    if (current == 0)
        return true;
    return candidate > current;
}

/* Order: higher confidence first, then stronger RSSI. */
static bool hit_ranks_above(const rf_sweep_hit_t *a, const rf_sweep_hit_t *b)
{
    if (a->confidence != b->confidence)
        return a->confidence > b->confidence;
    return rssi_stronger(a->rssi_dbm, b->rssi_dbm);
}

/* Bubble a possibly-improved slot into its sorted position (small N). */
static void resort(rf_sweep_report_t *rep)
{
    for (uint8_t i = 1; i < rep->count; i++) {
        rf_sweep_hit_t key = rep->hits[i];
        int8_t j = (int8_t)i - 1;
        while (j >= 0 && hit_ranks_above(&key, &rep->hits[j])) {
            rep->hits[j + 1] = rep->hits[j];
            j--;
        }
        rep->hits[j + 1] = key;
    }
}

/* Merge a fresh sighting into an existing slot. */
static void merge_into(rf_sweep_hit_t *slot,
                       const rf_fingerprint_t *fp,
                       const rf_match_result_t *match)
{
    if (slot->hits < UINT16_MAX)
        slot->hits++;
    if (match->confidence > slot->confidence)
        slot->confidence = match->confidence;
    if (rssi_stronger(fp->rssi_dbm, slot->rssi_dbm)) {
        slot->rssi_dbm = fp->rssi_dbm;
        slot->freq_hz  = fp->freq_hz;   /* track the strongest sample's freq */
    }
}

/* Fill a slot from scratch. */
static void fill_slot(rf_sweep_hit_t *slot,
                      const rf_fingerprint_t *fp,
                      const rf_match_result_t *match)
{
    slot->sig        = match->sig;
    slot->decode_name = NULL;
    slot->freq_hz    = fp->freq_hz;
    slot->band       = fp->band;
    slot->category   = match->sig->category;
    slot->security   = match->sig->security;
    slot->confidence = match->confidence;
    slot->rssi_dbm   = fp->rssi_dbm;
    slot->hits       = 1;
}

/* Fill a slot from a decode-confirmed detection. */
static void fill_slot_decoded(rf_sweep_hit_t *slot,
                              uint32_t        freq_hz,
                              uint16_t        band,
                              int16_t         rssi_dbm,
                              const char     *protocol_name)
{
    slot->sig         = NULL;
    slot->decode_name = protocol_name;
    slot->freq_hz     = freq_hz;
    slot->band        = band;
    slot->category    = RF_CAT_UNKNOWN;
    slot->security    = RF_SEC_UNKNOWN;
    slot->confidence  = 100;
    slot->rssi_dbm    = rssi_dbm;
    slot->hits        = 1;
}

/*----------------------------------------------------------------------------*/
/* Public API                                                                 */
/*----------------------------------------------------------------------------*/

void rf_sweep_report_reset(rf_sweep_report_t *rep)
{
    if (rep != NULL)
        memset(rep, 0, sizeof(*rep));
}

bool rf_sweep_report_add(rf_sweep_report_t      *rep,
                         const rf_fingerprint_t *fp,
                         const rf_match_result_t *match)
{
    if (rep == NULL || fp == NULL)
        return false;

    rep->total_detections++;

    /* No confident identity — counted, but occupies no slot. */
    if (match == NULL || match->sig == NULL)
        return false;

    rep->identified_detections++;

    /* Same protocol on the same band → merge into the existing slot. */
    for (uint8_t i = 0; i < rep->count; i++) {
        if (rep->hits[i].sig == match->sig &&
            rep->hits[i].band == fp->band) {
            merge_into(&rep->hits[i], fp, match);
            resort(rep);
            return true;
        }
    }

    /* New identity: take a free slot if available. */
    if (rep->count < RF_SWEEP_MAX_HITS) {
        fill_slot(&rep->hits[rep->count], fp, match);
        rep->count++;
        resort(rep);
        return true;
    }

    /* Full: evict the weakest slot only if the newcomer outranks it.  Slots
     * are sorted best-first, so the weakest is the last one. */
    {
        rf_sweep_hit_t candidate;
        fill_slot(&candidate, fp, match);
        rf_sweep_hit_t *weakest = &rep->hits[RF_SWEEP_MAX_HITS - 1];
        if (hit_ranks_above(&candidate, weakest)) {
            *weakest = candidate;
            resort(rep);
            return true;
        }
    }

    return false;
}

const rf_sweep_hit_t *rf_sweep_report_top(const rf_sweep_report_t *rep)
{
    if (rep == NULL || rep->count == 0)
        return NULL;
    return &rep->hits[0];
}

bool rf_sweep_report_add_decoded(rf_sweep_report_t *rep,
                                 uint32_t           freq_hz,
                                 uint16_t           band,
                                 int16_t            rssi_dbm,
                                 const char        *protocol_name)
{
    if (rep == NULL || protocol_name == NULL)
        return false;

    rep->total_detections++;
    rep->identified_detections++;

    /* Same protocol on the same band → merge (bump hits, update strongest RSSI). */
    for (uint8_t i = 0; i < rep->count; i++) {
        if (rep->hits[i].decode_name == protocol_name &&
            rep->hits[i].band == band) {
            rf_sweep_hit_t *slot = &rep->hits[i];
            if (slot->hits < UINT16_MAX)
                slot->hits++;
            if (rssi_stronger(rssi_dbm, slot->rssi_dbm)) {
                slot->rssi_dbm = rssi_dbm;
                slot->freq_hz  = freq_hz;
            }
            /* confidence stays 100 — no resort needed */
            return true;
        }
    }

    /* New decode-confirmed identity: take a free slot if available. */
    if (rep->count < RF_SWEEP_MAX_HITS) {
        fill_slot_decoded(&rep->hits[rep->count], freq_hz, band, rssi_dbm, protocol_name);
        rep->count++;
        resort(rep);
        return true;
    }

    /* Full: evict the weakest slot only if the newcomer outranks it.
     * A decode-confirmed hit has confidence 100, so it always outranks any
     * fingerprint slot that scored below 100. */
    {
        rf_sweep_hit_t candidate;
        fill_slot_decoded(&candidate, freq_hz, band, rssi_dbm, protocol_name);
        rf_sweep_hit_t *weakest = &rep->hits[RF_SWEEP_MAX_HITS - 1];
        if (hit_ranks_above(&candidate, weakest)) {
            *weakest = candidate;
            resort(rep);
            return true;
        }
    }

    return false;
}
