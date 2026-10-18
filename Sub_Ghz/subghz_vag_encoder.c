/* See COPYING.txt for license details. */

/**
 * @file   subghz_vag_encoder.c
 * @brief  VAG frame encoder — TX for captured/decoded VAG keyfobs.
 *
 * Algorithm ported from the ProtoPirate reference (RocketGod-git/ProtoPirate,
 * protocols/vag.c: vag_encoder_build_type1 / _type2 / _type3_4).
 * See subghz_vag_encoder.h for the module overview.
 *
 * This file has no hardware dependencies and is testable on the host.
 *
 * M1 Project — Hapax fork
 */

#include "subghz_vag_encoder.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "m1_aut64.h"

/* Fixed TEA schedule for type 2 (from the reference / M1 decoder). */
static const uint32_t VAG_TEA_KEY[4] = {
    0x0B46502Du, 0x5E253718u, 0x2BF93A19u, 0x622C1206u,
};

/*============================================================================*/
/* Button / dispatch mapping                                                   */
/*============================================================================*/

uint8_t subghz_vag_button_byte(uint8_t btn)
{
    /* ProtoPirate files may carry either normalized M1 buttons or the
     * decrypted high-nibble byte used on the wire. */
    if (btn == 0x10u) btn = 1u;
    else if (btn == 0x20u) btn = 2u;
    else if (btn == 0x40u) btn = 4u;
    return (uint8_t)((btn & 0x0Fu) << 4);
}

uint8_t subghz_vag_effective_type(uint8_t type, uint8_t type_byte)
{
    return (type == 1u && type_byte == 0x00u) ? 2u : type;
}

uint16_t subghz_vag_parse_key2(const char *bytes)
{
    if (bytes == NULL)
        return 0;

    uint16_t value = 0;
    const char *p = bytes;
    while (*p != '\0')
    {
        while (*p == ' ' || *p == '\t')
            p++;
        if (*p == '\0' || *p == '\r' || *p == '\n')
            break;
        char *end = NULL;
        const unsigned long byte = strtoul(p, &end, 16);
        if (end == p)
            break;
        value = (uint16_t)((value << 8) | (uint16_t)(byte & 0xFFu));
        p = end;
    }
    return value;
}

uint8_t subghz_vag_dispatch_byte(uint8_t btn_byte, uint8_t type)
{
    if (type == 1u || type == 2u)
    {
        switch (btn_byte)
        {
            case 0x20u: return 0x2Au;
            case 0x40u: return 0x46u;
            case 0x10u: return 0x1Cu;
            default:    return 0x2Au;
        }
    }
    switch (btn_byte)
    {
        case 0x20u: return 0x2Bu;
        case 0x40u: return 0x47u;
        case 0x10u: return 0x1Du;
        default:    return 0x2Bu;
    }
}

/*============================================================================*/
/* Frame encryption                                                            */
/*============================================================================*/

static void vag_tea_encrypt(uint32_t *left, uint32_t *right)
{
    uint32_t sum = 0;
    for (uint8_t round = 0; round < 32u; round++)
    {
        *left += (((*right << 4) ^ (*right >> 5)) + *right) ^
                 (sum + VAG_TEA_KEY[sum & 3u]);
        sum += 0x9E3779B9u;
        *right += (((*left << 4) ^ (*left >> 5)) + *left) ^
                  (sum + VAG_TEA_KEY[(sum >> 11) & 3u]);
    }
}

bool subghz_vag_encrypt_frame(uint32_t serial, uint32_t cnt, uint8_t btn,
                              uint8_t type, uint8_t type_byte,
                              uint8_t key_idx, uint64_t orig_key1,
                              uint16_t orig_key2, const uint8_t *packed_keys,
                              bool keys_available, uint64_t *key1_out,
                              uint16_t *key2_out)
{
    if (key1_out == NULL || key2_out == NULL)
        return false;

    type = subghz_vag_effective_type(type, type_byte);

    const uint8_t btn_byte = subghz_vag_button_byte(btn);
    const uint8_t dispatch = subghz_vag_dispatch_byte(btn_byte, type);

    uint8_t block[8];
    block[0] = (uint8_t)(serial >> 24);
    block[1] = (uint8_t)(serial >> 16);
    block[2] = (uint8_t)(serial >> 8);
    block[3] = (uint8_t)(serial);
    block[4] = (uint8_t)(cnt);
    block[5] = (uint8_t)(cnt >> 8);
    block[6] = (uint8_t)(cnt >> 16);
    block[7] = btn_byte;

    if (type == 2u)
    {
        uint32_t left = ((uint32_t)block[0] << 24) | ((uint32_t)block[1] << 16) |
                        ((uint32_t)block[2] << 8) | block[3];
        uint32_t right = ((uint32_t)block[4] << 24) | ((uint32_t)block[5] << 16) |
                         ((uint32_t)block[6] << 8) | block[7];
        vag_tea_encrypt(&left, &right);
        block[0] = (uint8_t)(left >> 24);
        block[1] = (uint8_t)(left >> 16);
        block[2] = (uint8_t)(left >> 8);
        block[3] = (uint8_t)(left);
        block[4] = (uint8_t)(right >> 24);
        block[5] = (uint8_t)(right >> 16);
        block[6] = (uint8_t)(right >> 8);
        block[7] = (uint8_t)(right);
    }
    else
    {
        /* AUT64 types 1/3/4 need the keystore. */
        if (!keys_available || packed_keys == NULL)
            return false;

        uint8_t idx = key_idx;
        if (idx == VAG_ENC_KEY_AUTO)
        {
            /* Trial-decrypt the original capture with each key and keep the
             * one that reproduces the capture's serial (persisted in the
             * file's Serial field).  With no usable match, fall back to key
             * 0 (reference default). */
            idx = 0;
            for (uint8_t i = 0; i < 3u; i++)
            {
                m1_aut64_key_t key;
                if (!m1_aut64_unpack(&key, &packed_keys[i * 16u]))
                    continue;
                uint8_t candidate[8] = {
                    (uint8_t)(orig_key1 >> 48), (uint8_t)(orig_key1 >> 40),
                    (uint8_t)(orig_key1 >> 32), (uint8_t)(orig_key1 >> 24),
                    (uint8_t)(orig_key1 >> 16), (uint8_t)(orig_key1 >> 8),
                    (uint8_t)(orig_key1), (uint8_t)(orig_key2 >> 8),
                };
                m1_aut64_decrypt(&key, candidate);
                const uint32_t cand_serial =
                    ((uint32_t)candidate[0] << 24) |
                    ((uint32_t)candidate[1] << 16) |
                    ((uint32_t)candidate[2] << 8) | candidate[3];
                if (cand_serial == serial)
                {
                    idx = i;
                    break;
                }
            }
        }
        else if (idx > 2u)
        {
            return false;
        }

        m1_aut64_key_t key;
        if (!m1_aut64_unpack(&key, &packed_keys[idx * 16u]))
            return false;
        m1_aut64_encrypt(&key, block);
    }

    *key1_out = ((uint64_t)type_byte << 56) |
                ((uint64_t)block[0] << 48) | ((uint64_t)block[1] << 40) |
                ((uint64_t)block[2] << 32) | ((uint64_t)block[3] << 24) |
                ((uint64_t)block[4] << 16) | ((uint64_t)block[5] << 8) |
                block[6];
    *key2_out = (uint16_t)(((uint16_t)block[7] << 8) | dispatch);
    return true;
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

/* Manchester bit: 1 = mark,space ; 0 = space,mark. */
static bool add_manchester_bit(subghz_upload_entry_t *upload, uint32_t *index,
                               uint32_t capacity, bool bit, uint32_t te)
{
    if (!add_level(upload, index, capacity, bit, te))
        return false;
    if (!add_level(upload, index, capacity, !bit, te))
        return false;
    return true;
}

static bool add_pairs(subghz_upload_entry_t *upload, uint32_t *index,
                      uint32_t capacity, uint32_t te, uint32_t pairs)
{
    for (uint32_t i = 0; i < pairs; i++)
    {
        if (!add_level(upload, index, capacity, true, te))
            return false;
        if (!add_level(upload, index, capacity, false, te))
            return false;
    }
    return true;
}

static uint32_t build_type12(subghz_upload_entry_t *upload, uint32_t capacity,
                             uint8_t type, uint64_t key1, uint16_t key2)
{
    uint32_t index = 0;

    if (!add_pairs(upload, &index, capacity, 300u, 220u))
        return 0;
    if (!add_level(upload, &index, capacity, false, 300u))
        return 0;
    if (!add_level(upload, &index, capacity, true, 300u))
        return 0;

    const uint16_t prefix = (type == 2u) ? 0xAF1Cu : 0xAF3Fu;
    for (int8_t i = 15; i >= 0; i--)
    {
        if (!add_manchester_bit(upload, &index, capacity,
                                ((prefix >> i) & 1u) != 0, 300u))
            return 0;
    }

    const uint64_t key1_inv = ~key1;
    for (int8_t i = 63; i >= 0; i--)
    {
        if (!add_manchester_bit(upload, &index, capacity,
                                ((key1_inv >> i) & 1u) != 0, 300u))
            return 0;
    }

    const uint16_t key2_inv = (uint16_t)~key2;
    for (int8_t i = 15; i >= 0; i--)
    {
        if (!add_manchester_bit(upload, &index, capacity,
                                ((key2_inv >> i) & 1u) != 0, 300u))
            return 0;
    }

    if (!add_level(upload, &index, capacity, false, 6000u))
        return 0;
    return index;
}

static uint32_t build_type34(subghz_upload_entry_t *upload, uint32_t capacity,
                             uint64_t key1, uint16_t key2)
{
    uint32_t index = 0;

    for (uint8_t rep = 0; rep < 2u; rep++)
    {
        if (!add_pairs(upload, &index, capacity, 500u, 45u))
            return 0;
        if (!add_level(upload, &index, capacity, true, 1000u))
            return 0;
        if (!add_level(upload, &index, capacity, false, 500u))
            return 0;
        if (!add_pairs(upload, &index, capacity, 750u, 3u))
            return 0;

        for (int8_t i = 63; i >= 0; i--)
        {
            if (!add_manchester_bit(upload, &index, capacity,
                                    ((key1 >> i) & 1u) != 0, 500u))
                return 0;
        }
        for (int8_t i = 15; i >= 0; i--)
        {
            if (!add_manchester_bit(upload, &index, capacity,
                                    ((key2 >> i) & 1u) != 0, 500u))
                return 0;
        }
        if (!add_level(upload, &index, capacity, false, 10000u))
            return 0;
    }
    return index;
}

uint32_t subghz_vag_build_upload(subghz_upload_entry_t *upload,
                                 uint32_t capacity, uint8_t type,
                                 uint64_t key1, uint16_t key2)
{
    if (upload == NULL)
        return 0;

    if (type == 1u || type == 2u)
    {
        if (capacity < VAG_ENC_UPLOAD_T12)
            return 0;
        return build_type12(upload, capacity, type, key1, key2);
    }
    if (capacity < VAG_ENC_UPLOAD_T34)
        return 0;
    return build_type34(upload, capacity, key1, key2);
}
