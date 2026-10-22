/* See COPYING.txt for license details. */

#include "subghz_saved_match.h"

#include <string.h>

bool subghz_saved_match_counter_compatible(
    const subghz_saved_match_identity_t *received,
    const subghz_saved_match_identity_t *saved)
{
    if (!received || !saved || !received->has_counter || !saved->has_counter)
        return true;

    const uint32_t diff = received->counter >= saved->counter
                              ? received->counter - saved->counter
                              : saved->counter - received->counter;
    return diff <= SUBGHZ_SAVED_MATCH_COUNTER_MARGIN;
}

bool subghz_saved_match_identity(
    const subghz_saved_match_identity_t *received,
    const subghz_saved_match_identity_t *saved)
{
    if (!received || !saved || received->protocol[0] == '\0' ||
        strcmp(received->protocol, saved->protocol) != 0)
        return false;

    if (received->allow_serial_match && received->has_serial &&
        saved->has_serial)
    {
        if (received->serial != saved->serial)
            return false;
    }
    else
    {
        if (!received->has_key || !saved->has_key ||
            received->key != saved->key)
            return false;
    }

    return subghz_saved_match_counter_compatible(received, saved);
}
