/* See COPYING.txt for license details. */

#ifndef M1_DISPLAY_UTIL_H_
#define M1_DISPLAY_UTIL_H_

#include "u8g2.h"

static inline u8g2_uint_t m1_battery_pct_text_width(u8g2_t *u8g2, const char *pct_str)
{
    return u8g2_GetStrWidth(u8g2, pct_str);
}

#endif /* M1_DISPLAY_UTIL_H_ */
