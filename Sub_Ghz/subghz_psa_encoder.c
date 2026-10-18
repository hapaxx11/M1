/* See COPYING.txt for license details. */

/**
 * @file   subghz_psa_encoder.c
 * @brief  PSA frame encoder — TX for type 0x23 and 0x36 captures.
 *
 * Algorithm ported from the ProtoPirate reference (RocketGod-git/ProtoPirate,
 * protocols/psa.c: psa_build_buffer_mode23 / psa_build_buffer_mode36 /
 * psa_encoder_build_upload; protocols/psa_crypto.c: psa_crypto_tea_encrypt).
 * See subghz_psa_encoder.h for the module overview.
 *
 * This file has no hardware dependencies and is testable on the host.
 *
 * M1 Project — Hapax fork
 */

#include "subghz_psa_encoder.h"

#include <stddef.h>
#include <string.h>

#include "subghz_psa_bruteforce.h"  /* TEA primitives, key schedules, tea_crc */

/*============================================================================*/
/* Type 0x36 (TEA)                                                             */
/*============================================================================*/

void subghz_psa_encode_buffer36(uint32_t serial, uint32_t counter,
                                uint8_t button, const uint8_t preserve01[2],
                                uint32_t candidate,
                                subghz_psa_bf_phase_t phase,
                                uint8_t buffer[10])
{
    memset(buffer, 0, 10);

    if (phase == PSA_BF_PHASE_2)
        counter &= 0x0FFFFF00u;
    else if (phase == PSA_BF_PHASE_1)
        counter &= 0x0FFFFFFFu;

    uint32_t v0 = ((serial & 0xFFFFFFu) << 8) | (((uint32_t)button & 0xFu) << 4) |
                  ((counter >> 24) & 0xFu);
    uint32_t v1 = ((counter & 0xFFFFFFu) << 8);

    uint32_t working_key[4];
    if (phase == PSA_BF_PHASE_1)
    {
        const uint8_t crc = subghz_psa_bf_tea_crc(v0, v1);
        v1 = (v1 & 0xFFFFFF00u) | crc;

        subghz_psa_tea_schedule_t bf1_sched;
        subghz_psa_tea_build_schedule(subghz_psa_bf1_key_schedule,
                                      &bf1_sched);
        working_key[2] = 0x0E0F5C41u;
        working_key[3] = candidate;
        subghz_psa_tea_encrypt_scheduled(&working_key[2], &working_key[3],
                                         &bf1_sched);
        working_key[0] = (candidate << 8) | 0x0Eu;
        working_key[1] = 0x0F5C4123u;
        subghz_psa_tea_encrypt_scheduled(&working_key[0], &working_key[1],
                                         &bf1_sched);
    }
    else if (phase == PSA_BF_PHASE_2)
    {
        const uint8_t crc_input[6] = {
            (uint8_t)(v0 >> 24), (uint8_t)(v0 >> 8),
            (uint8_t)(v0 >> 16), (uint8_t)v0,
            (uint8_t)(v1 >> 24), (uint8_t)(v1 >> 16),
        };
        const uint16_t crc = subghz_psa_bf_crc16(crc_input,
                                                  sizeof(crc_input));
        v1 = (v1 & 0xFFFF0000u) | crc;
        for (uint8_t i = 0; i < 4u; i++)
            working_key[i] = subghz_psa_bf2_key_schedule[i] ^ candidate;
    }
    else
    {
        return;
    }

    subghz_psa_tea_encrypt(&v0, &v1, working_key);

    /* Unpack: v0 → buffer[2..5] BE, v1 → buffer[6..9] BE. */
    buffer[2] = (uint8_t)(v0 >> 24);
    buffer[3] = (uint8_t)(v0 >> 16);
    buffer[4] = (uint8_t)(v0 >> 8);
    buffer[5] = (uint8_t)(v0);
    buffer[6] = (uint8_t)(v1 >> 24);
    buffer[7] = (uint8_t)(v1 >> 16);
    buffer[8] = (uint8_t)(v1 >> 8);
    buffer[9] = (uint8_t)(v1);

    if (preserve01 != NULL)
    {
        buffer[0] = preserve01[0];
        buffer[1] = preserve01[1];
    }
    else
    {
        buffer[0] = (uint8_t)(buffer[2] ^ buffer[6]);
        buffer[1] = (uint8_t)(buffer[3] ^ buffer[7]);
    }
}

/*============================================================================*/
/* Type 0x23 (direct XOR)                                                      */
/*============================================================================*/

/* Inverse of the decoder's second-stage XOR un-mixing. */
static void psa_second_stage_xor_encrypt(uint8_t *buffer)
{
    const uint8_t e6 = buffer[8];
    const uint8_t e7 = buffer[9];

    uint8_t p[6];
    p[0] = buffer[2];
    p[1] = buffer[3];
    p[2] = buffer[4];
    p[3] = buffer[5];
    p[4] = buffer[6];
    p[5] = buffer[7];

    const uint8_t e5 = (uint8_t)(p[5] ^ e7 ^ e6);
    const uint8_t e0 = (uint8_t)(p[2] ^ e5);
    const uint8_t e2 = (uint8_t)(p[4] ^ e0);
    const uint8_t e4 = (uint8_t)(p[3] ^ e2);
    const uint8_t e3 = (uint8_t)(p[0] ^ e5);
    const uint8_t e1 = (uint8_t)(p[1] ^ e3);

    buffer[2] = e0;
    buffer[3] = e1;
    buffer[4] = e2;
    buffer[5] = e3;
    buffer[6] = e4;
    buffer[7] = e5;
}

/* Nibble checksum over buffer[2..7], result in the high nibble (decoder-side
 * psa_nibble_checksum equivalent). */
static uint8_t psa_enc_nibble_checksum(const uint8_t *b)
{
    uint32_t checksum = 0;
    for (uint8_t i = 2; i < 8u; i++)
        checksum += (uint32_t)(b[i] & 0x0Fu) + (uint32_t)((b[i] >> 4) & 0x0Fu);
    return (uint8_t)((checksum * 0x10u) & 0xFFu);
}

void subghz_psa_encode_buffer23(uint32_t serial, uint16_t counter,
                                uint8_t crc, uint8_t button, uint16_t key2,
                                bool has_key2, const uint8_t preserve01[2],
                                uint8_t buffer[10])
{
    memset(buffer, 0, 10);

    buffer[2] = (uint8_t)((serial >> 16) & 0xFFu);
    buffer[3] = (uint8_t)((serial >> 8) & 0xFFu);
    buffer[4] = (uint8_t)(serial & 0xFFu);
    buffer[5] = (uint8_t)((counter >> 8) & 0xFFu);
    buffer[6] = (uint8_t)(counter & 0xFFu);
    buffer[7] = crc;
    buffer[8] = (uint8_t)(button & 0xFu);

    const uint8_t initial_plaintext[6] = {buffer[2], buffer[3], buffer[4],
                                          buffer[5], buffer[6], buffer[7]};
    const uint8_t initial_button = (uint8_t)(buffer[8] & 0xFu);

    /* Nibble search: find buffer[8] high nibble / buffer[9] such that the
     * post-encryption nibble checksum matches buffer[8]'s high nibble (the
     * decoder's validation).  Prefers the captured Key_2 low byte when one
     * is known. */
    bool found = false;
    const uint8_t b9_start = has_key2 ? (uint8_t)(key2 & 0xFFu) : 0u;
    const uint16_t b9_end = has_key2 ? (uint16_t)(b9_start + 1u) : 255u;

    for (uint16_t b9 = b9_start; b9 < b9_end && !found; b9++)
    {
        for (uint8_t b8_hi = 0; b8_hi < 16u && !found; b8_hi++)
        {
            memcpy(&buffer[2], initial_plaintext, 6);
            buffer[8] = (uint8_t)(initial_button | (b8_hi << 4));
            buffer[9] = (uint8_t)b9;

            psa_second_stage_xor_encrypt(buffer);
            const uint8_t checksum = psa_enc_nibble_checksum(buffer);

            if (((checksum ^ buffer[8]) & 0xF0u) == 0u)
            {
                buffer[8] = (uint8_t)((buffer[8] & 0x0Fu) | (checksum & 0xF0u));
                found = true;
            }
        }
    }

    if (!found)
    {
        /* Fallback: default Key_2 low byte, accept whatever checksum. */
        memcpy(&buffer[2], initial_plaintext, 6);
        buffer[8] = initial_button;
        buffer[9] = has_key2 ? (uint8_t)(key2 & 0xFFu) : 0x23u;
        psa_second_stage_xor_encrypt(buffer);
        const uint8_t checksum = psa_enc_nibble_checksum(buffer);
        buffer[8] = (uint8_t)((buffer[8] & 0x0Fu) | (checksum & 0xF0u));
    }

    if (preserve01 != NULL)
    {
        buffer[0] = preserve01[0];
        buffer[1] = preserve01[1];
    }
    else
    {
        buffer[0] = (uint8_t)(buffer[2] ^ buffer[6]);
        buffer[1] = (uint8_t)(buffer[3] ^ buffer[7]);
    }
}

/*============================================================================*/
/* Upload (waveform) builder                                                   */
/*============================================================================*/

#define PSA_TE          250u
#define PSA_TE_LONG     500u
#define PSA_TE_END      1000u

static bool add_level(psa_upload_entry_t *upload, uint32_t *index,
                      uint32_t capacity, bool level, uint32_t duration)
{
    if (*index >= capacity)
        return false;
    upload[*index].level = level;
    upload[*index].duration = duration;
    (*index)++;
    return true;
}

static bool add_bits(psa_upload_entry_t *upload, uint32_t *index,
                     uint32_t capacity, uint64_t value, uint8_t bit_count)
{
    for (uint8_t i = bit_count; i > 0; i--)
    {
        const bool one = ((value >> (i - 1u)) & 1u) != 0;
        /* Manchester: 1 = mark,space ; 0 = space,mark (TE each). */
        if (!add_level(upload, index, capacity, one, PSA_TE))
            return false;
        if (!add_level(upload, index, capacity, !one, PSA_TE))
            return false;
    }
    return true;
}

uint32_t subghz_psa_build_upload(psa_upload_entry_t *upload,
                                 uint32_t capacity, const uint8_t buffer[10])
{
    if (upload == NULL || buffer == NULL ||
        capacity < PSA_ENC_UPLOAD_CAPACITY)
        return 0;

    uint32_t index = 0;

    /* Preamble: 80 mark/space pairs. */
    for (uint8_t i = 0; i < 80u; i++)
    {
        if (!add_level(upload, &index, capacity, true, PSA_TE))
            return 0;
        if (!add_level(upload, &index, capacity, false, PSA_TE))
            return 0;
    }

    /* Transition: low 250, high 500, low 250. */
    if (!add_level(upload, &index, capacity, false, PSA_TE))
        return 0;
    if (!add_level(upload, &index, capacity, true, PSA_TE_LONG))
        return 0;
    if (!add_level(upload, &index, capacity, false, PSA_TE))
        return 0;

    /* Key1: buffer[0..7] big-endian, 64 bits MSB-first. */
    uint64_t key1 = 0;
    for (uint8_t i = 0; i < 8u; i++)
        key1 = (key1 << 8) | buffer[i];
    if (!add_bits(upload, &index, capacity, key1, 64u))
        return 0;

    /* Validation field: buffer[8..9], 16 bits MSB-first. */
    const uint16_t validation = (uint16_t)(((uint16_t)buffer[8] << 8) |
                                           buffer[9]);
    if (!add_bits(upload, &index, capacity, validation, 16u))
        return 0;

    /* End: high 1000, low 1000. */
    if (!add_level(upload, &index, capacity, true, PSA_TE_END))
        return 0;
    if (!add_level(upload, &index, capacity, false, PSA_TE_END))
        return 0;

    return index;
}
