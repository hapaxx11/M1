/* See COPYING.txt for license details. */

/*
 * subghz_config_cycle.h
 *
 * Pure-logic helper for the Sub-GHz Config scene's Frequency/Modulation
 * cycling.  Extracted from m1_subghz_scene_config.c so the mask-based
 * cycling/wraparound algorithm can be exercised on the host without any
 * HAL, RTOS, or scene-state dependency.
 */

#ifndef SUBGHZ_CONFIG_CYCLE_H
#define SUBGHZ_CONFIG_CYCLE_H

#include <stdint.h>

/**
 * @brief Advance @p idx to the next value allowed by @p mask, circularly.
 *
 * This is the cycling/mask-transition logic shared by the Config scene's
 * Frequency and Modulation controls.  It has no notion of a "filter mode":
 * callers that want unrestricted cycling (e.g. SubGhzConfigFilterNone)
 * simply pass a @p mask with every one of the low @p count bits set (such
 * as UINT64_MAX), which cycles through every index in the given direction
 * exactly like plain unfiltered cycling.
 *
 * @param idx    Current index (expected to be in 0..count-1).
 * @param dir    Direction: >0 advances (wraps past count-1 to 0), <=0
 *               retreats (wraps past 0 to count-1).
 * @param count  Number of valid indices (0..64). Only the low @p count
 *               bits of @p mask are consulted.
 * @param mask   Bitmask where bit i set means index i is allowed.
 * @return The next allowed index, found by walking circularly up to
 *         @p count steps. If @p count is 0 or no bit in the low @p count
 *         bits of @p mask is set, returns @p idx unchanged.
 */
uint8_t subghz_cfg_cycle_next_allowed(uint8_t idx, int8_t dir, uint8_t count,
                                       uint64_t mask);

#endif /* SUBGHZ_CONFIG_CYCLE_H */
