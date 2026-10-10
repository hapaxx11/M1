/* See COPYING.txt for license details. */

#ifndef BLE_SIGNAL_FINDER_H_
#define BLE_SIGNAL_FINDER_H_

#include <stdbool.h>
#include <stdint.h>

static inline bool ble_signal_finder_matches(
    const uint8_t target_addr[6], uint8_t target_addr_type,
    const uint8_t candidate_addr[6], uint8_t candidate_addr_type)
{
    if (!target_addr || !candidate_addr || target_addr_type != candidate_addr_type)
        return false;

    for (uint8_t i = 0; i < 6; i++)
    {
        if (target_addr[i] != candidate_addr[i])
            return false;
    }

    return true;
}

static inline uint8_t ble_signal_finder_strength(int8_t rssi)
{
    if (rssi >= -55) return 4;
    if (rssi >= -67) return 3;
    if (rssi >= -80) return 2;
    if (rssi >= -95) return 1;
    return 0;
}

#endif /* BLE_SIGNAL_FINDER_H_ */
