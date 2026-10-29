/* See COPYING.txt for license details. */

#ifndef HOME_SCREEN_LAYOUT_H_
#define HOME_SCREEN_LAYOUT_H_

#include <stddef.h>
#include <stdint.h>

#define HOME_SCREEN_STATUS_BASELINE_Y 64u

static inline uint8_t home_screen_status_x(uint16_t display_width,
                                           uint8_t font_width,
                                           size_t text_len)
{
    uint32_t text_width = (uint32_t)text_len * font_width;
    uint8_t x;

    if (text_width >= display_width)
        return 0;

    x = (uint8_t)((display_width - text_width) / 2u);
    if (x >= font_width)
        x -= font_width;
    return x;
}

#endif /* HOME_SCREEN_LAYOUT_H_ */
