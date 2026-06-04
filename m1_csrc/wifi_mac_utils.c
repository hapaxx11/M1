/* See COPYING.txt for license details. */

/**
 * @file   wifi_mac_utils.c
 * @brief  Pure-logic MAC address utilities.
 *
 * See wifi_mac_utils.h for API documentation.
 */

#include "wifi_mac_utils.h"
#include <string.h>
#include <stdio.h>

bool wifi_mac_is_zero(const uint8_t mac[6])
{
    for (uint8_t i = 0; i < 6; i++)
    {
        if (mac[i]) return false;
    }
    return true;
}

void wifi_mac_format(const uint8_t mac[6], char *out, size_t out_len)
{
    if (!out || out_len == 0) return;
    snprintf(out, out_len, "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

bool wifi_mac_match(const uint8_t *a, const uint8_t *b)
{
    return memcmp(a, b, 6) == 0;
}
