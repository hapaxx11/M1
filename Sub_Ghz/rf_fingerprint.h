/* See COPYING.txt for license details. */

/*
 * rf_fingerprint.h
 *
 * Sensor-agnostic RF signal "fingerprint" — the common physical-characteristic
 * descriptor at the heart of the RF Rosetta identification pipeline
 * (github.com/joelewis012/RF_Rosetta).
 *
 * RF Rosetta's value is that it identifies a signal not by decoding its
 * payload but by measuring its *physical characteristics* — frequency,
 * modulation family, bandwidth/deviation, pulse timing, repetition count,
 * RSSI/noise/SNR — and matching that measurement vector against a database of
 * known protocols.  Because identification now spans more than one radio
 * (SI4463 Sub-GHz and the ESP32-C6's 2.4 GHz domains), the measurement vector
 * must be a shared, sensor-agnostic structure so a single database and scoring
 * engine can serve every sensor.  That structure is rf_fingerprint_t.
 *
 * This module is deliberately hardware-independent: the Sub-GHz extractor is a
 * pure function over the same signed timing-sample array the rest of the RAW
 * pipeline uses, and it composes the two existing pure-logic analyzers —
 * subghz_mod_suggest() (issue #616) for the modulation family and
 * rf_repetition_detect() for the repeat count.  It compiles on ARM and host
 * and is unit-tested by tests/test_rf_fingerprint.c.
 *
 * M1 Project — Hapax fork
 */

#ifndef RF_FINGERPRINT_H
#define RF_FINGERPRINT_H

#include <stdint.h>
#include <stdbool.h>

/*============================================================================*/
/* Shared enums                                                               */
/*============================================================================*/

/** Which sensor produced the fingerprint. */
typedef enum {
    RF_SENSOR_SUBGHZ = 0,   /**< SI4463 sub-GHz radio */
    RF_SENSOR_BLE,          /**< ESP32-C6 BLE advertising */
    RF_SENSOR_WIFI,         /**< ESP32-C6 2.4 GHz WiFi */
    RF_SENSOR_802154,       /**< ESP32-C6 IEEE 802.15.4 (Zigbee/Thread) */
} rf_sensor_t;

/** Modulation family (shared vocabulary across sensors and the DB). */
typedef enum {
    RF_MOD_UNKNOWN = 0,
    RF_MOD_OOK,             /**< Amplitude keying (OOK / AM / ASK) */
    RF_MOD_FSK,             /**< Frequency keying (FSK / FM / GFSK) */
} rf_mod_family_t;

/**
 * Frequency-band bit flags.  A fingerprint carries exactly one band bit
 * (the band it was captured on); a DB signature may carry several (the bands
 * a protocol is known to operate on).  Matching is a simple bitmask AND.
 */
typedef enum {
    RF_BAND_300  = (1u << 0),   /**< ~300–348 MHz */
    RF_BAND_315  = (1u << 1),   /**< ~315 MHz */
    RF_BAND_433  = (1u << 2),   /**< ~387–464 MHz (433 ISM) */
    RF_BAND_868  = (1u << 3),   /**< ~779–928 MHz (868 EU ISM) */
    RF_BAND_915  = (1u << 4),   /**< ~902–928 MHz (915 US ISM) */
    RF_BAND_2400 = (1u << 5),   /**< 2.4 GHz ISM */
} rf_band_flag_t;

/*============================================================================*/
/* Fingerprint                                                                */
/*============================================================================*/

/**
 * Measured physical characteristics of one captured signal.
 *
 * All fields are best-effort measurements; a field a given sensor cannot
 * measure is left at its zero/unknown value and simply does not contribute to
 * scoring.  This keeps the structure usable from a bare BLE-advertising scan
 * (which has no pulse timing) as well as a full Sub-GHz RAW capture.
 */
typedef struct {
    rf_sensor_t     sensor;         /**< Producing sensor */
    uint32_t        freq_hz;        /**< Centre frequency (Hz); 0 if unknown */
    uint16_t        band;           /**< rf_band_flag_t — captured band (single bit) */

    rf_mod_family_t mod;            /**< Modulation family */
    uint8_t         mod_confidence; /**< 0..3 (mirrors mod_suggest confidence) */
    uint32_t        bandwidth_hz;   /**< Channel BW / 2·deviation (Hz); 0 if unknown */

    uint16_t        te_us;          /**< Estimated timing element (µs); 0 if unknown */
    uint16_t        pulse_count;    /**< In-band pulses analysed */
    uint16_t        est_bits;       /**< Rough estimated payload bit count */

    uint8_t         repetition;     /**< Repeated-burst count (>=1) */
    uint8_t         rep_confidence; /**< 0..100 repetition confidence */

    int16_t         rssi_dbm;       /**< Signal strength (dBm); 0 if unknown */
    int16_t         noise_dbm;      /**< Noise floor (dBm); 0 if unknown */
    int16_t         snr_db;         /**< rssi − noise (dB); 0 if unknown */
} rf_fingerprint_t;

/*============================================================================*/
/* Helpers                                                                    */
/*============================================================================*/

/** Map a centre frequency (Hz) to its rf_band_flag_t bit (0 if out of band). */
uint16_t rf_band_from_freq(uint32_t freq_hz);

/**
 * Bandwidth/deviation (Hz) for an M1 modulation preset index.
 *
 * The M1 presets AM270 / AM650 / FM238 / FM476 encode their bandwidth (OOK)
 * or peak deviation (FSK) in kHz directly in the label — exactly the
 * OOK-650kHz / FSK-238kHz / FSK-476kHz scan modes RF Rosetta uses.  Returns 0
 * for an out-of-range index.
 */
uint32_t rf_bandwidth_for_preset(uint8_t preset_idx);

/*============================================================================*/
/* Extraction                                                                 */
/*============================================================================*/

/**
 * Build a Sub-GHz fingerprint from a RAW timing capture.
 *
 * Composes subghz_mod_suggest() (modulation family + timing element) and
 * rf_repetition_detect() (repeat count) and fills the frequency/band/
 * bandwidth/RSSI fields from the supplied capture metadata.
 *
 * @param raw_data    Signed mark/space timing samples (µs).
 * @param raw_count   Number of samples.
 * @param freq_hz     Capture centre frequency (Hz).
 * @param preset_idx  M1 modulation preset index (for bandwidth); 0xFF if N/A.
 * @param rssi_dbm    Measured RSSI (dBm), or 0 if unknown.
 * @param noise_dbm   Measured noise floor (dBm), or 0 if unknown.
 * @param out         Receives the fingerprint (must not be NULL).
 */
void rf_fingerprint_from_subghz_raw(const int16_t *raw_data,
                                    uint16_t       raw_count,
                                    uint32_t       freq_hz,
                                    uint8_t        preset_idx,
                                    int16_t        rssi_dbm,
                                    int16_t        noise_dbm,
                                    rf_fingerprint_t *out);

/** Human-readable modulation-family label ("OOK/AM", "FSK/FM", "?"). */
const char *rf_mod_family_str(rf_mod_family_t mod);

/**
 * Does the fingerprint carry a discriminating feature beyond its band?
 *
 * Band alone cannot name a protocol — every device on 433 MHz shares that
 * band — so a band-only fingerprint would match the first same-band signature
 * at full confidence, which is misleading.  A consumer that builds coarse
 * fingerprints (e.g. the live sweep) should only trust rf_match_best() when
 * this returns true; otherwise the signal is merely "active, unidentified".
 *
 * @return true when modulation, timing element, or an estimated bit count is
 *         known (any feature the scorer can weigh against more than band).
 */
bool rf_fingerprint_is_discriminating(const rf_fingerprint_t *fp);

#endif /* RF_FINGERPRINT_H */
