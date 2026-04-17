/* See COPYING.txt for license details. */

/*
 * subghz_secplus_v1_encoder.c
 *
 * Security+ 1.0 ternary OOK packet encoder.
 *
 * This is the inverse of m1_chamberlain_decode.c: given a known fixed code
 * and rolling counter, it produces the two-sub-packet ternary OOK signal
 * that a Security+ 1.0 remote would transmit.
 *
 * Algorithm: argilo/secplus (MIT licence)
 *   https://github.com/argilo/secplus
 * Implementation adapted from Flipper Zero firmware (GPLv3):
 *   lib/subghz/protocols/secplus_v1.c
 *
 * Symbol encoding (OOK, each symbol = LOW + HIGH pair):
 *   Symbol 0: LOW 1500 µs + HIGH  500 µs   (3:1)
 *   Symbol 1: LOW 1000 µs + HIGH 1000 µs   (2:2)
 *   Symbol 2: LOW  500 µs + HIGH 1500 µs   (1:3)
 *
 * Sub-packet 1 header: Symbol 0  (PKT1_HEADER)
 * Sub-packet 2 header: Symbol 2  (PKT2_HEADER)
 * Data: 20 symbols per sub-packet.
 *
 * Encoding loop (inverse of decode, from Flipper secplus_v1.c):
 *   Working from LSBs of ternary-encoded rolling + fixed:
 *   Packet 1: pairs at odd indices 1,3,5,…,19 in data_array
 *     data_array[i]   = rolling_digit (mod 3)
 *     data_array[i+1] = (fixed_digit + acc) % 3
 *   Packet 2: pairs at even indices 0,2,4,…,18 in data_array
 *     same pattern
 *
 * M1 Project — Hapax fork
 */

#include <string.h>
#include "subghz_secplus_v1_encoder.h"
#include "../../m1_csrc/bit_util.h"

/* ── Symbol timing helpers ──────────────────────────────────────────────── */

/*
 * Fill one symbol slot.
 * sym: 0 → (1500 LOW, 500 HIGH), 1 → (1000, 1000), 2 → (500, 1500)
 */
static void fill_symbol(SubGhzSecPlusV1Symbol *s, uint8_t sym)
{
    switch (sym)
    {
        case 0:
            s->low_us  = SUBGHZ_SECPLUS_V1_TE_LONG;
            s->high_us = SUBGHZ_SECPLUS_V1_TE_SHORT;
            break;
        case 1:
            s->low_us  = SUBGHZ_SECPLUS_V1_TE_MID;
            s->high_us = SUBGHZ_SECPLUS_V1_TE_MID;
            break;
        case 2:
        default:
            s->low_us  = SUBGHZ_SECPLUS_V1_TE_SHORT;
            s->high_us = SUBGHZ_SECPLUS_V1_TE_LONG;
            break;
    }
}

/* ── Ternary decomposition ──────────────────────────────────────────────── */

/*
 * Decompose a 20-trit ternary number into an array of trits.
 * trits[0] is the MOST significant trit (for MSB-first encoding).
 * total: the numeric value (must be < 3^20)
 * trits: output array of 20 uint8_t values (0, 1, or 2)
 */
static void decompose_trits(uint32_t total, uint8_t trits[20])
{
    for (int8_t i = 19; i >= 0; i--)
    {
        trits[i] = (uint8_t)(total % 3u);
        total /= 3u;
    }
}

/* ── Public encoder ─────────────────────────────────────────────────────── */

bool subghz_secplus_v1_encode(uint32_t fixed, uint32_t rolling,
                               SubGhzSecPlusV1Packet *out)
{
    if (!out)
        return false;

    /*
     * Security+ 1.0 ternary encoding:
     *
     * The 40-bit "code" = (fixed << 20) | rolling (both as ternary values).
     * fixed  = 20-trit ternary number (fits in 32 bits: 3^20 = 3486784401)
     * rolling = 20-trit ternary number (fits in 32 bits: same bound)
     *
     * Each is decomposed into 20 ternary digits.
     *
     * Encoding (inverse of Flipper decode):
     *   data_array layout:
     *     [0]       = PKT1_HEADER (sym 0)
     *     [1..20]   = 20 data symbols for sub-packet 1
     *     [21]      = PKT2_HEADER (sym 2)
     *     [22..41]  = 20 data symbols for sub-packet 2
     *
     *   Packet 1 data (odd+even pairs at [1..20]):
     *     For i = 1,3,5,…,19 (using trits indexed i//2 from rolling):
     *       data_array[i]   = rolling_trit[k]    (k = (i-1)/2, from trit[0])
     *       acc            += rolling_trit[k]
     *       data_array[i+1] = fixed_trit[k]      (raw trit from fixed)
     *       acc            += data_array[i+1]    (but data_array[i+1] is raw fixed trit!)
     *
     * Wait — let me re-read the Flipper decode more carefully to get the
     * exact inverse.
     *
     * From m1_chamberlain_decode.c reconstruct_payload():
     *
     *   Packet 1: for i = 1, 3, 5, …, 19 (step 2):
     *     rolling = rolling * 3 + da[i]         → rolling_trit = da[i]
     *     acc += da[i]
     *     fixed_digit = (60 + da[i+1] - acc) % 3  → da[i+1] = (fixed_digit + acc) % 3
     *     fixed  = fixed * 3 + fixed_digit
     *     acc   += fixed_digit
     *
     *   Packet 2: acc = 0; for i = 22, 24, 26, …, 40 (step 2):
     *     rolling = rolling * 3 + da[i]         → rolling_trit = da[i]
     *     acc += da[i]
     *     fixed_digit = (60 + da[i+1] - acc) % 3  → da[i+1] = (fixed_digit + acc) % 3
     *     fixed  = fixed * 3 + fixed_digit
     *     acc   += fixed_digit
     *
     * So to encode, we reverse this:
     *   given rolling (MSB first = trit[0..19]) and fixed (MSB first = trit[0..19]):
     *
     *   Packet 1: acc = 0; for k = 0..9:
     *     i = 1 + k*2
     *     da[i]   = rolling_trit[k]
     *     acc    += rolling_trit[k]
     *     da[i+1] = (fixed_trit[k] + acc) % 3     ← encode fixed using acc
     *     acc    += fixed_trit[k]
     *
     *   Packet 2: acc = 0; for k = 10..19:
     *     i = 22 + (k-10)*2
     *     da[i]   = rolling_trit[k]
     *     acc    += rolling_trit[k]
     *     da[i+1] = (fixed_trit[k] + acc) % 3
     *     acc    += fixed_trit[k]
     *
     * Then:
     *   sub-pkt 1 data: da[1..20] → 20 data symbols
     *   sub-pkt 2 data: da[22..41] → 20 data symbols
     */

    /* Decompose fixed and rolling into 20 ternary digits each, MSB first */
    uint8_t rolling_trits[20];
    uint8_t fixed_trits[20];
    decompose_trits(rolling, rolling_trits);
    decompose_trits(fixed,   fixed_trits);

    /* Build data_array[42] */
    uint8_t da[42];
    memset(da, 0, sizeof(da));
    da[0]  = 0; /* PKT1_HEADER = Symbol 0 */
    da[21] = 2; /* PKT2_HEADER = Symbol 2 */

    uint32_t acc;

    /* Packet 1: pairs at positions [1,2], [3,4], … [19,20] → 10 rolling+fixed pairs */
    acc = 0;
    for (uint8_t k = 0; k < 10; k++)
    {
        uint8_t idx = (uint8_t)(1u + k * 2u);
        da[idx]     = rolling_trits[k];
        acc        += rolling_trits[k];
        da[idx + 1] = (uint8_t)((fixed_trits[k] + acc) % 3u);
        acc        += fixed_trits[k];
    }

    /* Packet 2: pairs at positions [22,23], [24,25], … [40,41] → 10 rolling+fixed pairs */
    acc = 0;
    for (uint8_t k = 0; k < 10; k++)
    {
        uint8_t idx = (uint8_t)(22u + k * 2u);
        da[idx]     = rolling_trits[10u + k];
        acc        += rolling_trits[10u + k];
        da[idx + 1] = (uint8_t)((fixed_trits[10u + k] + acc) % 3u);
        acc        += fixed_trits[10u + k];
    }

    /* Convert data_array symbols to timing pairs */
    for (uint8_t i = 0; i < 42u; i++)
    {
        fill_symbol(&out->symbols[i], da[i]);
    }

    return true;
}
