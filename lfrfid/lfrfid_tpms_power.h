/* See COPYING.txt for license details. */

#ifndef LFRFID_TPMS_POWER_H
#define LFRFID_TPMS_POWER_H

#include <stdbool.h>
#include <stdint.h>

typedef uint8_t lfrfid_tpms_power_state_t;

#define LFRFID_TPMS_POWER_ACTIVE 0x01U
#define LFRFID_TPMS_POWER_SUPPLY_WAS_ENABLED 0x02U

static inline lfrfid_tpms_power_state_t lfrfid_tpms_power_started(
    bool supply_was_enabled)
{
    return LFRFID_TPMS_POWER_ACTIVE |
        (supply_was_enabled ? LFRFID_TPMS_POWER_SUPPLY_WAS_ENABLED : 0U);
}

static inline bool lfrfid_tpms_power_is_active(lfrfid_tpms_power_state_t state)
{
    return (state & LFRFID_TPMS_POWER_ACTIVE) != 0U;
}

static inline bool lfrfid_tpms_power_was_enabled(
    lfrfid_tpms_power_state_t state)
{
    return (state & LFRFID_TPMS_POWER_SUPPLY_WAS_ENABLED) != 0U;
}

#endif
