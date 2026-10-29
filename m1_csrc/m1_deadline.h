/* See COPYING.txt for license details. */

#ifndef M1_DEADLINE_H_
#define M1_DEADLINE_H_

#include <stdint.h>

static inline uint32_t m1_deadline_remaining_ms(uint32_t deadline,
                                                uint32_t now)
{
    int32_t remaining = (int32_t)(deadline - now);
    return remaining > 0 ? (uint32_t)remaining : 0u;
}

static inline int m1_deadline_remaining_seconds(uint32_t deadline,
                                                uint32_t now)
{
    uint32_t remaining = m1_deadline_remaining_ms(deadline, now);
    return remaining > 0u ? (int)((remaining + 999u) / 1000u) : 0;
}

#endif /* M1_DEADLINE_H_ */
