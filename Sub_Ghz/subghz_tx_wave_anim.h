/* See COPYING.txt for license details. */

/**
 * @file   subghz_tx_wave_anim.h
 * @brief  Scrolling sine-wave TX animation — pure logic, host-testable.
 *
 * Momentum's Read RAW view animates its RSSI plot area with a continuously
 * scrolling sine wave while a signal is transmitting (see
 * `subghz_read_raw_draw_sin()` / `subghz_read_raw_update_sin()` upstream).
 * This module ports that idea as a small, hardware-free helper so the
 * generic key-file Transmitter scene (`m1_subghz_scene_transmitter.c`) can
 * show the same kind of "sending" animation instead of a static "..." dot
 * cycle, without pulling any u8g2 drawing calls into a unit-testable module.
 *
 * Usage from the scene:
 *   - Call `subghz_tx_wave_anim_step()` once per display tick while TX is
 *     in flight to advance the phase (wraps automatically).
 *   - Call `subghz_tx_wave_anim_sample()` for each x column in the plot
 *     area to get a signed y-offset (in the same amplitude units the caller
 *     chooses to scale) to draw at that column.
 */

#ifndef SUBGHZ_TX_WAVE_ANIM_H_
#define SUBGHZ_TX_WAVE_ANIM_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Number of distinct phase steps before the scroll offset wraps around.
 *  Matches the lookup table period so `subghz_tx_wave_anim_step()` and
 *  `subghz_tx_wave_anim_sample()` stay in sync. */
#define SUBGHZ_TX_WAVE_ANIM_PHASE_PERIOD 64U

/**
 * @brief  Advance the scrolling wave's phase by one tick.
 *
 * @param  phase  In/out phase counter (any prior value is valid; wrapped to
 *                the [0, SUBGHZ_TX_WAVE_ANIM_PHASE_PERIOD) range internally
 *                before being incremented, so callers can pass a raw
 *                free-running tick counter without pre-masking it).
 */
void subghz_tx_wave_anim_step(uint8_t *phase);

/**
 * @brief  Sample the scrolling sine wave at a given column and phase.
 *
 * @param  column     0-based horizontal position within the plot area.
 * @param  phase      Current animation phase (see subghz_tx_wave_anim_step()).
 * @param  amplitude  Maximum |y| deviation returned (peak amplitude, 0..127;
 *                    values above 127 are clamped). Values are proportionally
 *                    scaled from the internal +/-127 lookup table range.
 * @retval  Signed y-offset in the range [-amplitude, +amplitude].
 */
int8_t subghz_tx_wave_anim_sample(uint16_t column, uint8_t phase, uint8_t amplitude);

#ifdef __cplusplus
}
#endif

#endif /* SUBGHZ_TX_WAVE_ANIM_H_ */
