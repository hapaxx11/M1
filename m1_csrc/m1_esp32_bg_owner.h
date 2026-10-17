/* See COPYING.txt for license details. */

#ifndef M1_ESP32_BG_OWNER_H_
#define M1_ESP32_BG_OWNER_H_

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    M1_ESP32_BG_OWNER_BLE_DIRECT = 1u << 0,
    M1_ESP32_BG_OWNER_WIFI_STA = 1u << 1,
    M1_ESP32_BG_OWNER_HOTSPOT = 1u << 2,
} m1_esp32_bg_owner_t;

static inline uint8_t m1_esp32_bg_owner_update(uint8_t owners,
                                               m1_esp32_bg_owner_t owner,
                                               bool active)
{
    if (active)
        return (uint8_t)(owners | (uint8_t)owner);
    return (uint8_t)(owners & (uint8_t)~owner);
}

static inline bool m1_esp32_bg_owner_any(uint8_t owners)
{
    return owners != 0u;
}

#endif /* M1_ESP32_BG_OWNER_H_ */
