/* See COPYING.txt for license details. */

/*
 * rf_fingerprint_24.h
 *
 * ESP32-C6 2.4 GHz fingerprint backend for the RF Rosetta identification
 * pipeline (github.com/joelewis012/RF_Rosetta).
 *
 * The RF Rosetta pipeline is deliberately sensor-agnostic: rf_fingerprint_t is
 * a shared measurement vector and rf_match scores it against a single database.
 * Until now the only *extractor* was the Sub-GHz one
 * (rf_fingerprint_from_subghz_raw()); the ESP32-C6 2.4 GHz domains (BLE
 * advertising, 2.4 GHz WiFi, IEEE 802.15.4 Zigbee/Thread) had DB signatures but
 * no way to turn a coprocessor scan observation into a fingerprint.  This
 * module is that backend.
 *
 * Two responsibilities, both pure logic (no hardware deps, host-testable via
 * tests/test_rf_fingerprint_24.c):
 *
 *   1. Extraction — turn the modest per-detection data the ESP32-C6 can report
 *      (radio channel + RSSI) into an rf_fingerprint_t.  A 2.4 GHz scan cannot
 *      measure pulse timing, so these fingerprints carry only band, modulation
 *      family, centre frequency and RSSI — exactly the "data-poor" case
 *      rf_match_score() is built to handle fairly.
 *
 *   2. Identification — the three 2.4 GHz families share band (2.4 GHz) and
 *      modulation family (all GFSK/O-QPSK, mapped to the FSK family), so
 *      band+modulation scoring cannot tell them apart.  The backend already
 *      knows which radio produced a detection, so rf_match_24() names the
 *      signal deterministically from its sensor domain instead of guessing.
 *
 * The *runtime* backend that drives the ESP32-C6 radio must gate on the
 * relevant capability bit before scanning; rf_sensor_24_required_cap() returns
 * the abstract capability each sensor needs so the (hardware-coupled) m1 layer
 * can translate it to the M1_ESP32_CAP_* bit and call m1_esp32_require_cap().
 * Keeping the mapping here (pure) makes it testable; the actual CAPS gate lives
 * in m1_csrc/m1_rf_fingerprint_24.c.
 *
 * M1 Project — Hapax fork
 */

#ifndef RF_FINGERPRINT_24_H
#define RF_FINGERPRINT_24_H

#include <stdint.h>
#include <stdbool.h>
#include "rf_fingerprint.h"
#include "rf_match.h"

/*============================================================================*/
/* Capability mapping (abstract, translated to M1_ESP32_CAP_* by the m1 layer) */
/*============================================================================*/

/**
 * Abstract ESP32-C6 capability a 2.4 GHz sensor requires.  Intentionally
 * decoupled from m1_esp32_caps.h so this module stays pure and host-testable;
 * m1_csrc/m1_rf_fingerprint_24.c maps these onto the concrete M1_ESP32_CAP_*
 * bits.
 */
typedef enum {
    RF24_CAP_NONE = 0,   /**< Sensor is not a 2.4 GHz ESP32 domain */
    RF24_CAP_BLE_SCAN,   /**< -> M1_ESP32_CAP_BLE_SCAN */
    RF24_CAP_WIFI_SCAN,  /**< -> M1_ESP32_CAP_WIFI_SCAN */
    RF24_CAP_802154,     /**< -> M1_ESP32_CAP_802154 */
} rf24_cap_t;

/** Capability the given 2.4 GHz sensor needs (RF24_CAP_NONE for non-2.4). */
rf24_cap_t rf_sensor_24_required_cap(rf_sensor_t sensor);

/*============================================================================*/
/* Channel -> centre-frequency helpers (Hz)                                   */
/*============================================================================*/

/**
 * 2.4 GHz WiFi channel (1..14) centre frequency.
 * Channels 1..13 are 2412 + (ch-1)*5 MHz; channel 14 is 2484 MHz.
 * @return centre frequency in Hz, or 0 for an out-of-range channel.
 */
uint32_t rf_wifi_channel_to_freq(uint8_t channel);

/**
 * BLE channel index (0..39) centre frequency.
 * Advertising channels 37/38/39 map to 2402/2426/2480 MHz; data channels
 * 0..36 fill the gaps (2404..2478 MHz) skipping the advertising channels.
 * @return centre frequency in Hz, or 0 for an out-of-range channel.
 */
uint32_t rf_ble_channel_to_freq(uint8_t channel);

/**
 * IEEE 802.15.4 2.4 GHz channel (11..26) centre frequency.
 * Channel k is 2405 + (k-11)*5 MHz.
 * @return centre frequency in Hz, or 0 for an out-of-range channel.
 */
uint32_t rf_802154_channel_to_freq(uint8_t channel);

/*============================================================================*/
/* Fingerprint extraction                                                     */
/*============================================================================*/

/**
 * Build a BLE-advertising fingerprint from a coprocessor scan observation.
 *
 * @param adv_channel  BLE channel index the advertisement was seen on (0..39);
 *                     out-of-range leaves freq_hz at 0 but still sets the band.
 * @param rssi_dbm     Measured RSSI (dBm), or 0 if unknown.
 * @param out          Receives the fingerprint (NULL is a no-op).
 */
void rf_fingerprint_from_ble(uint8_t  adv_channel,
                             int16_t  rssi_dbm,
                             rf_fingerprint_t *out);

/**
 * Build a 2.4 GHz WiFi fingerprint from an AP/station scan observation.
 *
 * @param channel   2.4 GHz WiFi channel (1..14).
 * @param rssi_dbm  Measured RSSI (dBm), or 0 if unknown.
 * @param out       Receives the fingerprint (NULL is a no-op).
 */
void rf_fingerprint_from_wifi(uint8_t  channel,
                              int16_t  rssi_dbm,
                              rf_fingerprint_t *out);

/**
 * Build an IEEE 802.15.4 (Zigbee/Thread) fingerprint from a scan observation.
 *
 * @param channel   802.15.4 channel (11..26).
 * @param rssi_dbm  Measured RSSI (dBm), or 0 if unknown.
 * @param out       Receives the fingerprint (NULL is a no-op).
 */
void rf_fingerprint_from_802154(uint8_t  channel,
                                int16_t  rssi_dbm,
                                rf_fingerprint_t *out);

/*============================================================================*/
/* Identification                                                             */
/*============================================================================*/

/**
 * Identify a 2.4 GHz fingerprint from its sensor domain.
 *
 * Unlike rf_match_best(), which scores against the whole database and cannot
 * separate the three same-band/same-modulation 2.4 GHz families, this resolves
 * the signature directly from fp->sensor (rf_protocol_db_find_2400()) and then
 * scores it with rf_match_score() to produce an honest confidence.  A match
 * below RF_MATCH_MIN_CONFIDENCE, a NULL fingerprint, or a non-2.4 GHz sensor
 * yields an empty result (index -1, sig NULL).
 *
 * The returned rf_match_result_t plugs straight into rf_sweep_report_add(),
 * so the 2.4 GHz backend reuses the existing ranked-report UI unchanged.
 */
rf_match_result_t rf_match_24(const rf_fingerprint_t *fp);

#endif /* RF_FINGERPRINT_24_H */
