/* See COPYING.txt for license details. */

/*
 * subghz_weather_history.h
 *
 * Pure, hardware-independent sensor list for the Weather Station scene.
 *
 * Flipper/Momentum's Weather Station app keeps one row per physical sensor
 * (keyed on protocol + serial + channel) rather than one row per reception,
 * refreshing the row in place every time the same sensor re-transmits and
 * tracking how long ago it was last heard.  This module owns that bookkeeping
 * so it can be unit-tested on the host without any radio hardware.
 *
 * Time is supplied by the caller as a monotonic millisecond tick, so there is
 * no dependency on FreeRTOS or HAL.
 */

#ifndef SUBGHZ_WEATHER_HISTORY_H
#define SUBGHZ_WEATHER_HISTORY_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum number of distinct sensors tracked simultaneously. */
#define WX_HISTORY_MAX  8

/** One tracked sensor. */
typedef struct {
    uint16_t protocol;      /**< Protocol registry index.                    */
    uint64_t data;          /**< Last raw decoded data word.                 */
    uint16_t bit_len;       /**< Decoded bit count of the last reception.    */
    uint32_t serial;        /**< Sensor serial / ID.                         */
    uint8_t  channel;       /**< Channel (1..8, 0 = protocol has none).      */
    uint8_t  button;        /**< TX / "button" flag from the last frame.     */
    uint8_t  battery_low;   /**< 1 = battery low.                            */
    uint8_t  humidity;      /**< 0-100 %, 0xFF = not supported by sensor.    */
    int16_t  temp_raw;      /**< Temperature in 0.1 degC units.              */
    bool     has_temp;      /**< Temperature field is valid.                 */
    int16_t  rssi;          /**< RSSI of the last reception (dBm).           */
    uint32_t last_seen_ms;  /**< Tick of the last reception.                 */
    uint16_t count;         /**< Total receptions from this sensor.          */
} SubGhzWeatherSensor;

/** Sensor table. May live on the caller's stack or in .bss. */
typedef struct {
    SubGhzWeatherSensor items[WX_HISTORY_MAX];
    uint8_t             count;   /**< Number of valid rows (0..WX_HISTORY_MAX). */
} SubGhzWeatherHistory;

/** Clear the table (may be called with NULL — no-op). */
void subghz_weather_history_reset(SubGhzWeatherHistory *h);

/**
 * Insert or refresh a sensor row.
 *
 * Rows are matched on (protocol, serial, channel).  A matching row is updated
 * in place (data/temperature/humidity/battery/RSSI refreshed, count++,
 * last_seen_ms bumped) and keeps its position, so the list does not reshuffle
 * while the user is scrolling it — this mirrors Momentum's behaviour.
 *
 * When no row matches and the table is full, the least-recently-seen row is
 * evicted.
 *
 * @return index of the inserted/updated row, or -1 if @p h or @p in is NULL.
 */
int subghz_weather_history_add(SubGhzWeatherHistory *h,
                               const SubGhzWeatherSensor *in,
                               uint32_t now_ms);

/** Bounds-checked accessor; returns NULL when @p idx is out of range. */
const SubGhzWeatherSensor *subghz_weather_history_get(
    const SubGhzWeatherHistory *h, uint8_t idx);

/**
 * Age of a row in whole minutes, saturated at 99 (Flipper shows "Nm").
 * Handles monotonic-tick wrap-around via unsigned subtraction.
 */
uint8_t subghz_weather_history_age_min(const SubGhzWeatherSensor *s,
                                       uint32_t now_ms);

/** Convert 0.1 degC units to 0.1 degF units (round-half-away-from-zero). */
int16_t subghz_weather_c_to_f_d10(int16_t temp_c_d10);

#ifdef __cplusplus
}
#endif

#endif /* SUBGHZ_WEATHER_HISTORY_H */
