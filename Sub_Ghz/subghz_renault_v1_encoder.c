/* See COPYING.txt for license details. */

/**
 * @file   subghz_renault_v1_encoder.c
 * @brief  Renault V1 (Hitag2) frame encoder — TX after seed recovery.
 *
 * Algorithm ported verbatim from the ProtoPirate reference
 * (RocketGod-git/ProtoPirate, protocols/renault_v1.c: hitag2_build_iv,
 * hitag2_encrypt_from_iv, hitag2_frame_xor, renault_v1_encoder_get_upload).
 * See subghz_renault_v1_encoder.h for the module overview.
 *
 * This file has no hardware dependencies and is testable on the host.
 *
 * M1 Project — Hapax fork
 */

#include "subghz_renault_v1_encoder.h"

#include <stddef.h>

#include "subghz_renault_v1_bruteforce.h"  /* serial_permute + keystream cipher */

/*============================================================================*/
/* Frame encryption (seed path)                                                */
/*============================================================================*/

void subghz_renault_v1_build_iv(uint32_t cnt, uint8_t btn, uint32_t seed,
                                uint8_t iv[4])
{
    iv[0] = (uint8_t)(((cnt << 4) & 0xF0u) | (btn & 0x0Fu));
    iv[1] = (uint8_t)((cnt >> 4) & 0xFFu);
    iv[2] = (uint8_t)(((seed >> 8) & 0xF0u) | ((cnt >> 12) & 0x0Fu));
    iv[3] = (uint8_t)(seed & 0xFFu);
}

void subghz_renault_v1_encrypt_frame(uint32_t serial, uint32_t cnt,
                                     uint8_t btn, uint32_t seed,
                                     uint8_t out[11], uint8_t iv[4])
{
    const uint8_t serial_be[4] = {
        (uint8_t)(serial >> 24),
        (uint8_t)(serial >> 16),
        (uint8_t)(serial >> 8),
        (uint8_t)serial,
    };

    uint8_t iv_local[4];
    subghz_renault_v1_build_iv(cnt, btn, seed, iv_local);

    /* Clock the cipher: keystream lands in iv_work, final state in state[5]
     * (the reference uses state[5] for the last hop byte). */
    uint8_t perm[6];
    subghz_hitag2_serial_permute(serial_be, perm);

    uint8_t keystream[4];
    uint8_t state5 = 0;
    subghz_hitag2_keystream_ex(serial_be, perm, iv_local, keystream, &state5);

    /* Reference hitag2_encrypt_from_iv hop extraction. */
    const uint8_t hop0 = keystream[0];
    uint8_t hop1 = (uint8_t)((hop0 >> 7) | (keystream[1] << 1));
    uint8_t hop2 = (uint8_t)((keystream[1] >> 7) | (keystream[2] << 1));
    const uint8_t hop_ext = (uint8_t)(((hop0 >> 6) & 1u) | (hop1 << 1));
    uint8_t hop3 = (uint8_t)((keystream[2] >> 7) | (keystream[3] << 1));
    uint8_t hop4 = (uint8_t)((keystream[3] >> 7) | (state5 << 1));
    hop1 = (uint8_t)((hop1 >> 7) | (hop2 << 1));
    hop2 = (uint8_t)((hop2 >> 7) | (hop3 << 1));
    hop3 = (uint8_t)((hop3 >> 7) | (hop4 << 1));

    uint32_t mix = ((uint32_t)iv_local[1] << 4) | ((uint32_t)iv_local[0] >> 4);
    mix = (mix | (((uint32_t)iv_local[2] << 12) & 0xFFFFu)) & 0xFFFFu;

    out[0] = serial_be[0];
    out[1] = serial_be[1];
    out[2] = serial_be[2];
    out[3] = serial_be[3];
    out[4] = (uint8_t)(((mix >> 6) & 0x0Fu) | ((iv_local[0] << 4) & 0xF0u));
    out[5] = (uint8_t)((hop3 & 3u) | ((mix << 2) & 0xFCu));
    out[6] = hop2;
    out[7] = hop1;
    out[8] = hop_ext;
    out[9] = (uint8_t)((hop0 << 2) | 2u);  /* tail = 0b10 on the seed path */

    uint8_t x = 0;
    for (uint8_t i = 0; i < 10u; i++)
        x ^= out[i];
    out[10] = x;

    if (iv != NULL)
    {
        iv[0] = iv_local[0];
        iv[1] = iv_local[1];
        iv[2] = iv_local[2];
        iv[3] = iv_local[3];
    }
}

/*============================================================================*/
/* Upload (waveform) builder                                                   */
/*============================================================================*/

static bool add_level(renault_v1_upload_entry_t *upload, uint32_t *index,
                      uint32_t capacity, bool level, uint32_t duration)
{
    if (*index >= capacity)
        return false;
    upload[*index].level = level;
    upload[*index].duration = duration;
    (*index)++;
    return true;
}

static bool add_bits(renault_v1_upload_entry_t *upload, uint32_t *index,
                     uint32_t capacity, uint64_t value, uint8_t bit_count)
{
    for (uint8_t i = bit_count; i > 0; i--)
    {
        const bool one = ((value >> (i - 1u)) & 1u) != 0;
        /* Manchester: 1 = mark,space ; 0 = space,mark (TE each). */
        if (!add_level(upload, index, capacity, one, RENAULT_V1_TE_US))
            return false;
        if (!add_level(upload, index, capacity, !one, RENAULT_V1_TE_US))
            return false;
    }
    return true;
}

uint32_t subghz_renault_v1_build_upload(renault_v1_upload_entry_t *upload,
                                        uint32_t capacity, uint64_t key,
                                        uint32_t key2_24)
{
    if (upload == NULL || capacity < RENAULT_V1_UPLOAD_CAPACITY)
        return 0;

    uint32_t index = 0;

    for (uint32_t i = 0; i < RENAULT_V1_PREAMBLE_PAIRS; i++)
    {
        if (!add_level(upload, &index, capacity, true, RENAULT_V1_TE_US))
            return 0;
        if (!add_level(upload, &index, capacity, false, RENAULT_V1_TE_US))
            return 0;
    }

    for (uint32_t f = 0; f < RENAULT_V1_LONG_FRAMES; f++)
    {
        if (!add_level(upload, &index, capacity, false,
                       RENAULT_V1_HEADER_LOW_US))
            return 0;
        if (!add_level(upload, &index, capacity, true,
                       RENAULT_V1_HEADER_HIGH_US))
            return 0;
        if (!add_bits(upload, &index, capacity, 1, 16))   /* header bits */
            return 0;
        if (!add_bits(upload, &index, capacity, key, 64))
            return 0;
        if (!add_bits(upload, &index, capacity, key2_24 & 0xFFFFFFu, 24))
            return 0;
    }

    const uint64_t short_key = (key >> 18) & 0x3FFu;
    for (uint32_t f = 0; f < RENAULT_V1_SHORT_FRAMES; f++)
    {
        if (!add_level(upload, &index, capacity, false,
                       RENAULT_V1_HEADER_LOW_US))
            return 0;
        if (!add_level(upload, &index, capacity, true,
                       RENAULT_V1_HEADER_HIGH_US))
            return 0;
        if (!add_bits(upload, &index, capacity, 1, 16))
            return 0;
        if (!add_bits(upload, &index, capacity, short_key, 10))
            return 0;
        if (!add_level(upload, &index, capacity, false,
                       RENAULT_V1_SHORT_GAP_US))
            return 0;
    }

    return index;
}
