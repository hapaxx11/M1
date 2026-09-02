/* See COPYING.txt for license details. */

/**
 * @file   m1_pet_tag.c
 * @brief  Pet Tag Scanner — pure-logic FDX-B (ISO 11784/11785) decoder.
 *
 * See m1_pet_tag.h for the public API. The FDX-B decoded payload stores each
 * multi-bit field LSB-first (order of transmission), so the numeric value of a
 * field is recovered by weighting received bit @c i by @c 2^i.  This matches
 * the reference Flipper Zero FDX-B decoding (national code, country code and
 * the animal-application flag).
 *
 * FDX-B decoded byte layout (bit offsets within the 88-bit payload):
 *   bits  0..37  National identification code (38 bits)
 *   bits 38..47  Country code (10 bits)
 *   bit  48      Data-block status flag
 *   bit  63      Animal-application indicator
 *   bits 64..87  Optional extra data block (e.g. temperature)
 */

#include "m1_pet_tag.h"

#include <string.h>
#include <stdio.h>

#include "lfrfid_bit_lib.h"

/*============================================================================*/
/* Recover the numeric value of an LSB-first field of @p len bits starting at  */
/* bit @p pos (received bit i contributes weight 2^i).                         */
/*============================================================================*/
static uint64_t pet_field_lsb_first(const uint8_t *data, size_t pos, uint8_t len)
{
    uint64_t value = 0;
    for (uint8_t i = 0; i < len; i++) {
        if (bl_get_bit(data, pos + i))
            value |= (uint64_t)1 << i;
    }
    return value;
}

/*============================================================================*/
bool m1_pet_tag_decode_fdxb(const uint8_t *data, m1_pet_tag_info_t *out)
{
    if (data == NULL || out == NULL)
        return false;

    memset(out, 0, sizeof(*out));

    out->national_code = pet_field_lsb_first(data, 0, 38);
    out->country_code  = (uint16_t)pet_field_lsb_first(data, 38, 10);
    out->block_status  = bl_get_bit(data, 48);
    out->animal        = bl_get_bit(data, 63);

    snprintf(out->id_string, sizeof(out->id_string), "%03u-%012llu",
             (unsigned)out->country_code,
             (unsigned long long)out->national_code);

    return true;
}
