/* See COPYING.txt for license details. */

/*
 * rf_sweep.h
 *
 * Ranked "sweep report" aggregator for the RF Rosetta identification pipeline
 * (github.com/joelewis012/RF_Rosetta).
 *
 * A Signal Identifier sweep visits many frequencies and, whenever it catches a
 * signal, produces an rf_fingerprint_t and scores it with rf_match_best().
 * This module is the pure-logic accumulator that turns that stream of
 * per-detection results into the live report the user reads: a short,
 * deduplicated, confidence-ranked list of the identified signals seen during
 * the sweep ("Car Key Fob 87% · Weather Station 71% · …").
 *
 * It is deliberately hardware-independent so it compiles on ARM and host and
 * is unit-tested by tests/test_rf_sweep.c.  The SI4463 sweep loop and the
 * scene UI feed and render it; the scoring engine and database are untouched.
 *
 * M1 Project — Hapax fork
 */

#ifndef RF_SWEEP_H
#define RF_SWEEP_H

#include <stdint.h>
#include <stdbool.h>
#include "rf_fingerprint.h"
#include "rf_match.h"
#include "rf_protocol_db.h"

/** Maximum number of distinct identified signals kept in a report. */
#ifndef RF_SWEEP_MAX_HITS
#define RF_SWEEP_MAX_HITS   8U
#endif

/** One identified signal accumulated during a sweep. */
typedef struct {
    const rf_protocol_sig_t *sig;        /**< Matched signature, or NULL for decode-confirmed hits */
    const char              *decode_name; /**< Decoder protocol name (non-NULL when sig==NULL,
                                              meaning the protocol decoder identified it directly
                                              at 100% confidence; NULL for fingerprint-scored hits) */
    uint32_t                 freq_hz;    /**< Frequency of the strongest sample (Hz) */
    uint16_t                 band;       /**< rf_band_flag_t the signal was seen on */
    rf_category_t            category;   /**< Device category (from sig, or RF_CAT_UNKNOWN) */
    rf_security_t            security;   /**< Security posture (from sig, or RF_SEC_UNKNOWN) */
    uint8_t                  confidence; /**< Best 0..100 confidence observed */
    int16_t                  rssi_dbm;   /**< Strongest RSSI observed (dBm); 0 unknown */
    uint16_t                 hits;       /**< Times this signal was seen */
} rf_sweep_hit_t;

/**
 * Accumulated sweep report.  Slots are kept sorted best-first (highest
 * confidence, then strongest RSSI).  Zero-initialisation is a valid empty
 * report, but prefer rf_sweep_report_reset() for clarity.
 */
typedef struct {
    rf_sweep_hit_t hits[RF_SWEEP_MAX_HITS]; /**< Ranked identified signals */
    uint8_t        count;                   /**< Slots in use (<= RF_SWEEP_MAX_HITS) */
    uint32_t       total_detections;        /**< Signals fed in (identified or not) */
    uint32_t       identified_detections;   /**< Detections that were identified */
} rf_sweep_report_t;

/** Clear a report back to empty. */
void rf_sweep_report_reset(rf_sweep_report_t *rep);

/**
 * Fold one detection into the report.
 *
 * @param rep    Report to update (NULL is a no-op returning false).
 * @param fp     Fingerprint of the caught signal (NULL is a no-op).
 * @param match  Match result from rf_match_best() (NULL treated as no match).
 *
 * A detection whose match->sig is NULL (nothing scored above the confidence
 * floor) still counts toward total_detections but occupies no slot.  An
 * identified detection is merged into an existing slot when the same signature
 * is seen on the same band (raising its confidence/RSSI and hit count),
 * otherwise it takes a new slot — evicting the current weakest slot only when
 * the report is full and the newcomer is more confident.
 *
 * @return true when the detection was identified and is represented in a slot.
 */
bool rf_sweep_report_add(rf_sweep_report_t      *rep,
                         const rf_fingerprint_t *fp,
                         const rf_match_result_t *match);

/**
 * Best (rank-0) hit in the report, or NULL when nothing has been identified.
 */
const rf_sweep_hit_t *rf_sweep_report_top(const rf_sweep_report_t *rep);

/**
 * Fold a decode-confirmed hit into the report.
 *
 * Use this when the protocol decoder recognised the timing burst directly,
 * yielding a definitive protocol name.  The hit is recorded at 100% confidence
 * and deduplication is performed by (protocol_name pointer, band) identity.
 *
 * @param rep           Report to update (NULL is a no-op returning false).
 * @param freq_hz       Centre frequency the signal was captured on (Hz).
 * @param band          rf_band_flag_t bit for that frequency.
 * @param rssi_dbm      RSSI of the strongest burst sample (dBm); 0 = unknown.
 * @param protocol_name Protocol name string as returned by
 *                      subghz_protocol_get_name() — must be a stable pointer
 *                      (e.g. a string literal in the registry table) since the
 *                      hit stores the pointer, not a copy.
 *
 * @return true when the hit was stored (always, unless rep is NULL).
 */
bool rf_sweep_report_add_decoded(rf_sweep_report_t *rep,
                                 uint32_t           freq_hz,
                                 uint16_t           band,
                                 int16_t            rssi_dbm,
                                 const char        *protocol_name);

#endif /* RF_SWEEP_H */
