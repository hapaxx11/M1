/* See COPYING.txt for license details. */

/**
 * @file   subghz_renault_v1_bruteforce.c
 * @brief  Renault V1 Hitag2 IV (seed) brute-force recovery — pure-logic module.
 *
 * Algorithm ported verbatim from the ProtoPirate reference
 * (RocketGod-git/ProtoPirate, protocols/renault_v1.c).  See
 * subghz_renault_v1_bruteforce.h for the search overview.
 *
 * This file has no hardware dependencies and is testable on the host.
 *
 * M1 Project — Hapax fork
 */

#include "subghz_renault_v1_bruteforce.h"

#include <stddef.h>

/*============================================================================*/
/* Frame preparation                                                           */
/*============================================================================*/

void subghz_renault_v1_bf_frame_from_capture(uint8_t frame[RENAULT_V1_BF_FRAME_BYTES],
                                             uint64_t key, uint32_t key2_low24)
{
    for (uint8_t i = 0; i < 8u; i++)
        frame[i] = (uint8_t)(key >> (56u - i * 8u));
    frame[8]  = (uint8_t)((key2_low24 >> 16) & 0xFFu);
    frame[9]  = (uint8_t)((key2_low24 >> 8) & 0xFFu);
    frame[10] = (uint8_t)(key2_low24 & 0xFFu);
}

/* Reference bit shuffle over the reversed frame (hitag2_bf_rearrange_dest). */
static void renault_v1_bf_rearrange_dest(uint8_t dest[11])
{
    const uint8_t frame4 = dest[6];
    uint8_t frame5 = dest[5];
    uint8_t frame6 = dest[4];
    uint8_t frame7 = dest[3];
    const uint8_t frame8 = dest[2];
    const uint8_t frame9 = dest[1];

    uint8_t hop0 = (uint8_t)(((frame4 & 1u) << 7) | (frame5 >> 1));
    frame5 = (uint8_t)(((frame5 & 1u) << 7) | (frame6 >> 1));
    const uint8_t hop0_hi = (uint8_t)(hop0 >> 1);
    hop0 = (uint8_t)(hop0 & 1u);
    hop0 = (uint8_t)((hop0 << 7) | (frame5 >> 1));
    frame6 = (uint8_t)(((frame6 & 1u) << 7) | (frame7 >> 1));
    frame5 = (uint8_t)(((frame5 & 1u) << 7) | (frame6 >> 1));
    dest[3] = frame5;
    frame7 = (uint8_t)(((frame7 & 1u) << 7) | (frame8 >> 1));
    const uint8_t hop2 = (uint8_t)(((frame6 & 1u) << 7) | (frame7 >> 1));
    frame6 = (uint8_t)(((frame8 & 1u) << 7) | (frame9 >> 1));
    dest[6] = (uint8_t)(((frame9 & 1u) << 7) | (frame4 >> 2));
    dest[1] = (uint8_t)(((frame7 & 1u) << 7) | (frame6 >> 1));
    dest[5] = (uint8_t)(hop0_hi | (((frame4 >> 1) & 1u) << 7));
    dest[2] = hop2;
    dest[4] = hop0;
}

void subghz_renault_v1_bf_prepare(const uint8_t frame[RENAULT_V1_BF_FRAME_BYTES],
                                  uint8_t serial_be[4], uint8_t perm[6],
                                  uint8_t hop_target[4],
                                  uint8_t *iv0, uint8_t *fp)
{
    uint8_t dest[11];
    for (uint8_t i = 0; i < 11u; i++)
        dest[i] = frame[10u - i];
    renault_v1_bf_rearrange_dest(dest);

    hop_target[0] = dest[1];
    hop_target[1] = dest[2];
    hop_target[2] = dest[3];
    hop_target[3] = dest[4];
    *iv0 = (uint8_t)((frame[4] >> 4) | (dest[5] << 4));
    *fp  = (uint8_t)((dest[5] >> 4) | ((dest[6] & 3u) << 4));

    serial_be[0] = frame[0];
    serial_be[1] = frame[1];
    serial_be[2] = frame[2];
    serial_be[3] = frame[3];
    subghz_hitag2_serial_permute(serial_be, perm);
}

/*============================================================================*/
/* Hitag2 cipher (byte-oriented form)                                          */
/*============================================================================*/

static inline uint8_t hitag2_extract_bits(uint32_t value, uint8_t lsb,
                                          uint8_t width)
{
    return (uint8_t)((value >> lsb) & ((1u << width) - 1u));
}

void subghz_hitag2_serial_permute(const uint8_t serial[4], uint8_t perm[6])
{
    const uint8_t sn0 = serial[0];
    const uint8_t sn1 = serial[1];
    const uint8_t sn2 = serial[2];
    const uint8_t sn3 = serial[3];

    uint8_t acc =
        (uint8_t)(((sn0 >> 6) & 2u) | ((sn1 >> 4) & 8u) |
                  hitag2_extract_bits((uint8_t)(sn0 ^ 0x10u), 4, 1));
    acc |= (uint8_t)((~(uint32_t)(sn0 << 2)) & 0x20u);
    acc |= (uint8_t)((sn0 << 5) & 0x40u);
    acc |= (uint8_t)((sn2 << 1) & 0x80u);
    acc |= (uint8_t)((~(uint32_t)(sn2 >> 5)) & 4u);
    acc |= (uint8_t)((~(uint32_t)(sn0 >> 2)) & 0x10u);
    perm[0] = acc;

    const uint8_t sn1_inv_shr3 = (uint8_t)~(sn1 >> 3);
    const uint8_t sn3_inv_shl3 = (uint8_t)~(sn3 << 3);
    acc = (uint8_t)(hitag2_extract_bits(sn1, 5, 1) | (sn1_inv_shr3 & 4u) |
                    (sn0 & 0x20u));
    acc |= (uint8_t)(sn3_inv_shl3 & 8u);
    acc |= (uint8_t)((~(uint32_t)(sn2 << 2)) & 0x10u);
    acc |= (uint8_t)((~(uint32_t)(sn2 << 3)) & 0x40u);
    acc |= 0x80u;
    perm[1] = acc;

    const uint8_t sn0_shr3 = (uint8_t)(sn0 >> 3);
    acc = (uint8_t)(hitag2_extract_bits(sn0, 2, 1) | (sn0_shr3 & 2u) |
                    ((~(uint32_t)(sn3 >> 2)) & 4u) | (sn1 & 0x10u));
    acc |= (uint8_t)((~(uint32_t)(sn1 << 4)) & 0x20u);
    acc |= (uint8_t)(sn3_inv_shl3 & 0x40u);
    acc |= 0x80u;
    perm[2] = acc;

    const uint8_t sn2_inv_shl6 = (uint8_t)~(sn2 << 6);
    acc = (uint8_t)(((sn0 >> 2) & 2u) | ((sn1 >> 3) & 8u) |
                    hitag2_extract_bits((uint8_t)(sn2 ^ 0x20u), 5, 1));
    acc |= (uint8_t)((sn1 << 5) & 0x20u);
    acc |= (uint8_t)((sn3 << 1) & 0x40u);
    acc |= (uint8_t)(((uint8_t)~sn0_shr3) & 4u);
    acc |= (uint8_t)(sn1_inv_shr3 & 0x10u);
    acc |= (uint8_t)(sn2_inv_shl6 & 0x80u);
    perm[3] = acc;

    uint8_t perm4_lo =
        (uint8_t)(((sn3 << 2) & 8u) | ((sn0 << 4) & 0x10u) |
                  hitag2_extract_bits((uint8_t)(sn0 ^ 4u), 2, 1));
    perm4_lo |= (uint8_t)((~(uint32_t)(sn3 >> 1)) & 2u);
    perm4_lo |= (uint8_t)((~(uint32_t)(sn1 >> 4)) & 4u);
    uint8_t perm4_hi = (uint8_t)((~(uint32_t)(sn0 << 4)) & 0x20u);
    perm4_hi |= (uint8_t)(sn2_inv_shl6 & 0x40u);
    const uint8_t sn1_inv_shl3 = (uint8_t)~(sn1 << 3);
    perm4_hi |= (uint8_t)(sn1_inv_shl3 & 0x80u);
    perm[4] = (uint8_t)(perm4_lo | perm4_hi);

    uint8_t perm5 =
        (uint8_t)(((sn3 >> 2) & 0x10u) | ((sn2 >> 3) & 2u) |
                  (((uint8_t)~sn0) & 0x80u));
    perm5 |= (uint8_t)((~(uint32_t)(sn0 << 3)) & 8u);
    perm5 |= (uint8_t)((sn0 >> 2) & 0x10u);
    perm5 |= (uint8_t)(sn1_inv_shl3 & 0x20u);
    perm5 |= (uint8_t)((sn3 >> 1) & 0x40u);
    perm5 |= (uint8_t)((~(uint32_t)(sn1 >> 1)) & 4u);
    perm[5] = perm5;
}

static inline uint8_t hitag2_truth(uint32_t table, uint8_t index)
{
    return (uint8_t)((table >> index) & 1u);
}

static inline uint8_t hitag2_filter_index(uint8_t a, uint8_t b, uint8_t c,
                                          uint8_t d)
{
    return (uint8_t)((a << 3) | (b << 2) | (c << 1) | d);
}

static inline uint8_t hitag2_byte_bit(uint8_t byte, uint8_t bit)
{
    return (uint8_t)((byte >> bit) & 1u);
}

static uint8_t hitag2_filter(const uint8_t state[6])
{
    uint8_t group = 0;
    group |= hitag2_truth(
        0x2C79u,
        hitag2_filter_index(hitag2_byte_bit(state[0], 1),
                            hitag2_byte_bit(state[0], 2),
                            hitag2_byte_bit(state[0], 4),
                            hitag2_byte_bit(state[0], 5)));
    group |= (uint8_t)(hitag2_truth(0x6671u,
                                    hitag2_filter_index(
                                        hitag2_byte_bit(state[1], 0),
                                        hitag2_byte_bit(state[1], 1),
                                        hitag2_byte_bit(state[1], 3),
                                        hitag2_byte_bit(state[1], 7)))
                       << 1);
    group |= (uint8_t)(hitag2_truth(0x6671u,
                                    hitag2_filter_index(
                                        hitag2_byte_bit(state[3], 5),
                                        hitag2_byte_bit(state[2], 0),
                                        hitag2_byte_bit(state[2], 2),
                                        hitag2_byte_bit(state[2], 6)))
                       << 2);
    group |= (uint8_t)(hitag2_truth(0x6671u,
                                    hitag2_filter_index(
                                        hitag2_byte_bit(state[4], 6),
                                        hitag2_byte_bit(state[3], 0),
                                        hitag2_byte_bit(state[3], 2),
                                        hitag2_byte_bit(state[3], 3)))
                       << 3);
    group |= (uint8_t)(hitag2_truth(0x2C79u,
                                    hitag2_filter_index(
                                        hitag2_byte_bit(state[5], 1),
                                        hitag2_byte_bit(state[5], 3),
                                        hitag2_byte_bit(state[5], 4),
                                        hitag2_byte_bit(state[4], 5)))
                       << 4);
    return hitag2_truth(0x7907287BUL, group);
}

static inline uint8_t hitag2_parity8(uint8_t value)
{
    value ^= (uint8_t)(value >> 4);
    value ^= (uint8_t)(value >> 2);
    value ^= (uint8_t)(value >> 1);
    return (uint8_t)(value & 1u);
}

static uint8_t hitag2_feedback(const uint8_t state[6])
{
    static const uint8_t masks[6] = {0xB3u, 0x80u, 0x83u, 0x22u, 0x00u, 0x73u};
    uint8_t feedback = 0;
    for (uint8_t i = 0; i < 6u; i++)
        feedback ^= hitag2_parity8((uint8_t)(state[i] & masks[i]));
    return (uint8_t)(feedback & 1u);
}

static void hitag2_shift_state(uint8_t state[6], uint8_t input)
{
    for (uint8_t i = 0; i < 5u; i++)
        state[i] = (uint8_t)((state[i] << 1) | (state[i + 1u] >> 7));
    state[5] = (uint8_t)((state[5] << 1) | (input & 1u));
}

static void hitag2_shift_u32(uint8_t buf[4], uint8_t inject)
{
    const uint8_t b0 = buf[0];
    const uint8_t b1 = buf[1];
    const uint8_t b2 = buf[2];
    const uint8_t b3 = buf[3];
    buf[3] = (uint8_t)((b2 >> 7) | (b3 << 1));
    buf[2] = (uint8_t)((b1 >> 7) | (b2 << 1));
    buf[1] = (uint8_t)((b0 >> 7) | (b1 << 1));
    buf[0] = (uint8_t)((b0 << 1) | (inject & 1u));
}

static void hitag2_clock_cipher(uint8_t state[6], uint8_t iv_work[4],
                                uint8_t iv_orig[4])
{
    for (uint8_t i = 0; i < 32u; i++)
    {
        const uint8_t filter = hitag2_filter(state);
        uint8_t mix = (iv_work[3] & 0x80u) ? (filter ? 0u : 1u)
                                           : (filter ? 1u : 0u);
        if (iv_orig[3] & 0x80u)
            mix ^= 5u;
        hitag2_shift_state(state, (uint8_t)(mix & 1u));
        hitag2_shift_u32(iv_work, 0);
        hitag2_shift_u32(iv_orig, (uint8_t)((mix >> 2) & 1u));
    }

    for (uint8_t i = 0; i < 32u; i++)
    {
        hitag2_shift_u32(iv_work, 0);
        if (hitag2_filter(state))
            iv_work[0] |= 1u;
        hitag2_shift_state(state, hitag2_feedback(state));
    }
}

void subghz_hitag2_keystream_ex(const uint8_t serial_be[4],
                                const uint8_t perm[6], const uint8_t iv[4],
                                uint8_t keystream[4], uint8_t *state5_out)
{
    uint8_t state[6];
    uint8_t iv_work[4];
    uint8_t iv_orig[4];

    state[0] = serial_be[0];
    state[1] = serial_be[1];
    state[2] = serial_be[2];
    state[3] = serial_be[3];
    state[4] = perm[4];
    state[5] = perm[5];
    iv_work[0] = perm[0];
    iv_work[1] = perm[1];
    iv_work[2] = perm[2];
    iv_work[3] = perm[3];
    iv_orig[0] = iv[0];
    iv_orig[1] = iv[1];
    iv_orig[2] = iv[2];
    iv_orig[3] = iv[3];
    hitag2_clock_cipher(state, iv_work, iv_orig);

    keystream[0] = iv_work[0];
    keystream[1] = iv_work[1];
    keystream[2] = iv_work[2];
    keystream[3] = iv_work[3];
    if (state5_out != NULL)
        *state5_out = state[5];
}

void subghz_hitag2_keystream(const uint8_t serial_be[4], const uint8_t perm[6],
                             const uint8_t iv[4], uint8_t keystream[4])
{
    subghz_hitag2_keystream_ex(serial_be, perm, iv, keystream, NULL);
}

/*============================================================================*/
/* Search                                                                      */
/*============================================================================*/

bool subghz_renault_v1_bf_run_range(subghz_bruteforce_job_t *job,
                                    const uint8_t frame[RENAULT_V1_BF_FRAME_BYTES],
                                    uint32_t start, uint32_t end,
                                    uint8_t iv_out[4])
{
    if (job == NULL || frame == NULL || end < start ||
        end > RENAULT_V1_BF_CANDIDATES)
        return false;

    uint8_t serial_be[4];
    uint8_t perm[6];
    uint8_t hop_target[4];
    uint8_t iv0 = 0, fp = 0;
    subghz_renault_v1_bf_prepare(frame, serial_be, perm, hop_target, &iv0,
                                 &fp);

    for (uint32_t cand = start; cand < end; cand++)
    {
        uint8_t iv[4];
        iv[0] = iv0;
        iv[1] = (uint8_t)(fp | ((cand & 3u) << 6));
        iv[2] = (uint8_t)(cand >> 2);
        iv[3] = (uint8_t)(cand >> 10);

        uint8_t keystream[4];
        subghz_hitag2_keystream(serial_be, perm, iv, keystream);
        const bool matched =
            keystream[0] == hop_target[0] && keystream[1] == hop_target[1] &&
            keystream[2] == hop_target[2] && keystream[3] == hop_target[3];

        if (!subghz_bruteforce_job_advance(job, matched, cand))
            return false;  /* range overran the armed job total */
        if (job->status != SUBGHZ_BRUTEFORCE_JOB_RUNNING)
        {
            if (job->status == SUBGHZ_BRUTEFORCE_JOB_FOUND)
            {
                if (iv_out != NULL)
                {
                    iv_out[0] = iv[0];
                    iv_out[1] = iv[1];
                    iv_out[2] = iv[2];
                    iv_out[3] = iv[3];
                }
                return true;
            }
            return false;  /* cancelled or exhausted */
        }
    }

    return false;
}

bool subghz_renault_v1_bf_run(subghz_bruteforce_job_t *job,
                              const uint8_t frame[RENAULT_V1_BF_FRAME_BYTES],
                              uint8_t iv_out[4])
{
    if (job == NULL || frame == NULL)
        return false;

    if (job->status != SUBGHZ_BRUTEFORCE_JOB_RUNNING &&
        !subghz_bruteforce_job_begin(job, RENAULT_V1_BF_CANDIDATES))
        return false;

    return subghz_renault_v1_bf_run_range(job, frame, 0,
                                          RENAULT_V1_BF_CANDIDATES, iv_out);
}

/*============================================================================*/
/* Result helpers                                                              */
/*============================================================================*/

uint32_t subghz_renault_v1_bf_seed_from_iv(const uint8_t iv[4])
{
    return ((uint32_t)iv[0] << 24) | ((uint32_t)iv[1] << 16) |
           ((uint32_t)iv[2] << 8) | (uint32_t)iv[3];
}

uint32_t subghz_renault_v1_bf_counter_from_seed(uint32_t seed)
{
    return ((seed >> 12) & 0xFF0u) | ((seed << 4) & 0xF000u) | (seed >> 28);
}
