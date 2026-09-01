/* See COPYING.txt for license details. */

/**
 * @file   subghz_rssi_bar.inc
 * @brief  Pure-logic geometry helpers for the Sub-GHz RSSI level bar.
 *
 * Hardware-independent: no HAL, RTOS, or display dependencies.
 * Included (or compiled standalone) by both firmware and host tests.
 *
 * RSSI range: RSSI_FLOOR (-110 dBm) → RSSI_CEIL (-30 dBm)
 * Any value below the floor maps to 0 px; any value above the ceiling
 * maps to bar_max_w px.
 *
 * Usage in firmware (included by m1_subghz_button_bar.c):
 *   #include "subghz_rssi_bar.inc"
 *   ...
 *   uint8_t w = subghz_rssi_fill_w(rssi_dbm, bar_max_w);
 *
 * Usage in host tests (compiled standalone):
 *   #include "subghz_rssi_bar.inc"
 *   TEST_ASSERT_EQUAL_UINT8(0, subghz_rssi_fill_w(-110, 108));
 */

#ifndef SUBGHZ_RSSI_BAR_INC_
#define SUBGHZ_RSSI_BAR_INC_

#include <stdint.h>

/** dBm value mapped to 0 pixels (weakest displayable signal). */
#define SUBGHZ_RSSI_FLOOR  (-110)

/** dBm value mapped to bar_max_w pixels (strongest displayable signal). */
#define SUBGHZ_RSSI_CEIL   (-30)

/**
 * @brief  Map an RSSI reading to a horizontal pixel width.
 *
 * Clamps @p rssi_dbm to [SUBGHZ_RSSI_FLOOR, SUBGHZ_RSSI_CEIL], then
 * linearly scales into [0, @p bar_max_w].
 *
 * @param rssi_dbm   Raw RSSI in dBm (signed 16-bit; values outside the
 *                   floor/ceiling are clamped, not truncated).
 * @param bar_max_w  Maximum pixel width of the filled bar region.
 * @retval uint8_t   Pixel width in [0, bar_max_w].
 */
static inline uint8_t subghz_rssi_fill_w(int16_t rssi_dbm, uint8_t bar_max_w)
{
    if (rssi_dbm <= (int16_t)SUBGHZ_RSSI_FLOOR) return 0;
    if (rssi_dbm >= (int16_t)SUBGHZ_RSSI_CEIL)  return bar_max_w;
    return (uint8_t)((uint32_t)(rssi_dbm - SUBGHZ_RSSI_FLOOR)
                     * bar_max_w
                     / (uint32_t)(SUBGHZ_RSSI_CEIL - SUBGHZ_RSSI_FLOOR));
}

#endif /* SUBGHZ_RSSI_BAR_INC_ */
