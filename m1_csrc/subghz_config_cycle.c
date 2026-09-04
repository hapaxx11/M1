/* See COPYING.txt for license details. */

#include "subghz_config_cycle.h"

uint8_t subghz_cfg_cycle_next_allowed(uint8_t idx, int8_t dir, uint8_t count,
                                       uint64_t mask)
{
    if (count == 0)
        return idx;

    for (uint8_t step = 0; step < count; step++)
    {
        if (dir > 0)
        {
            idx = (uint8_t)((idx + 1) % count);
        }
        else
        {
            idx = (idx > 0) ? (uint8_t)(idx - 1) : (uint8_t)(count - 1);
        }
        if (mask & (UINT64_C(1) << idx))
            return idx;
    }
    return idx;  /* no allowed value found (should not happen) */
}
