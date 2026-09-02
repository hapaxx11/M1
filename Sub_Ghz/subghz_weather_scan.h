/* See COPYING.txt for license details. */

/*
 * subghz_weather_scan.h
 *
 * Pure, hardware-independent dwell state machine for the Momentum-style
 * Weather Station RX scan loop.
 *
 * Weather stations transmit in one of two modulations at 433.92 MHz:
 *   - OOK / AM650  (Oregon v1/v2/v3, Acurite, Nexus-TH, Auriol, GT-WT02, ...)
 *   - 2FSK         (LaCrosse TX141TH-Bv2, Bresser 5-in-1/6-in-1, Fine Offset)
 *
 * The M1 SI4463 demodulator can only be configured for one modulation at a
 * time, so — exactly like Flipper/Momentum's Weather Station app — we dwell
 * on each modulation for a fixed period and alternate.  This module owns the
 * timing/state decision so it can be unit-tested on the host without any
 * radio hardware; the firmware calls subghz_weather_scan_tick() each main-loop
 * iteration and re-arms the radio whenever it returns true.
 */

#ifndef SUBGHZ_WEATHER_SCAN_H
#define SUBGHZ_WEATHER_SCAN_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Modulation the scan loop is currently dwelling on. */
typedef enum {
    WX_SCAN_MOD_OOK = 0,   /**< OOK / AM650 demodulator */
    WX_SCAN_MOD_FSK = 1,   /**< 2FSK demodulator        */
} SubGhzWeatherScanMod;

/**
 * Scan state.  All fields are plain data — the struct may live on the caller's
 * stack.  Time is supplied by the caller as a monotonic millisecond tick, so
 * the module has no dependency on FreeRTOS or HAL.
 */
typedef struct {
    uint32_t             dwell_ms;        /**< Time to dwell per modulation.   */
    uint32_t             last_switch_ms;  /**< Tick of the most recent switch. */
    SubGhzWeatherScanMod mod;             /**< Current modulation.             */
    bool                 dual;            /**< true = alternate OOK/FSK;
                                               false = stay on `mod` forever.  */
} SubGhzWeatherScan;

/** Return the opposite modulation. */
SubGhzWeatherScanMod subghz_weather_scan_other(SubGhzWeatherScanMod m);

/**
 * Initialise the scan state.
 *
 * @param s        Scan state to initialise (must be non-NULL).
 * @param dwell_ms Milliseconds to dwell on each modulation before switching.
 *                 A value of 0 is clamped to 1 to avoid a divide-by-zero-like
 *                 busy switch.
 * @param start    Modulation to begin on.
 * @param dual     When true the loop alternates OOK<->FSK; when false it stays
 *                 on @p start (single-modulation mode) and never switches.
 * @param now_ms   Current monotonic millisecond tick (seeds the dwell timer).
 */
void subghz_weather_scan_init(SubGhzWeatherScan *s, uint32_t dwell_ms,
                              SubGhzWeatherScanMod start, bool dual,
                              uint32_t now_ms);

/**
 * Advance the dwell timer.
 *
 * @param s      Scan state.
 * @param now_ms Current monotonic millisecond tick.
 * @return true  if the dwell elapsed and the modulation was switched (the
 *               caller must re-arm the radio for @c s->mod); false otherwise.
 *
 * In single-modulation mode (dual == false) this always returns false.
 * Handles tick wrap-around via unsigned subtraction.
 */
bool subghz_weather_scan_tick(SubGhzWeatherScan *s, uint32_t now_ms);

/** Short human label for the current modulation ("AM" / "FSK"). */
const char *subghz_weather_scan_label(SubGhzWeatherScanMod m);

#ifdef __cplusplus
}
#endif

#endif /* SUBGHZ_WEATHER_SCAN_H */
