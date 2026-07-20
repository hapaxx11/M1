/* See COPYING.txt for license details. */

/*
 * m1_rf_fingerprint_24.h
 *
 * CAPS-gated runtime entry points for the ESP32-C6 2.4 GHz fingerprint backend.
 *
 * The pure-logic backend lives in Sub_Ghz/rf_fingerprint_24.c (extraction +
 * identification, host-tested).  This thin m1 layer is the only place that
 * couples that backend to the hardware capability system: it translates the
 * abstract rf24_cap_t a sensor needs (rf_sensor_24_required_cap()) into the
 * concrete M1_ESP32_CAP_* bit and gates on it via m1_esp32_has_cap() /
 * m1_esp32_require_cap().
 *
 * Rule (per m1_esp32_caps.h): any 2.4 GHz scan that feeds this backend MUST
 * pass m1_rf24_require_sensor() before touching the ESP32-C6 radio, so the
 * feature fails closed on firmware that lacks the capability.
 *
 * M1 Project — Hapax fork
 */

#ifndef M1_RF_FINGERPRINT_24_H_
#define M1_RF_FINGERPRINT_24_H_

#include <stdint.h>
#include <stdbool.h>
#include "rf_fingerprint.h"        /* rf_sensor_t */

/**
 * The M1_ESP32_CAP_* bit a 2.4 GHz sensor requires, or 0 for a non-2.4 GHz
 * sensor.  Wraps rf_sensor_24_required_cap() with the concrete capability
 * bits.
 */
uint64_t m1_rf24_sensor_cap(rf_sensor_t sensor);

/**
 * True when the connected ESP32-C6 firmware advertises the capability the
 * given 2.4 GHz sensor needs.  Non-2.4 GHz sensors always return false.
 */
bool m1_rf24_sensor_available(rf_sensor_t sensor);

/**
 * CAPS gate for a 2.4 GHz fingerprint scan.  Returns true when the sensor's
 * capability is present; otherwise shows the standard "feature not supported"
 * message (via m1_esp32_require_cap()) and returns false.
 *
 * @param sensor        RF_SENSOR_BLE / RF_SENSOR_WIFI / RF_SENSOR_802154.
 * @param feature_name  Human-readable feature label for the message.
 */
bool m1_rf24_require_sensor(rf_sensor_t sensor, const char *feature_name);

#endif /* M1_RF_FINGERPRINT_24_H_ */
