/* See COPYING.txt for license details. */

/*
 * m1_led_color.h — LED color easing (pure logic, hardware-independent)
 *
 * Provides battery-level-based linear interpolation between the user's
 * chosen LED color and a low-battery LED color.
 */

#ifndef M1_LED_COLOR_H_
#define M1_LED_COLOR_H_

#include <stdint.h>

/**
 * @brief  Compute eased RGB color between low-battery and full-battery colors.
 *
 * Uses a normalized Gaussian drop-off:
 *   d      = (100 - level) / 100          (depletion fraction)
 *   weight = 100 * (e^(-d^2) - e^(-1)) / (1 - e^(-1))
 *
 * The color stays close to the full-battery color across most of the
 * battery's range, then drops off toward the low-battery color with
 * Gaussian curvature as charge is depleted.
 *
 * At level == 0   → returns the low-battery color  (weight = 0).
 * At level == 100 → returns the full-battery color  (weight = 100).
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
