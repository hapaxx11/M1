/* See COPYING.txt for license details. */

/**
 * @file   m1_button_bar.h
 * @brief  Standardized 3-column bottom bar renderer, shared across all M1
 *         scenes and modules (Sub-GHz, WiFi, CAN, NFC, Infrared, etc.).
 *
 * Provides consistent Momentum-style button hint rendering.  Fixed layout:
 * left (x=0), center (x=43), right (x=86), y=52-64, 42px-wide buttons with
 * 1px gaps.
 */

#ifndef M1_BUTTON_BAR_H_
#define M1_BUTTON_BAR_H_

#include <stdint.h>

/**
 * @brief  Draw a standardized bottom bar with up to 3 button hints.
 *
 * Each slot can have an icon (8x8 bitmap) and/or a text label.
 * Pass NULL for unused slots.
 *
 * @param left_icon   Left slot icon (8x8 XBM), or NULL
 * @param left_text   Left slot label, or NULL
 * @param center_icon Center slot icon (8x8 XBM), or NULL
 * @param center_text Center slot label, or NULL
 * @param right_icon  Right slot icon (8x8/10x10 XBM), or NULL
 * @param right_text  Right slot label, or NULL
 */
void m1_button_bar_draw(
    const uint8_t *left_icon,   const char *left_text,
    const uint8_t *center_icon, const char *center_text,
    const uint8_t *right_icon,  const char *right_text);

#endif /* M1_BUTTON_BAR_H_ */
