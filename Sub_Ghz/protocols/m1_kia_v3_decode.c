/* See COPYING.txt for license details. */

/*
 * m1_kia_v3_decode.c
 *
 * ProtoPirate "Kia V3" registry wrapper.  The shared state machine lives
 * in m1_kia_v3_v4_decode.c (decode algorithm derived from
 * RocketGod-git/ProtoPirate protocols/kia_v3_v4.c, GPLv3); this wrapper
 * commits only frames whose sync polarity is V3 (sync high 400 us then
 * low 1200 us), so the "Kia V3/V4" registry entry never claims V4 frames.
 *
 * M1 Project -- Hapax fork
 */

#include <stdint.h>

/* Upstream version numbering: is_v3_sync -> version 1. */
#define M1_KIA_V3_V4_VERSION_V3 1u

uint8_t m1_kia_v3_v4_decode_frame(uint16_t protocol_index, uint16_t pulse_count,
                                  uint8_t expected_version);

uint8_t subghz_decode_kia_v3(uint16_t protocol_index, uint16_t pulse_count)
{
    return m1_kia_v3_v4_decode_frame(protocol_index, pulse_count,
                                     M1_KIA_V3_V4_VERSION_V3);
}
