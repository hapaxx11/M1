/* See COPYING.txt for license details. */

/*
 * m1_kia_v4_decode.c
 *
 * ProtoPirate "Kia V4" registry wrapper.  The shared state machine lives
 * in m1_kia_v3_v4_decode.c (decode algorithm derived from
 * RocketGod-git/ProtoPirate protocols/kia_v3_v4.c, GPLv3); this wrapper
 * commits only frames whose sync polarity is V4 (sync low 400 us then
 * high 1200 us), so the "Kia V4" registry entry never claims V3 frames.
 *
 * M1 Project -- Hapax fork
 */

#include <stdint.h>

/* Upstream version numbering: 0 = V4 polarity. */
#define M1_KIA_V3_V4_VERSION_V4 0u

uint8_t m1_kia_v3_v4_decode_frame(uint16_t protocol_index, uint16_t pulse_count,
                                  uint8_t expected_version);

uint8_t subghz_decode_kia_v4(uint16_t protocol_index, uint16_t pulse_count)
{
    return m1_kia_v3_v4_decode_frame(protocol_index, pulse_count,
                                     M1_KIA_V3_V4_VERSION_V4);
}
