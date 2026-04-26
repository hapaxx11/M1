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
    BATT_FULL_MV                        = 4200  /* 100% — typical Li-Ion full charge   */
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

#endif /* BATTERY_SOC_ESTIMATE_H_ */
