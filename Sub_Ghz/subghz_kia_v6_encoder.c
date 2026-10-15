/* See COPYING.txt for license details. */

/**
 * @file   subghz_kia_v6_encoder.c
 * @brief  Kia V6 frame encoder — TX for captured/decoded Kia V6 keyfobs.
 *
 * Algorithm ported from the ProtoPirate reference (RocketGod-git/ProtoPirate,
 * protocols/kia_v6.c: kia_v6_encrypt_payload / kia_v6_encode_message /
 * kia_protocol_encoder_v6_build_upload).  See subghz_kia_v6_encoder.h for the
 * module overview.
 *
 * This file has no hardware dependencies and is testable on the host.
 *
 * M1 Project — Hapax fork
 */

#include "subghz_kia_v6_encoder.h"

#include <stddef.h>
#include <string.h>

#include "../../NFC/amiibo/tiny_aes.h"

/* Keystore XOR masks (from the reference). */
#define KIA_V6_XOR_MASK_LOW   0x84AF25FBu
#define KIA_V6_XOR_MASK_HIGH  0x638766ABu

/* Timing (µs). */
#define KIA_V6_TE_SHORT  200u
#define KIA_V6_TE_LONG   400u

#define KIA_V6_PREAMBLE_PAIRS_1  640u
#define KIA_V6_PREAMBLE_PAIRS_2  38u

/*============================================================================*/
/* AES S-box (computed)                                                        */
/*============================================================================*/

/* GF(2^8) multiply by x (the AES field's xtime). */
static uint8_t kia_v6_xtime(uint8_t x)
{
    return (uint8_t)((x << 1) ^ ((x & 0x80u) ? 0x1Bu : 0x00u));
}

static uint8_t kia_v6_gf_mul(uint8_t a, uint8_t b)
{
    uint8_t r = 0;
    while (b != 0u)
    {
        if (b & 1u)
            r ^= a;
        a = kia_v6_xtime(a);
        b >>= 1;
    }
    return r;
}

uint8_t subghz_kia_v6_aes_sbox(uint8_t value)
{
    /* Multiplicative inverse in GF(2^8) via exponentiation (x^254). */
    uint8_t inv = 0;
    if (value != 0u)
    {
        uint8_t result = 1;
        uint8_t base = value;
        for (uint8_t e = 0; e < 8u; e++)   /* 254 = 11111110b */
        {
            if (e > 0u)   /* bit0 of 254 is 0: skip the first multiply */
                result = kia_v6_gf_mul(result, base);
            base = kia_v6_gf_mul(base, base);
        }
        inv = result;
    }
    /* Affine transform: s = inv ^ rotl(inv,1) ^ rotl(inv,2) ^ rotl(inv,3)
     * ^ rotl(inv,4) ^ 0x63. */
    uint8_t s = 0x63u;
    for (uint8_t r = 0; r < 5u; r++)
        s ^= (uint8_t)((inv << r) | (inv >> (8u - r)));
    return s;
}

/*============================================================================*/
/* Frame encryption                                                            */
/*============================================================================*/

void subghz_kia_v6_aes_key(const uint64_t keys[2], uint8_t aes_key[16])
{
    for (uint8_t part = 0; part < 2u; part++)
    {
        const uint32_t high = (uint32_t)(keys[part] >> 32) ^ KIA_V6_XOR_MASK_HIGH;
        const uint32_t low  = (uint32_t)keys[part] ^ KIA_V6_XOR_MASK_LOW;
        for (uint8_t i = 0; i < 4u; i++)
        {
            aes_key[part * 8u + i]      = (uint8_t)(high >> (24u - 8u * i));
            aes_key[part * 8u + 4u + i] = (uint8_t)(low >> (24u - 8u * i));
        }
    }
}

void subghz_kia_v6_encrypt_frame(uint8_t fx, uint32_t serial, uint8_t btn,
                                 uint32_t cnt, const uint64_t keys[2],
                                 uint8_t out[18])
{
    uint8_t plain[16];
    memset(plain, 0, sizeof(plain));
    plain[0]  = fx;
    plain[4]  = (uint8_t)(serial >> 16);
    plain[5]  = (uint8_t)(serial >> 8);
    plain[6]  = (uint8_t)(serial);
    plain[7]  = (uint8_t)(btn & 0x0Fu);
    plain[8]  = (uint8_t)(cnt >> 24);
    plain[9]  = (uint8_t)(cnt >> 16);
    plain[10] = (uint8_t)(cnt >> 8);
    plain[11] = (uint8_t)(cnt);
    plain[12] = subghz_kia_v6_aes_sbox((uint8_t)cnt);

    /* CRC-8 (poly 0x07, init 0xFF) over plain[0..14]. */
    uint8_t crc = 0xFFu;
    for (uint8_t i = 0; i < 15u; i++)
    {
        crc ^= plain[i];
        for (uint8_t bit = 0; bit < 8u; bit++)
        {
            const bool high_bit = (crc & 0x80u) != 0u;
            crc = (uint8_t)(crc << 1);
            if (high_bit)
                crc ^= 0x07u;
        }
    }
    plain[15] = crc;

    uint8_t aes_key[16];
    subghz_kia_v6_aes_key(keys, aes_key);
    struct AES_ctx aes;
    AES_init_ctx(&aes, aes_key);
    AES_ECB_encrypt(&aes, plain);

    /* Logical (pre-inversion) wire frame: fx metadata bytes + ciphertext. */
    out[0] = (uint8_t)(0x20u | (fx >> 4));
    out[1] = (uint8_t)(fx & 0x0Fu);
    memcpy(&out[2], plain, 16);
}

/*============================================================================*/
/* Upload (waveform) builder                                                   */
/*============================================================================*/

static bool add_level(subghz_upload_entry_t *upload, uint32_t *index,
                      uint32_t capacity, bool level, uint32_t duration)
{
    if (*index >= capacity)
        return false;
    upload[*index].level = level;
    upload[*index].duration = duration;
    (*index)++;
    return true;
}

/* Manchester bit, TE=200µs, INVERTED polarity vs Renault/PSA:
 * bit=1 → (space, mark); bit=0 → (mark, space). */
static bool add_manchester_bit(subghz_upload_entry_t *upload, uint32_t *index,
                               uint32_t capacity, bool bit)
{
    if (!add_level(upload, index, capacity, !bit, KIA_V6_TE_SHORT))
        return false;
    if (!add_level(upload, index, capacity, bit, KIA_V6_TE_SHORT))
        return false;
    return true;
}

static bool encode_message(subghz_upload_entry_t *upload, uint32_t *index,
                           uint32_t capacity, uint32_t preamble_pairs,
                           uint32_t p1_lo, uint32_t p1_hi, uint32_t p2_lo,
                           uint32_t p2_hi, uint16_t p3)
{
    for (uint32_t i = 0; i < preamble_pairs; i++)
    {
        if (!add_level(upload, index, capacity, true, KIA_V6_TE_SHORT))
            return false;
        if (!add_level(upload, index, capacity, false, KIA_V6_TE_SHORT))
            return false;
    }

    /* Sync: low 200, high 400, low 200. */
    if (!add_level(upload, index, capacity, false, KIA_V6_TE_SHORT))
        return false;
    if (!add_level(upload, index, capacity, true, KIA_V6_TE_LONG))
        return false;
    if (!add_level(upload, index, capacity, false, KIA_V6_TE_SHORT))
        return false;

    /* 61 bits of part1 (bits 60..0 — the top 3 are supplied by the sync),
     * 64 bits of part2, 16 bits of part3; all data bits inverted on air. */
    for (int8_t b = 60; b >= 0; b--)
    {
        const uint32_t word = (b >= 32) ? p1_hi : p1_lo;
        const uint8_t shift = (uint8_t)((b >= 32) ? (b - 32) : b);
        if (!add_manchester_bit(upload, index, capacity,
                                ((~word) >> shift) & 1u))
            return false;
    }
    for (int8_t b = 63; b >= 0; b--)
    {
        const uint32_t word = (b >= 32) ? p2_hi : p2_lo;
        const uint8_t shift = (uint8_t)((b >= 32) ? (b - 32) : b);
        if (!add_manchester_bit(upload, index, capacity,
                                ((~word) >> shift) & 1u))
            return false;
    }
    for (int8_t b = 15; b >= 0; b--)
    {
        if (!add_manchester_bit(upload, index, capacity, ((~p3) >> b) & 1u))
            return false;
    }
    return true;
}

uint32_t subghz_kia_v6_build_upload(subghz_upload_entry_t *upload,
                                    uint32_t capacity, const uint8_t frame[18])
{
    if (upload == NULL || frame == NULL || capacity < KIA_V6_ENC_UPLOAD_SIZE)
        return 0;

    /* Repack the logical frame into the on-air parts (big-endian). */
    const uint32_t p1_hi = ((uint32_t)frame[0] << 24) |
                           ((uint32_t)frame[1] << 16) |
                           ((uint32_t)frame[2] << 8) | frame[3];
    const uint32_t p1_lo = ((uint32_t)frame[4] << 24) |
                           ((uint32_t)frame[5] << 16) |
                           ((uint32_t)frame[6] << 8) | frame[7];
    const uint32_t p2_hi = ((uint32_t)frame[8] << 24) |
                           ((uint32_t)frame[9] << 16) |
                           ((uint32_t)frame[10] << 8) | frame[11];
    const uint32_t p2_lo = ((uint32_t)frame[12] << 24) |
                           ((uint32_t)frame[13] << 16) |
                           ((uint32_t)frame[14] << 8) | frame[15];
    const uint16_t p3 = (uint16_t)(((uint16_t)frame[16] << 8) | frame[17]);

    uint32_t index = 0;

    if (!encode_message(upload, &index, capacity, KIA_V6_PREAMBLE_PAIRS_1,
                        p1_lo, p1_hi, p2_lo, p2_hi, p3))
        return 0;
    if (!add_level(upload, &index, capacity, false, KIA_V6_TE_LONG))
        return 0;

    if (!encode_message(upload, &index, capacity, KIA_V6_PREAMBLE_PAIRS_2,
                        p1_lo, p1_hi, p2_lo, p2_hi, p3))
        return 0;
    if (!add_level(upload, &index, capacity, false, KIA_V6_TE_LONG))
        return 0;

    return index;
}
