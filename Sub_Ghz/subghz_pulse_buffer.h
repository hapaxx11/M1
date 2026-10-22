/* See COPYING.txt for license details. */

#ifndef SUBGHZ_PULSE_BUFFER_H_
#define SUBGHZ_PULSE_BUFFER_H_

#include <stdbool.h>
#include <stdint.h>

static inline bool subghz_pulse_buffer_append(
    uint16_t *pulse_times, uint16_t *pulse_count,
    uint16_t capacity, uint16_t duration)
{
    if (!pulse_times || !pulse_count || *pulse_count >= capacity)
        return false;

    pulse_times[(*pulse_count)++] = duration;
    return true;
}

#endif
