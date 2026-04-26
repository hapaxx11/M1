/* See COPYING.txt for license details. */

/*
 * battery_soc_estimate.h — Voltage-based battery SoC estimation (pure logic,
 *                          hardware-independent)
 *
 * Provides a piecewise-linear approximation of the Li-Ion/LiPo discharge
 * curve so that the system can estimate the state of charge from the raw
 * battery voltage when the fuel gauge (BQ27421) reading is unreliable.
 *
 * Typical use: apply as a floor when the fuel gauge reports 0% but the
 * measured voltage indicates meaningful charge remains.
 */

#ifndef BATTERY_SOC_ESTIMATE_H_
#define BATTERY_SOC_ESTIMATE_H_

#include <stdint.h>

/*
 * Battery SoC estimation constants.
 *
 * BATT_SOC_VOLTAGE_FALLBACK_THRESHOLD: fuel-gauge readings at or below this
 *   percent are considered suspect; the voltage-based estimate is applied as
 *   a floor.
 *
 * BATT_TERMINATE_MV / BATT_FULL_MV: lower and upper voltage bounds of the
 *   piecewise-linear estimation curve (millivolts).  These are the curve
 *   endpoints — battery_voltage_to_soc() returns 0% at or below TERMINATE
 *   and 100% at or above FULL.
 */
enum {
    BATT_SOC_VOLTAGE_FALLBACK_THRESHOLD = 5,   /* percent */
    BATT_TERMINATE_MV                   = 3200, /* 0%   — estimation curve lower bound */
    BATT_FULL_MV                        = 4200, /* 100% — typical Li-Ion full charge   */

    /* Maximum SoC change per display update period (1 second).
     * The BQ27421 can jump several percent in one reading when
     * transitioning between charge/discharge; this cap prevents those
     * raw gauge recalibration steps from appearing as sudden jumps on
     * screen.  Real decisions (battery-level checks, firmware update
     * guard) use the unsmoothed battery_level field. */
    BATT_DISPLAY_MAX_DELTA_PCT          = 1     /* percent per update */
};

/**
 * @brief  Estimate battery state of charge from terminal voltage.
 *
 * Uses a piecewise-linear approximation of a typical LiPo/Li-Ion discharge
 * curve.  The result is intentionally conservative — it underestimates
 * slightly compared to a calibrated gauge so it is safe to use as a floor
 * when the primary SoC reading is unreliable.
 *
 * Breakpoints (voltage → SoC):
 *   ≥ 4200 mV → 100 %
 *   4000 mV  →  85 %
 *   3800 mV  →  60 %
 *   3600 mV  →  35 %
 *   3400 mV  →  15 %
 *   ≤ 3200 mV →   0 %
 *
 * @param  voltage_mv  Measured battery voltage in millivolts
 * @return             Estimated SoC in percent (0–100, clamped)
 */
uint8_t battery_voltage_to_soc(uint16_t voltage_mv);

/**
 * @brief  Rate-limit the displayed battery SoC toward a new raw reading.
 *
 * Prevents sudden gauge recalibration jumps from appearing on screen.
 * The displayed value moves toward raw_pct by at most
 * BATT_DISPLAY_MAX_DELTA_PCT percent in a single call.  Call once per
 * battery_status_update() cycle (nominally every 1 second).
 *
 * @param  displayed_pct  Current displayed SoC (previous return value)
 * @param  raw_pct        Raw SoC from the fuel gauge (0–100)
 * @return                New displayed SoC (0–100)
 */
uint8_t battery_soc_smooth(uint8_t displayed_pct, uint8_t raw_pct);

#endif /* BATTERY_SOC_ESTIMATE_H_ */
