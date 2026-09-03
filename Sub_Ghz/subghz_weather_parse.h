/* See COPYING.txt for license details. */

/*
 * subghz_weather_parse.h
 *
 * Pure, hardware-independent field extraction for weather-station frames.
 *
 * The M1 protocol decoders recover the raw data word of a weather frame
 * (MSB-first, exactly like Flipper/Momentum's SubGhzBlockDecoder) but, with a
 * handful of exceptions, they do not turn that word into sensor readings — so
 * the Weather Station scene had nothing meaningful to display.  This module
 * performs that second step for every supported protocol, using the same bit
 * layouts and checksum algorithms as the Flipper Weather Station app
 * (Next-Flip/Momentum-Apps: weather_station protocol decoders, GPLv3).
 *
 * It is deliberately free of HAL/RTOS/registry dependencies beyond the
 * protocol id enum, so the whole table can be unit-tested on the host with
 * known-good captures.
 */

#ifndef SUBGHZ_WEATHER_PARSE_H
#define SUBGHZ_WEATHER_PARSE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Sentinel for "sensor does not report humidity" (matches Flipper). */
#define WX_NO_HUMIDITY  0xFFU
/** Sentinel for "protocol has no channel field". */
#define WX_NO_CHANNEL   0x00U
/** Sentinel for "protocol has no TX/button field". */
#define WX_NO_BUTTON    0xFFU

/** Decoded weather-sensor reading. */
typedef struct {
    uint32_t id;            /**< Sensor id / serial.                        */
    uint8_t  channel;       /**< 1..8, or WX_NO_CHANNEL.                    */
    uint8_t  button;        /**< TX/"button" flag, or WX_NO_BUTTON.         */
    uint8_t  battery_low;   /**< 1 = battery low.                           */
    uint8_t  humidity;      /**< 0..100 %, or WX_NO_HUMIDITY.               */
    int16_t  temp_d10;      /**< Temperature in 0.1 degC units.             */
    bool     has_temp;      /**< false when the protocol carries no temp.   */
} SubGhzWeatherFields;

/**
 * Extract sensor fields from a decoded weather frame.
 *
 * @param protocol Protocol registry index (values from the protocol enum in
 *                 m1_sub_ghz_decenc.h).
 * @param data     Raw decoded data word, MSB-first as produced by the M1/
 *                 Flipper block decoders.
 * @param bit_len  Number of decoded bits in @p data.
 * @param out      Result (untouched when the function returns false).
 *
 * @return true when the protocol is supported, @p bit_len matches, and the
 *         frame passes the protocol's checksum / constant-field validation.
 *         Returning false for an unsupported protocol lets the caller fall
 *         back to a raw-data-only display.
 *
 * Checksum validation is part of the contract: the Weather Station scene runs
 * the decoders over sliding pulse offsets, so mis-aligned garbage frames must
 * be rejected here rather than shown to the user.
 */
bool subghz_weather_parse(uint16_t protocol, uint64_t data, uint16_t bit_len,
                          SubGhzWeatherFields *out);

/** True when @p protocol has a field-extraction implementation. */
bool subghz_weather_parse_supported(uint16_t protocol);

/**
 * Convert tenths of degrees Fahrenheit to tenths of degrees Celsius,
 * rounding half away from zero.  Exposed for unit tests.
 */
int16_t subghz_weather_f_to_c_d10(int32_t temp_f_d10);

#ifdef __cplusplus
}
#endif

#endif /* SUBGHZ_WEATHER_PARSE_H */
