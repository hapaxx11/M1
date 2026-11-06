/* See COPYING.txt for license details. */
#ifndef M1_ESP32_HAL_LIFECYCLE_H_
#define M1_ESP32_HAL_LIFECYCLE_H_

#include <stdbool.h>
#include <stdint.h>
#include "esp32_feature_map.h"

typedef bool (*m1_esp32_lock_wait_fn)(uint32_t timeout_ticks);
typedef void (*m1_esp32_wdt_kick_fn)(void);

static inline bool m1_esp32_lifecycle_lock_with_wdt(
    m1_esp32_lock_wait_fn lock_wait,
    m1_esp32_wdt_kick_fn wdt_kick,
    uint32_t timeout_ticks)
{
    if (!lock_wait || !wdt_kick) return false;
    wdt_kick();
    bool locked = lock_wait(timeout_ticks);
    wdt_kick();
    return locked;
}

static inline bool m1_esp32_lifecycle_at_task_required(esp32_transport_t transport)
{
    return transport == ESP32_TRANSPORT_AT;
}

static inline uint8_t m1_esp32_lifecycle_foreground_update(uint8_t users,
                                                            bool acquire)
{
    if (acquire) return users < UINT8_MAX ? (uint8_t)(users + 1u) : users;
    return users > 0u ? (uint8_t)(users - 1u) : 0u;
}

static inline bool m1_esp32_lifecycle_relay_allowed(uint8_t foreground_users,
                                                     bool firmware_update)
{
    return foreground_users == 0u && !firmware_update;
}

#endif /* M1_ESP32_HAL_LIFECYCLE_H_ */
