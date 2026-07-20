/* See COPYING.txt for license details. */

/*
 * rf_protocol_db.h
 *
 * Protocol *signature* database + security metadata for the RF Rosetta
 * identification pipeline (github.com/joelewis012/RF_Rosetta).
 *
 * Unlike the M1 protocol *decoder* registry (subghz_protocol_registry.h),
 * which knows how to demodulate and decode a payload, this database describes
 * the *physical fingerprint* of a protocol family — the band(s) it uses, its
 * modulation, its pulse-timing window, its rough payload size — together with
 * human-facing metadata: what devices use it, whether it is fixed / rolling /
 * encrypted, whether it has a known vulnerability, and a plain-English
 * security note.  It is what lets the identifier categorise a signal
 * ("looks like a TPMS sensor, 72% confidence, rolling? no — replayable") even
 * when no decoder matches its payload.
 *
 * The database is a static, const table with no hardware dependencies, so it
 * links on both ARM and host and is unit-tested by tests/test_rf_protocol_db.c.
 *
 * M1 Project — Hapax fork
 */

#ifndef RF_PROTOCOL_DB_H
#define RF_PROTOCOL_DB_H

#include <stdint.h>
#include <stdbool.h>
#include "rf_fingerprint.h"   /* rf_mod_family_t, rf_band_flag_t */

/*============================================================================*/
/* Classification enums                                                       */
/*============================================================================*/

/** Device category (mirrors RF Rosetta's category groupings). */
typedef enum {
    RF_CAT_UNKNOWN = 0,
    RF_CAT_AUTOMOTIVE,
    RF_CAT_HOME,
    RF_CAT_SECURITY,
    RF_CAT_WEATHER,
    RF_CAT_IOT,
    RF_CAT_UTILITY,
    RF_CAT_INDUSTRIAL,
    RF_CAT_MEDICAL,
    RF_CAT_CONSUMER,
    RF_CAT_MISC,
    RF_CAT_COUNT,
} rf_category_t;

/** Security posture — drives the replay-vulnerability note. */
typedef enum {
    RF_SEC_UNKNOWN = 0,
    RF_SEC_FIXED,       /**< Fixed code — replay-vulnerable */
    RF_SEC_ROLLING,     /**< Rolling code — resists naive replay */
    RF_SEC_ENCRYPTED,   /**< Encrypted payload */
} rf_security_t;

/*============================================================================*/
/* Signature entry                                                            */
/*============================================================================*/

/**
 * One protocol-family fingerprint signature.
 *
 * A signal matches this entry when its band, modulation and timing all fall
 * inside the ranges below.  Ranges are inclusive; a zero te_max means "no
 * upper bound", a zero bits_max means "no upper bound" (used for families with
 * highly variable payloads).
 */
typedef struct {
    const char     *name;           /**< Protocol / family name */
    rf_category_t   category;       /**< Device category */
    rf_mod_family_t mod;            /**< Expected modulation family */
    uint16_t        bands;          /**< rf_band_flag_t bitmask of valid bands */
    uint16_t        te_min_us;      /**< Min timing element (µs); 0 = no lower bound */
    uint16_t        te_max_us;      /**< Max timing element (µs); 0 = no upper bound */
    uint16_t        bits_min;       /**< Min payload bits; 0 = no lower bound */
    uint16_t        bits_max;       /**< Max payload bits; 0 = no upper bound */
    rf_security_t   security;       /**< Fixed / rolling / encrypted */
    bool            known_vuln;     /**< Has a widely-known vulnerability */
    const char     *device_note;    /**< Brands / devices that use it */
    const char     *security_note;  /**< Plain-English security note */
} rf_protocol_sig_t;

/*============================================================================*/
/* Accessors                                                                  */
/*============================================================================*/

/** Number of signatures in the database. */
uint16_t rf_protocol_db_count(void);

/** Signature at @p index, or NULL if out of range. */
const rf_protocol_sig_t *rf_protocol_db_get(uint16_t index);

/**
 * Resolve the canonical 2.4 GHz signature index for an ESP32-C6 sensor domain.
 *
 * The three 2.4 GHz families (BLE / WiFi / 802.15.4) share the same band and
 * modulation family, so band+modulation scoring alone cannot tell them apart.
 * The 2.4 GHz backend already knows which radio domain produced a detection
 * (from the fingerprint's rf_sensor_t), so it uses this lookup to name the
 * signal deterministically instead of relying on rf_match_best().
 *
 * @param sensor  RF_SENSOR_BLE, RF_SENSOR_WIFI or RF_SENSOR_802154.
 * @return        DB index of the matching signature, or -1 for a non-2.4 GHz
 *                sensor or if the signature is absent from the table.
 */
int rf_protocol_db_find_2400(rf_sensor_t sensor);

/** Human-readable category label (never NULL). */
const char *rf_category_str(rf_category_t cat);

/** Human-readable security label (never NULL). */
const char *rf_security_str(rf_security_t sec);

#endif /* RF_PROTOCOL_DB_H */
