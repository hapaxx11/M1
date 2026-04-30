/* See COPYING.txt for license details. */

/*
 * m1_led_color.c — LED color easing (pure logic, hardware-independent)
 */

#include <stdint.h>
#include <math.h>
#include "m1_led_color.h"

/*
 * Precomputed Gaussian constants (avoid expf(-1) on every call).
 *   GAUSS_EMPTY = e^(-1)     ≈ 0.36787944
 *   GAUSS_DENOM = 1 - e^(-1) ≈ 0.63212056
 */
static const float GAUSS_EMPTY = 0.36787944117f;
static const float GAUSS_DENOM = 0.63212055883f;

/*
 * Battery level at and above which the LED color is held constant at the
 * full-charge color (no easing applied).
 */
#define LED_EASE_THRESHOLD 90

/**
 * @brief  Interpolate a single channel by weight (0–100).
 *         result = low + (full - low) * weight / 100
 */
static uint8_t lerp_channel(uint8_t low, uint8_t full, uint8_t weight)
{
    /* Use signed arithmetic to handle full < low correctly */
    int16_t diff = (int16_t)full - (int16_t)low;
    return (uint8_t)((int16_t)low + diff * weight / 100);
}

void m1_led_color_ease(uint8_t level,
                       uint8_t full_r, uint8_t full_g, uint8_t full_b,
                       uint8_t low_r,  uint8_t low_g,  uint8_t low_b,
                       uint8_t *out_r, uint8_t *out_g, uint8_t *out_b)
{
    if (level > 100) level = 100;

    uint8_t weight;

    if (level >= LED_EASE_THRESHOLD)
    {
        /*
         * Above the threshold the LED holds at the full-battery color —
         * no easing, no floating-point work.
         */
        weight = 100;
    }
    else
    {
        /*
         * Gaussian drop-off below LED_EASE_THRESHOLD %.
         *
         * d = depletion fraction within the eased range:
         *     d = (LED_EASE_THRESHOLD - level) / LED_EASE_THRESHOLD
         *     d = 0 at level == threshold (seam — matches weight 100)
         *     d = 1 at level == 0         (weight 0 → low-battery color)
         *
         * Normalized Gaussian:
         *   weight = 100 * (e^(-d^2) - GAUSS_EMPTY) / GAUSS_DENOM
         */
        float d         = (float)(LED_EASE_THRESHOLD - level) / (float)LED_EASE_THRESHOLD;
        float gauss_raw = expf(-d * d);
        float weight_f  = 100.0f * (gauss_raw - GAUSS_EMPTY) / GAUSS_DENOM;
        weight          = (uint8_t)(weight_f + 0.5f);
    }

    if (out_r) *out_r = lerp_channel(low_r, full_r, weight);
    if (out_g) *out_g = lerp_channel(low_g, full_g, weight);
    if (out_b) *out_b = lerp_channel(low_b, full_b, weight);
}
