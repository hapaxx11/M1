/* See COPYING.txt for license details. */

#ifndef M1_KIA_V6_DECODE_H
#define M1_KIA_V6_DECODE_H

#include <stdbool.h>
#include <stdint.h>

#define M1_KIA_V6_FRAME_BITS 144u

typedef struct {
    uint32_t serial;
    uint32_t counter;
    uint8_t button;
    uint8_t fx;
} m1_kia_v6_data_t;

bool m1_kia_v6_parse(const uint8_t frame_bits[M1_KIA_V6_FRAME_BITS],
                     const uint64_t keys[2], m1_kia_v6_data_t *data);
uint8_t subghz_decode_kia_v6(uint16_t protocol_index, uint16_t pulse_count);

#endif
