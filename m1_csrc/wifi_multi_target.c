/* See COPYING.txt for license details. */

/**
 * @file   wifi_multi_target.c
 * @brief  Pure-logic multi-target AP list builder.
 *
 * Zero dependencies on HAL, RTOS, or display subsystems.
 *
 * M1 Project
 */

#include "wifi_multi_target.h"

uint16_t wifi_multi_target_build(const wifi_ap_t *list, uint16_t count,
                                  wifi_ap_t       *out,  uint16_t out_max)
{
    uint16_t n = 0;

    if (!list || !out || out_max == 0)
        return 0;

    for (uint16_t i = 0; i < count && n < out_max; i++) {
        if (list[i].selected) {
            out[n] = list[i];
            n++;
        }
    }

    return n;
}
