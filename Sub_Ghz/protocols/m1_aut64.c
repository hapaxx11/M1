/* See COPYING.txt for license details. */

/*
 * AUT64 decryption, adapted from RocketGod-git/ProtoPirate protocols/aut64.c.
 * The algorithm is described in the USENIX Security 2016 paper by Garcia et al.
 */

#include "m1_aut64.h"

#include <string.h>

static const uint8_t table_ln[12][8] = {
    {4, 5, 6, 7, 0, 1, 2, 3}, {5, 4, 7, 6, 1, 0, 3, 2},
    {6, 7, 4, 5, 2, 3, 0, 1}, {7, 6, 5, 4, 3, 2, 1, 0},
    {0, 1, 2, 3, 4, 5, 6, 7}, {1, 0, 3, 2, 5, 4, 7, 6},
    {2, 3, 0, 1, 6, 7, 4, 5}, {3, 2, 1, 0, 7, 6, 5, 4},
    {5, 4, 7, 6, 1, 0, 3, 2}, {4, 5, 6, 7, 0, 1, 2, 3},
    {7, 6, 5, 4, 3, 2, 1, 0}, {6, 7, 4, 5, 2, 3, 0, 1},
};

static const uint8_t table_un[12][8] = {
    {1, 0, 3, 2, 5, 4, 7, 6}, {0, 1, 2, 3, 4, 5, 6, 7},
    {3, 2, 1, 0, 7, 6, 5, 4}, {2, 3, 0, 1, 6, 7, 4, 5},
    {5, 4, 7, 6, 1, 0, 3, 2}, {4, 5, 6, 7, 0, 1, 2, 3},
    {7, 6, 5, 4, 3, 2, 1, 0}, {6, 7, 4, 5, 2, 3, 0, 1},
    {3, 2, 1, 0, 7, 6, 5, 4}, {2, 3, 0, 1, 6, 7, 4, 5},
    {1, 0, 3, 2, 5, 4, 7, 6}, {0, 1, 2, 3, 4, 5, 6, 7},
};

static uint8_t gf16_mul(uint8_t a, uint8_t b)
{
    uint8_t result = 0;
    for (uint8_t bit = 0; bit < 4; bit++) {
        if (b & 1u)
            result ^= a;
        b >>= 1;
        a = (uint8_t)(((a << 1) ^ ((a & 8u) ? 3u : 0u)) & 0x0Fu);
    }
    return result;
}

static uint8_t gf16_inverse(uint8_t value)
{
    if (value == 0u)
        return 0u;
    for (uint8_t candidate = 1; candidate < 16; candidate++) {
        if (gf16_mul(value, candidate) == 1u)
            return candidate;
    }
    return 0u;
}

bool m1_aut64_unpack(m1_aut64_key_t *key, const uint8_t packed[16])
{
    if (key == NULL || packed == NULL)
        return false;

    key->index = packed[0];
    for (uint8_t i = 0; i < 4; i++) {
        key->key[i * 2u] = packed[i + 1u] >> 4;
        key->key[i * 2u + 1u] = packed[i + 1u] & 0x0Fu;
    }

    uint32_t pbox = ((uint32_t)packed[5] << 16) |
                    ((uint32_t)packed[6] << 8) | packed[7];
    for (int8_t i = 7; i >= 0; i--) {
        key->pbox[i] = (uint8_t)(pbox & 7u);
        pbox >>= 3;
    }

    for (uint8_t i = 0; i < 8; i++) {
        key->sbox[i * 2u] = packed[i + 8u] >> 4;
        key->sbox[i * 2u + 1u] = packed[i + 8u] & 0x0Fu;
    }

    uint8_t seen_pbox = 0;
    uint16_t seen_sbox = 0;
    for (uint8_t i = 0; i < 8; i++) {
        if ((seen_pbox & (1u << key->pbox[i])) != 0u)
            return false;
        seen_pbox |= (uint8_t)(1u << key->pbox[i]);
    }
    for (uint8_t i = 0; i < 16; i++) {
        if ((seen_sbox & (1u << key->sbox[i])) != 0u)
            return false;
        seen_sbox |= (uint16_t)(1u << key->sbox[i]);
    }
    return true;
}

static uint8_t round_key(const m1_aut64_key_t *key, const uint8_t *state,
                         uint8_t round)
{
    uint8_t high = 0, low = 0;
    for (uint8_t i = 0; i < 7; i++) {
        high ^= gf16_mul(key->key[table_un[round][i]], state[i] >> 4);
        low ^= gf16_mul(key->key[table_ln[round][i]], state[i] & 0x0Fu);
    }
    return (uint8_t)((high << 4) | low);
}

static uint8_t substitute(const m1_aut64_key_t *key, uint8_t value)
{
    return (uint8_t)((key->sbox[value >> 4] << 4) | key->sbox[value & 0x0Fu]);
}

static uint8_t permute_bits(const m1_aut64_key_t *key, uint8_t value)
{
    uint8_t result = 0;
    for (uint8_t i = 0; i < 8; i++) {
        if (value & (1u << i))
            result |= (uint8_t)(1u << key->pbox[i]);
    }
    return result;
}

static void permute_bytes(const m1_aut64_key_t *key, uint8_t state[8])
{
    uint8_t result[8];
    for (uint8_t i = 0; i < 8; i++)
        result[key->pbox[i]] = state[i];
    memcpy(state, result, sizeof(result));
}

static uint8_t decrypt_compress(const m1_aut64_key_t *key,
                                const uint8_t state[8], uint8_t round)
{
    const uint8_t rk = round_key(key, state, round);
    const uint8_t inverse_key = gf16_inverse(key->key[table_un[round][7]]);
    const uint8_t inverse_key_low = gf16_inverse(key->key[table_ln[round][7]]);
    const uint8_t high = gf16_mul(inverse_key,
                                  (uint8_t)((state[7] >> 4) ^ (rk >> 4)));
    const uint8_t low = gf16_mul(inverse_key_low,
                                 (uint8_t)((state[7] & 0x0Fu) ^ (rk & 0x0Fu)));
    return (uint8_t)((high << 4) | low);
}

void m1_aut64_decrypt(const m1_aut64_key_t *key, uint8_t block[8])
{
    if (key == NULL || block == NULL)
        return;

    for (int8_t round = 11; round >= 0; round--) {
        block[7] = substitute(key, block[7]);
        block[7] = permute_bits(key, block[7]);
        block[7] = substitute(key, block[7]);
        block[7] = decrypt_compress(key, block, (uint8_t)round);
        permute_bytes(key, block);
    }
}
