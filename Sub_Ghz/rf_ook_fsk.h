/* See COPYING.txt for license details. */

/*
 * rf_ook_fsk.h
 *
 * Lightweight OOK-vs-FSK modulation classifier for the RF Rosetta Signal
 * Identifier.  During the sweep the SI4463 delegate cannot afford a full RAW
 * timing capture on every candidate, but it *can* cheaply read a short burst
 * of RSSI samples while dwelling on an active frequency.  This module turns
 * that RSSI burst into a modulation-family guess:
 *
 *   - OOK / AM / ASK keys the carrier amplitude on and off, so the RSSI trace
 *     is strongly bimodal with a large peak-to-trough spread.
 *   - FSK / FM / a steady carrier keeps amplitude roughly constant, so the
 *     RSSI trace is tightly clustered with a small spread.
 *
 * It is a heuristic, so it returns a modest 0..3 confidence and reports
 * RF_MOD_UNKNOWN when the evidence is ambiguous.  Pure logic, no hardware
 * deps, unit-tested by tests/test_rf_ook_fsk.c.
 *
 * M1 Project — Hapax fork
 */

#ifndef RF_OOK_FSK_H
#define RF_OOK_FSK_H

#include <stdint.h>
#include "rf_fingerprint.h"   /* rf_mod_family_t */

/** Minimum RSSI samples required before a classification is attempted. */
#ifndef RF_OOK_FSK_MIN_SAMPLES
#define RF_OOK_FSK_MIN_SAMPLES   8U
#endif

/** dB spread at/above which a burst is considered amplitude-keyed (OOK). */
#ifndef RF_OOK_FSK_OOK_SPREAD_DB
#define RF_OOK_FSK_OOK_SPREAD_DB  12
#endif

/** dB spread at/below which a burst is considered a steady carrier (FSK). */
#ifndef RF_OOK_FSK_FSK_SPREAD_DB
#define RF_OOK_FSK_FSK_SPREAD_DB   5
#endif

typedef struct {
    rf_mod_family_t mod;         /**< OOK / FSK / UNKNOWN */
    uint8_t         confidence;  /**< 0 (none) .. 3 (strong) */
    int16_t         spread_db;   /**< max−min of the RSSI burst (dB) */
} rf_ook_fsk_result_t;

/**
 * Classify a burst of RSSI samples (dBm) as OOK, FSK, or unknown.
 *
 * @param rssi  Sample buffer (dBm); NULL or n < RF_OOK_FSK_MIN_SAMPLES yields
 *              an UNKNOWN result with confidence 0.
 * @param n     Number of samples.
 */
rf_ook_fsk_result_t rf_ook_fsk_classify(const int16_t *rssi, uint16_t n);

#endif /* RF_OOK_FSK_H */
