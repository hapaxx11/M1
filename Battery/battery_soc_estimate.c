/* See COPYING.txt for license details. */

/*
 * battery_soc_estimate.c — Voltage-based battery SoC estimation
 *
 * Pure logic, no hardware dependencies.  See battery_soc_estimate.h.
 */

#include "battery_soc_estimate.h"

/*
 * Piecewise-linear approximation of a typical LiPo/Li-Ion discharge curve.
 * Each entry is { voltage_mv, soc_percent }.  The table must be sorted in
 * descending voltage order.
 */
typedef struct {
    uint16_t voltage_mv;
    uint8_t  soc_percent;
} BattSocPoint;

static const BattSocPoint soc_curve[] = {
    { 4200, 100 },
    { 4000,  85 },
    { 3800,  60 },
    { 3600,  35 },
    { 3400,  15 },
    { 3200,   0 },
};

#define SOC_CURVE_LEN  (sizeof(soc_curve) / sizeof(soc_curve[0]))

/*============================================================================*/
/**
 * @brief  Estimate battery state of charge from terminal voltage.
 *         See battery_soc_estimate.h for full documentation.
 */
/*============================================================================*/
uint8_t battery_voltage_to_soc(uint16_t voltage_mv)
{
    /* Above the highest breakpoint → full */
    if (voltage_mv >= soc_curve[0].voltage_mv)
        return 100u;

    /* Below (or at) the lowest breakpoint → empty */
    if (voltage_mv <= soc_curve[SOC_CURVE_LEN - 1].voltage_mv)
        return 0u;

    /* Find the segment [i, i-1] that brackets voltage_mv */
    for (uint8_t i = 1u; i < SOC_CURVE_LEN; i++) {
        if (voltage_mv >= soc_curve[i].voltage_mv) {
            /* Linear interpolation between breakpoints i-1 and i */
            uint16_t v_hi  = soc_curve[i - 1u].voltage_mv;
            uint16_t v_lo  = soc_curve[i].voltage_mv;
            uint8_t  s_hi  = soc_curve[i - 1u].soc_percent;
            uint8_t  s_lo  = soc_curve[i].soc_percent;

            /* soc = s_lo + (voltage - v_lo) * (s_hi - s_lo) / (v_hi - v_lo) */
            uint32_t numerator   = (uint32_t)(voltage_mv - v_lo) * (s_hi - s_lo);
            uint32_t denominator = (uint32_t)(v_hi - v_lo);
            return (uint8_t)(s_lo + (uint8_t)(numerator / denominator));
        }
    }

    return 0u; /* unreachable, but satisfies compiler */
}
