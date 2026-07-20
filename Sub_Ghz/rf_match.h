/* See COPYING.txt for license details. */

/*
 * rf_match.h
 *
 * Fingerprint × database scoring engine for the RF Rosetta identification
 * pipeline (github.com/joelewis012/RF_Rosetta).
 *
 * Given an rf_fingerprint_t (measured physical characteristics) this module
 * scores it against every rf_protocol_sig_t in the database and returns the
 * best match with a 0..100 confidence — the "Car Key Fob / KeeLoq Rolling /
 * 87% confidence" line at the heart of RF Rosetta's report.
 *
 * Scoring is a transparent weighted sum of independent feature agreements
 * (band, modulation, timing window, payload size) so it is fully deterministic
 * and unit-testable.  Pure logic, zero hardware deps.
 *
 * M1 Project — Hapax fork
 */

#ifndef RF_MATCH_H
#define RF_MATCH_H

#include <stdint.h>
#include <stdbool.h>
#include "rf_fingerprint.h"
#include "rf_protocol_db.h"

/** Result of matching a fingerprint against the database. */
typedef struct {
    int                      index;      /**< DB index of best match, or -1 */
    uint8_t                  confidence; /**< 0..100 confidence in that match */
    const rf_protocol_sig_t *sig;        /**< Best signature, or NULL */
} rf_match_result_t;

/**
 * Minimum confidence for rf_match_best() to report a match.  Below this the
 * result is reported as "no confident match" (index -1, sig NULL) so the UI
 * shows an honest "unidentified" rather than a spurious low-quality guess.
 */
#ifndef RF_MATCH_MIN_CONFIDENCE
#define RF_MATCH_MIN_CONFIDENCE   40U
#endif

/**
 * Score a single fingerprint against a single signature.
 *
 * @return 0..100.  A hard band or modulation contradiction caps the score low;
 *         a fully-consistent fingerprint (band + mod + timing + size all agree)
 *         scores near 100.
 */
uint8_t rf_match_score(const rf_fingerprint_t  *fp,
                       const rf_protocol_sig_t *sig);

/**
 * Find the best-scoring signature for a fingerprint.
 *
 * @param fp  Fingerprint to identify (NULL yields an empty result).
 * @return    Best match; index -1 / sig NULL / confidence 0 when nothing
 *            scores at or above RF_MATCH_MIN_CONFIDENCE.
 */
rf_match_result_t rf_match_best(const rf_fingerprint_t *fp);

#endif /* RF_MATCH_H */
