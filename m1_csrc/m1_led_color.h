/* See COPYING.txt for license details. */

/*
 * m1_led_color.h — LED color easing (pure logic, hardware-independent)
 *
 * Provides battery-level-based Gaussian drop-off easing between the user's
 * chosen LED color and a low-battery LED color.
 */

#ifndef M1_LED_COLOR_H_
#define M1_LED_COLOR_H_

#include <stdint.h>

/**
 * @brief  Compute eased RGB color between low-battery and full-battery colors.
 *
 * Uses a normalized Gaussian drop-off active only below 90% charge:
 *
 *   level >= 90 → weight = 100 (full color; no easing)
 *   level <  90 → d = (90 - level) / 90
 *                 weight = 100 * (e^(-d^2) - e^(-1)) / (1 - e^(-1))
 *
 * The color is held at the full-battery color while charge is above 90%,
 * then drops off toward the low-battery color with Gaussian curvature
 * as charge falls from 90% to 0%.
 *
 * At level == 0   → returns the low-battery color  (weight = 0).
 * At level >= 90  → returns the full-battery color  (weight = 100).
 *
 * @param  level       Battery percentage (0–100, clamped internally)
 * @param  full_r/g/b  Full-charge (user-selected) LED color
 * @param  low_r/g/b   Low-battery LED color
 * @param  out_r/g/b   Output eased color (NULL-safe: skipped if NULL)
 */
void m1_led_color_ease(uint8_t level,
                       uint8_t full_r, uint8_t full_g, uint8_t full_b,
                       uint8_t low_r,  uint8_t low_g,  uint8_t low_b,
                       uint8_t *out_r, uint8_t *out_g, uint8_t *out_b);

#endif /* M1_LED_COLOR_H_ */
