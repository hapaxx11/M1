/* See COPYING.txt for license details. */

/*
 * m1_ford_v1_decode.c
 *
 * Placeholder decoder for ProtoPirate "ford_v1".
 * The TX encoder lives in Sub_Ghz/subghz_proto_pirate.c.
 * Decode is not yet implemented; this file exists so the registry entry
 * compiles and the protocol can be detected by name.
 *
 * M1 Project -- Hapax fork
 */

#include <stdint.h>
#include "subghz_protocol_registry.h"

uint8_t subghz_decode_ford_v1(uint16_t protocol_index, uint16_t pulse_count)
{
    (void)protocol_index;
    (void)pulse_count;
    return 1; /* not implemented */
}
