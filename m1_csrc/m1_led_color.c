/* See COPYING.txt for license details. */

/*
 * m1_led_color.c — LED color easing (pure logic, hardware-independent)
 */

#include <stdint.h>
#include "m1_led_color.h"

/**
 * @brief  Linear interpolation of a single channel.
 *         result = low + (full - low) * level / 100
 */
static uint8_t lerp_channel(uint8_t low, uint8_t full, uint8_t level)
{
    /* Use signed arithmetic to handle full < low correctly */
    int16_t diff = (int16_t)full - (int16_t)low;
    return (uint8_t)((int16_t)low + diff * level / 100);
}

void m1_led_color_ease(uint8_t level,
                       uint8_t full_r, uint8_t full_g, uint8_t full_b,
                       uint8_t low_r,  uint8_t low_g,  uint8_t low_b,
                       uint8_t *out_r, uint8_t *out_g, uint8_t *out_b)
{
    if (level > 100) level = 100;

    if (out_r) *out_r = lerp_channel(low_r, full_r, level);
    if (out_g) *out_g = lerp_channel(low_g, full_g, level);
    if (out_b) *out_b = lerp_channel(low_b, full_b, level);
}
