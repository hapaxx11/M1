/* See COPYING.txt for license details. */

/*
 * m1_led_color.c — LED color easing (pure logic, hardware-independent)
 */

#include <stdint.h>
#include <math.h>
#include "m1_led_color.h"

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

    /*
     * Gaussian drop-off easing.
     *
     * d = depletion fraction = (100 - level) / 100  (0 = full, 1 = empty)
     *
     * Raw Gaussian:  g(d) = e^(-d^2)
     *   g(0) = 1.0   (full battery  → full color)
     *   g(1) = e^-1  (empty battery → not quite zero, so normalize)
     *
     * Normalized to span [0, 100]:
     *   weight = 100 * (g(d) - g(1)) / (g(0) - g(1))
     *          = 100 * (e^(-d^2) - e^(-1)) / (1 - e^(-1))
     *
     * Results:
     *   level = 100 (full)  → weight = 100  → full color
     *   level =   0 (empty) → weight =   0  → low-battery color
     *   Color stays near full across most of the range, then drops
     *   off toward the low-battery color with Gaussian curvature.
     */
    float d = (100.0f - (float)level) / 100.0f;
    float gauss_raw   = expf(-d * d);
    float gauss_empty = expf(-1.0f);          /* g(1) ≈ 0.36788 */
    float weight_f    = 100.0f * (gauss_raw - gauss_empty) / (1.0f - gauss_empty);
    uint8_t weight    = (uint8_t)(weight_f + 0.5f);

    if (out_r) *out_r = lerp_channel(low_r, full_r, weight);
    if (out_g) *out_g = lerp_channel(low_g, full_g, weight);
    if (out_b) *out_b = lerp_channel(low_b, full_b, weight);
}
