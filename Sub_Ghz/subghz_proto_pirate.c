/* See COPYING.txt for license details. */

/*
 * subghz_proto_pirate.c
 *
 * Proto Pirate automotive keyfob encoder dispatcher.
 *
 * Implements pure-logic waveform generation for the Tier-A protocols from
 * RocketGod-git/ProtoPirate.  Outputs are SubGhzRawPair[] timing pairs so the
 * existing M1 KEY→RAW transmitter pipeline can replay them unchanged.
 *
 * This module is hardware-independent and host-testable.
 *
 * Portions of the packet construction are derived from ProtoPirate source
 * (GPLv3): https://github.com/RocketGod-git/ProtoPirate
 * Original files: protocols/kia_v7.c, protocols/mazda_v0.c,
 *                 protocols/honda_static.c, protocols/kia_v0.c, etc.
 *
 * M1 Project — Hapax fork
 */

#include "subghz_proto_pirate.h"
#include "subghz_protocol_registry.h"   /* for subghz_ascii_strcasecmp */
#include "subghz_manchester_encoder.h"
#include <string.h>

/*============================================================================*/
/* Catalog (.rodata)                                                           */
/*============================================================================*/

const SubGhzProtoPirateDef subghz_proto_pirate_catalog[] = {
    [SubGhzProtoPirate_FordV0]      = { SubGhzProtoPirate_FordV0,      "Ford V0",       250,  500, 64, false },
    [SubGhzProtoPirate_MazdaV0]     = { SubGhzProtoPirate_MazdaV0,     "Mazda V0",      250,  500, 64, true  },
    [SubGhzProtoPirate_HondaStatic] = { SubGhzProtoPirate_HondaStatic, "Honda Static",   63,   63, 64, true  },
    [SubGhzProtoPirate_KiaV0]       = { SubGhzProtoPirate_KiaV0,       "Kia V0",        250,  500, 61, false },
    [SubGhzProtoPirate_KiaV1]       = { SubGhzProtoPirate_KiaV1,       "Kia V1",        800, 1600, 57, false },
    [SubGhzProtoPirate_KiaV2]       = { SubGhzProtoPirate_KiaV2,       "Kia V2",        500, 1000, 53, false },
    [SubGhzProtoPirate_KiaV7]       = { SubGhzProtoPirate_KiaV7,       "Kia V7",        250,  500, 64, true  },
    [SubGhzProtoPirate_RenaultV0]   = { SubGhzProtoPirate_RenaultV0,   "Renault V0",    250,  500, 64, false },
    [SubGhzProtoPirate_ChryslerV0]  = { SubGhzProtoPirate_ChryslerV0,  "Chrysler V0",   270,  540, 64, false },
    [SubGhzProtoPirate_FiatV0]      = { SubGhzProtoPirate_FiatV0,      "Fiat V0",       250,  500, 64, false },
    [SubGhzProtoPirate_Subaru]      = { SubGhzProtoPirate_Subaru,      "Subaru",        620, 1620, 64, false },

    /* --- Tier B --- */
    [SubGhzProtoPirate_HondaV1]     = { SubGhzProtoPirate_HondaV1,     "Honda V1",     1000, 2000, 68, false },
    [SubGhzProtoPirate_HondaV2]     = { SubGhzProtoPirate_HondaV2,     "Honda V2",      250,  500, 81, true  },
    [SubGhzProtoPirate_FordV1]      = { SubGhzProtoPirate_FordV1,      "Ford V1",        65,  130, 136, true  },
    [SubGhzProtoPirate_FordV2]      = { SubGhzProtoPirate_FordV2,      "Ford V2",       200,  400, 104, true  },
    [SubGhzProtoPirate_KiaV3]       = { SubGhzProtoPirate_KiaV3,       "Kia V3/V4",     400,  800, 68, false },
    [SubGhzProtoPirate_KiaV4]       = { SubGhzProtoPirate_KiaV4,       "Kia V4",        400,  800, 68, false },
    [SubGhzProtoPirate_KiaV5]       = { SubGhzProtoPirate_KiaV5,       "Kia V5",        400,  800, 67, true  },
    [SubGhzProtoPirate_FiatV1]      = { SubGhzProtoPirate_FiatV1,      "Fiat V1",       250,  500, 104, false },
};

const uint8_t subghz_proto_pirate_catalog_count =
    (uint8_t)(sizeof(subghz_proto_pirate_catalog) / sizeof(subghz_proto_pirate_catalog[0]));

/*============================================================================*/
/* Helpers                                                                     */
/*============================================================================*/

static void pair(SubGhzRawPair *out, uint32_t *idx, uint32_t max,
                 uint32_t high_us, uint32_t low_us)
{
    if (*idx < max) {
        out[*idx].high_us = high_us;
        out[*idx].low_us  = low_us;
        (*idx)++;
    }
}

/*============================================================================*/
/* Kia V7 — true Manchester, 64-bit, CRC-8 poly 0x7F init 0x4C                */
/*============================================================================*/

#define KIA_V7_PREAMBLE_PAIRS 319
#define KIA_V7_TAIL_GAP_US    2000
#define KIA_V7_DEFAULT_HEADER 0xB3   /* raw stored byte[0]; on-air complement is 0x4C */

static uint32_t encode_kia_v7(uint64_t key, SubGhzRawPair *out,
                               uint32_t max_pairs, uint8_t reps)
{
    /* Count the exact number of Manchester symbols this key will produce.
     * The encoder emits one symbol per consumed bit; same-value pairs cost
     * one extra symbol.  We do a dry run first so we can size the buffer. */
    ManchesterEncoderState count_state;
    manchester_encoder_reset(&count_state);
    uint32_t manch_symbols = 0;
    for (int bit = 63; bit >= 0; bit--) {
        bool bit_val = (key >> bit) & 1ULL;
        ManchesterEncoderResult result;
        manchester_encoder_advance(&count_state, bit_val, &result);
        manch_symbols++;
        /* Same-value pairs need a follow-up call with the same bit. */
        if (bit > 0) {
            bool next_bit = (key >> (bit - 1)) & 1ULL;
            if (next_bit == bit_val) {
                manchester_encoder_advance(&count_state, bit_val, &result);
                manch_symbols++;
            }
        }
    }
    manchester_encoder_finish(&count_state);
    manch_symbols++;

    /* Two passes per repetition, each:
     *   319 preamble pairs + 1 sync-high + manch_symbols data pairs +
     *   1 trailing marker + 1 gap pair */
    const uint32_t pairs_per_pass = KIA_V7_PREAMBLE_PAIRS + 1 + manch_symbols + 1 + 1;
    const uint32_t total = pairs_per_pass * 2 * reps;
    if (total > max_pairs)
        return 0;

    uint32_t idx = 0;
    for (uint8_t r = 0; r < reps; r++) {
        for (uint8_t pass = 0; pass < 2; pass++) {
            for (int i = 0; i < KIA_V7_PREAMBLE_PAIRS; i++) {
                pair(out, &idx, max_pairs, 250, 250);
            }
            pair(out, &idx, max_pairs, 250, 250); /* extra sync-high */

            ManchesterEncoderState state;
            manchester_encoder_reset(&state);
            for (int bit = 63; bit >= 0; bit--) {
                bool bit_val = (key >> bit) & 1ULL;
                ManchesterEncoderResult result;
                while (!manchester_encoder_advance(&state, bit_val, &result)) {
                    bool level = (result == ManchesterEncoderResultLongHigh) ||
                                 (result == ManchesterEncoderResultShortHigh);
                    bool is_long = (result == ManchesterEncoderResultLongLow) ||
                                   (result == ManchesterEncoderResultLongHigh);
                    pair(out, &idx, max_pairs,
                         is_long ? 500 : 250,
                         level   ? 0 : (is_long ? 500 : 250));
                }
                bool level = (result == ManchesterEncoderResultLongHigh) ||
                             (result == ManchesterEncoderResultShortHigh);
                bool is_long = (result == ManchesterEncoderResultLongLow) ||
                               (result == ManchesterEncoderResultLongHigh);
                pair(out, &idx, max_pairs,
                     is_long ? 500 : 250,
                     level   ? 0 : (is_long ? 500 : 250));
            }
            ManchesterEncoderResult fin = manchester_encoder_finish(&state);
            bool level = (fin == ManchesterEncoderResultLongHigh) ||
                         (fin == ManchesterEncoderResultShortHigh);
            bool is_long = (fin == ManchesterEncoderResultLongLow) ||
                           (fin == ManchesterEncoderResultLongHigh);
            pair(out, &idx, max_pairs,
                 is_long ? 500 : 250,
                 level   ? 0 : (is_long ? 500 : 250));

            pair(out, &idx, max_pairs, 250, KIA_V7_TAIL_GAP_US);
        }
    }
    return idx;
}

static uint64_t kia_v7_build_key(uint32_t serial, uint8_t button, uint16_t counter,
                                  uint8_t fixed_header)
{
    uint8_t bytes[8];
    bytes[0] = fixed_header;
    bytes[1] = (uint8_t)(counter >> 8);
    bytes[2] = (uint8_t)(counter & 0xFF);
    bytes[3] = (uint8_t)((serial >> 20) & 0xFF);
    bytes[4] = (uint8_t)((serial >> 12) & 0xFF);
    bytes[5] = (uint8_t)((serial >> 4) & 0xFF);
    bytes[6] = (uint8_t)(((serial & 0x0F) << 4) | (button & 0x0F));
    bytes[7] = subghz_protocol_blocks_crc8(bytes, 7, 0x7F, 0x4C);

    uint64_t key = 0;
    for (int i = 0; i < 8; i++) {
        key = (key << 8) | bytes[i];
    }
    /* On-air waveform uses bitwise inverse of the packed key. */
    return ~key;
}

/*============================================================================*/
/* Generic helpers used by multiple protocols                                  */
/*============================================================================*/

static void emit_pwm_bit(SubGhzRawPair *out, uint32_t *idx, uint32_t max,
                         bool bit, uint32_t te_short, uint32_t te_long)
{
    if (bit) {
        pair(out, idx, max, te_short, te_long);
    } else {
        pair(out, idx, max, te_long, te_short);
    }
}

static void emit_pwm_byte(SubGhzRawPair *out, uint32_t *idx, uint32_t max,
                          uint8_t byte, uint32_t te_short, uint32_t te_long)
{
    for (int b = 7; b >= 0; b--) {
        emit_pwm_bit(out, idx, max, (byte >> b) & 1U, te_short, te_long);
    }
}

static uint8_t reverse8_local(uint8_t x)
{
    x = (uint8_t)(((x & 0xF0U) >> 4) | ((x & 0x0FU) << 4));
    x = (uint8_t)(((x & 0xCCU) >> 2) | ((x & 0x33U) << 2));
    x = (uint8_t)(((x & 0xAAU) >> 1) | ((x & 0x55U) << 1));
    return x;
}

/* Standard IEEE 802.4 Manchester using M1's state machine.
 * Emits one SubGhzRawPair per half-bit symbol. */
static void emit_manchester_bits(SubGhzRawPair *out, uint32_t *idx, uint32_t max,
                                 const uint8_t *data, uint32_t bit_count,
                                 uint32_t te_short)
{
    ManchesterEncoderState state;
    manchester_encoder_reset(&state);

    for (int i = (int)bit_count - 1; i >= 0; i--) {
        bool bit = (data[i / 8] >> (7 - (i % 8))) & 1U;
        ManchesterEncoderResult result;
        while (!manchester_encoder_advance(&state, bit, &result)) {
            bool level = (result == ManchesterEncoderResultLongHigh) ||
                         (result == ManchesterEncoderResultShortHigh);
            bool is_long = (result == ManchesterEncoderResultLongLow) ||
                           (result == ManchesterEncoderResultLongHigh);
            pair(out, idx, max,
                 is_long ? te_short * 2 : te_short,
                 level   ? 0 : (is_long ? te_short * 2 : te_short));
        }
        bool level = (result == ManchesterEncoderResultLongHigh) ||
                     (result == ManchesterEncoderResultShortHigh);
        bool is_long = (result == ManchesterEncoderResultLongLow) ||
                       (result == ManchesterEncoderResultLongHigh);
        pair(out, idx, max,
             is_long ? te_short * 2 : te_short,
             level   ? 0 : (is_long ? te_short * 2 : te_short));
    }

    ManchesterEncoderResult fin = manchester_encoder_finish(&state);
    bool level = (fin == ManchesterEncoderResultLongHigh) ||
                 (fin == ManchesterEncoderResultShortHigh);
    bool is_long = (fin == ManchesterEncoderResultLongLow) ||
                   (fin == ManchesterEncoderResultLongHigh);
    pair(out, idx, max,
         is_long ? te_short * 2 : te_short,
         level   ? 0 : (is_long ? te_short * 2 : te_short));
}

/* CRC-16/CCITT-FALSE (poly 0x1021, init 0x0000) used by Ford V1. */
static uint16_t crc16_ccitt(const uint8_t *data, size_t len)
{
    return subghz_protocol_blocks_crc16(data, len, 0x1021, 0x0000);
}

/*============================================================================*/
/* Honda V1 — OOK PWM, 68-bit, custom nibble checksum                          */
/*============================================================================*/

#define HONDA_V1_TE_SHORT      1000
#define HONDA_V1_TE_LONG       2000
#define HONDA_V1_PREAMBLE_PAIRS 180
#define HONDA_V1_FRAME_GAP_US   5000
#define HONDA_V1_FRAME_BYTES    9

static uint8_t honda_v1_crc_fold(uint16_t v)
{
    const uint8_t lo = (uint8_t)(v & 0x0FU);
    const uint16_t hi = (uint16_t)(v >> 4U);
    int32_t s = (hi & 1U) ? (int32_t)lo : -(int32_t)lo;
    uint8_t out = (uint8_t)((s - (int32_t)hi) & 7);
    out |= (uint8_t)(((v >> 3U) & 1U) << 3U);
    if ((((v >> 1U) & 1U) != 0) && ((((v >> 4U) ^ (v >> 5U)) & 1U) != 0)) {
        out ^= 0x04U;
    }
    return (uint8_t)(out & 0x0FU);
}

static uint8_t honda_v1_checksum_base(uint64_t data)
{
    const uint8_t a = honda_v1_crc_fold((uint16_t)(data & 0xFFFFU));
    const uint8_t b = honda_v1_crc_fold((uint8_t)((data >> 40U) & 0xFFU));
    return (uint8_t)((a ^ b ^ 1U) & 0x0FU);
}

static uint8_t honda_v1_checksum_alternate(uint8_t checksum)
{
    uint8_t mask = 0x09U;
    if ((checksum & 1U) == 0U) {
        mask = (checksum & 2U) ? 0x0BU : 0x0FU;
    }
    return (uint8_t)((checksum ^ mask) & 0x0FU);
}

static void honda_v1_checksum_wire_order(uint64_t data,
                                          uint8_t *first,
                                          uint8_t *second)
{
    const uint8_t checksum = honda_v1_checksum_base(data);
    const uint8_t other = honda_v1_checksum_alternate(checksum);
    if ((checksum & 0x08U) != 0U) {
        *first = other;
        *second = checksum;
    } else {
        *first = checksum;
        *second = other;
    }
}

static uint32_t encode_honda_v1(uint64_t key, SubGhzRawPair *out,
                                 uint32_t max_pairs, uint8_t reps)
{
    /* Honda V1 encoder builds two frames: first with checksum_first,
     * second with checksum_second, each repeated twice. */
    const uint32_t pairs_per_frame = HONDA_V1_PREAMBLE_PAIRS + 1 + (HONDA_V1_FRAME_BYTES * 8) + 3;
    const uint32_t total = (pairs_per_frame * 4 + 1) * reps;
    if (total > max_pairs)
        return 0;

    uint8_t frame[HONDA_V1_FRAME_BYTES] = {0};
    for (int i = 0; i < 8; i++) {
        frame[i] = (uint8_t)(key >> (56 - i * 8));
    }

    uint8_t first = 0, second = 0;
    honda_v1_checksum_wire_order(key, &first, &second);

    uint32_t idx = 0;
    for (uint8_t r = 0; r < reps; r++) {
        for (uint8_t crc_idx = 0; crc_idx < 2; crc_idx++) {
            frame[HONDA_V1_FRAME_BYTES - 1] = (uint8_t)((crc_idx == 0 ? first : second) << 4U);
            for (uint8_t repeat = 0; repeat < 2; repeat++) {
                for (uint32_t p = 0; p < HONDA_V1_PREAMBLE_PAIRS; p++) {
                    pair(out, &idx, max_pairs, HONDA_V1_TE_SHORT, HONDA_V1_TE_SHORT);
                }
                /* Honda V1 preamble ends with a long LOW gap before data */
                pair(out, &idx, max_pairs, HONDA_V1_TE_SHORT, HONDA_V1_FRAME_GAP_US);

                /* Data: alternating preamble start bits then frame bytes */
                for (uint32_t bit_index = 0; bit_index < 12; bit_index++) {
                    bool bit = ((uint32_t)~bit_index) & 0x01U;
                    emit_pwm_bit(out, &idx, max_pairs, bit,
                                 HONDA_V1_TE_SHORT, HONDA_V1_TE_LONG);
                }
                for (uint32_t bit_index = 12; bit_index < 12 + (HONDA_V1_FRAME_BYTES * 8); bit_index++) {
                    const uint32_t data_index = (bit_index - 12) >> 3U;
                    const uint8_t shift = (uint8_t)((11U - bit_index) & 0x07U);
                    bool bit = (frame[data_index] >> shift) & 0x01U;
                    emit_pwm_bit(out, &idx, max_pairs, bit,
                                 HONDA_V1_TE_SHORT, HONDA_V1_TE_LONG);
                }
                /* Tail: short pulse matching last level then gap */
                pair(out, &idx, max_pairs, HONDA_V1_TE_SHORT, HONDA_V1_TE_SHORT);
                pair(out, &idx, max_pairs, 0, HONDA_V1_FRAME_GAP_US);
            }
        }
    }
    return idx;
}

static uint32_t encode_honda_v1_wrapper(const SubGhzKeyParams *params,
                                         SubGhzRawPair *out,
                                         uint32_t max_pairs,
                                         uint8_t reps)
{
    (void)params;
    /* key_value holds the 64-bit Honda V1 key; the low nibble of byte 8 is
     * replaced by the computed checksum when encoding. */
    return encode_honda_v1(params->key_value, out, max_pairs, reps);
}

/*============================================================================*/
/* Honda V2 — differential Manchester/biphase, 81-bit                          */
/*============================================================================*/

#define HONDA_V2_TE_SHORT      250
#define HONDA_V2_SYNC_US       750
#define HONDA_V2_GAP_US        50000
#define HONDA_V2_PREAMBLE_PAIRS 319

static uint8_t honda_v2_calculate_check(uint32_t count)
{
    const uint8_t c0 = (uint8_t)(((count >> 1) ^ (count >> 2) ^ (count >> 3) ^
                                   (count >> 4) ^ (count >> 6)) & 1U);
    const uint8_t c1 = (uint8_t)(((count >> 0) ^ (count >> 2) ^ (count >> 3) ^
                                   (count >> 4) ^ (count >> 5) ^ (count >> 6) ^ 1U) & 1U);
    const uint8_t c2 = (uint8_t)(((count >> 1) ^ (count >> 3) ^ (count >> 4) ^
                                   (count >> 5) ^ (count >> 6)) & 1U);
    return (uint8_t)(c0 | (c1 << 1) | (c2 << 2));
}

static bool honda_v2_calculate_tail_msb(uint32_t count)
{
    return (((count >> 0) ^ (count >> 2) ^ (count >> 4) ^ (count >> 5)) & 1U) != 0U;
}

static uint16_t honda_v2_calculate_tail(uint32_t count)
{
    return honda_v2_calculate_tail_msb(count) ? 0xFFFFU : 0x7FFFU;
}

static uint64_t honda_v2_build_key(uint32_t signature, uint32_t serial, uint32_t count)
{
    uint8_t bytes[8] = {0};
    bytes[0] = (uint8_t)((signature >> 16) & 0xFFU);
    bytes[1] = (uint8_t)((signature >> 8) & 0xFFU);
    bytes[2] = (uint8_t)(signature & 0xFFU);
    bytes[3] = (uint8_t)((serial >> 16) & 0xFFU);
    bytes[4] = (uint8_t)((serial >> 8) & 0xFFU);
    bytes[5] = (uint8_t)(serial & 0xFFU);
    bytes[6] = (uint8_t)((count >> 1) & 0xFFU);

    const bool counter_lsb = (count & 1U) != 0;
    const uint8_t check = honda_v2_calculate_check(count);
    bytes[7] = (counter_lsb ? 0x80U : 0x00U) | check;

    uint64_t key = 0;
    for (int i = 0; i < 8; i++) {
        key = (key << 8) | bytes[i];
    }
    return key;
}

static bool honda_v2_add_level(SubGhzRawPair *out, uint32_t *idx, uint32_t max,
                               bool level, uint32_t duration)
{
    if (*idx >= max)
        return false;
    out[*idx].high_us = level ? duration : 0;
    out[*idx].low_us  = level ? 0 : duration;
    (*idx)++;
    return true;
}

static bool honda_v2_add_bit(SubGhzRawPair *out, uint32_t *idx, uint32_t max,
                             bool *previous_bit, bool bit,
                             uint32_t te_short, uint32_t te_long)
{
    if (!*previous_bit && !bit) {
        if (!honda_v2_add_level(out, idx, max, true, te_short) ||
            !honda_v2_add_level(out, idx, max, false, te_short))
            return false;
    } else if (!*previous_bit && bit) {
        if (!honda_v2_add_level(out, idx, max, true, te_long))
            return false;
    } else if (*previous_bit && !bit) {
        if (!honda_v2_add_level(out, idx, max, false, te_long))
            return false;
    } else {
        if (!honda_v2_add_level(out, idx, max, false, te_short) ||
            !honda_v2_add_level(out, idx, max, true, te_short))
            return false;
    }
    *previous_bit = bit;
    return true;
}

static uint32_t encode_honda_v2(uint64_t key, uint32_t count,
                                 SubGhzRawPair *out,
                                 uint32_t max_pairs, uint8_t reps)
{
    uint8_t key_bytes[8];
    for (int i = 0; i < 8; i++) {
        key_bytes[i] = (uint8_t)(key >> (56 - i * 8));
    }

    const uint32_t pairs_per_rep =
        HONDA_V2_PREAMBLE_PAIRS * 2 + 3 + /* sync */
        (1 + 62 + 16 + 1) * 2 + /* bits encoded as transitions, worst case */
        1; /* gap */
    const uint32_t total = pairs_per_rep * reps;
    if (total > max_pairs)
        return 0;

    uint32_t idx = 0;
    for (uint8_t r = 0; r < reps; r++) {
        for (uint32_t p = 0; p < HONDA_V2_PREAMBLE_PAIRS; p++) {
            pair(out, &idx, max_pairs, HONDA_V2_TE_SHORT, HONDA_V2_TE_SHORT);
        }
        /* Sync: 750us HIGH, 750us LOW, 250us HIGH */
        if (!honda_v2_add_level(out, &idx, max_pairs, true, HONDA_V2_SYNC_US) ||
            !honda_v2_add_level(out, &idx, max_pairs, false, HONDA_V2_SYNC_US) ||
            !honda_v2_add_level(out, &idx, max_pairs, true, HONDA_V2_TE_SHORT))
            return 0;

        bool previous_bit = true;
        if (!honda_v2_add_bit(out, &idx, max_pairs, &previous_bit, false,
                              HONDA_V2_TE_SHORT, HONDA_V2_TE_SHORT * 2))
            return 0;

        for (uint8_t bit_index = 2; bit_index < 64; bit_index++) {
            const uint8_t byte_index = bit_index / 8U;
            const uint8_t bit_in_byte = 7U - (bit_index % 8U);
            bool bit = (key_bytes[byte_index] >> bit_in_byte) & 1U;
            if (!honda_v2_add_bit(out, &idx, max_pairs, &previous_bit, bit,
                                  HONDA_V2_TE_SHORT, HONDA_V2_TE_SHORT * 2))
                return 0;
        }

        const uint16_t tail = honda_v2_calculate_tail(count);
        for (uint8_t bit_index = 0; bit_index < 16; bit_index++) {
            bool bit = (tail >> (15U - bit_index)) & 1U;
            if (!honda_v2_add_bit(out, &idx, max_pairs, &previous_bit, bit,
                                  HONDA_V2_TE_SHORT, HONDA_V2_TE_SHORT * 2))
                return 0;
        }

        if (!honda_v2_add_bit(out, &idx, max_pairs, &previous_bit, true,
                              HONDA_V2_TE_SHORT, HONDA_V2_TE_SHORT * 2))
            return 0;
        if (!honda_v2_add_level(out, &idx, max_pairs, false, HONDA_V2_GAP_US))
            return 0;
    }
    return idx;
}

static uint32_t encode_honda_v2_wrapper(const SubGhzKeyParams *params,
                                         SubGhzRawPair *out,
                                         uint32_t max_pairs,
                                         uint8_t reps)
{
    uint32_t serial  = params->serial ? params->serial : (uint32_t)((params->key_value >> 24) & 0xFFFFFFU);
    uint32_t button  = params->btn ? params->btn : (uint32_t)(params->key_value & 0xFFU);
    uint32_t count   = params->cnt ? params->cnt : (uint32_t)((params->key_value >> 16) & 0x1FFU);
    uint32_t signature = (uint32_t)((params->key_value >> 40) & 0xFFFFFFU);

    if (signature == 0) {
        /* Default signatures from ProtoPirate */
        if (button == 0x02) signature = 0xC20363UL;      /* Lock */
        else if (button == 0x04) signature = 0xA285E3UL; /* Unlock */
    }

    uint64_t key = honda_v2_build_key(signature, serial, count);
    return encode_honda_v2(key, count, out, max_pairs, reps);
}

/*============================================================================*/
/* Ford V2 — Manchester, 104-bit, sync word 0x7FA7, 6 bursts                   */
/*============================================================================*/

#define FORD_V2_TE_SHORT       200
#define FORD_V2_TE_LONG        400
#define FORD_V2_PREAMBLE_PAIRS 70
#define FORD_V2_SYNC_LO_US     476
#define FORD_V2_BURST_COUNT    6
#define FORD_V2_INTER_BURST_US 16000
#define FORD_V2_DATA_BYTES     13
#define FORD_V2_DATA_BITS      104

static uint8_t ford_v2_uint8_parity(uint8_t value)
{
    uint8_t parity = 0U;
    while (value) {
        parity ^= (value & 1U);
        value >>= 1U;
    }
    return parity;
}

static uint32_t encode_ford_v2_burst(const uint8_t raw_bytes[FORD_V2_DATA_BYTES],
                                      SubGhzRawPair *out,
                                      uint32_t *idx, uint32_t max)
{
    const uint32_t te_short = FORD_V2_TE_SHORT;
    const uint32_t te_long  = FORD_V2_TE_LONG;

    for (uint32_t i = 0; i < FORD_V2_PREAMBLE_PAIRS; i++) {
        pair(out, idx, max, 0, te_short);
        pair(out, idx, max, te_short, 0);
    }

    /* Sync */
    pair(out, idx, max, 0, FORD_V2_SYNC_LO_US);
    pair(out, idx, max, te_short, 0);

    /* Data: skip first bit (sync word MSB), encode bits 1..103 */
    ManchesterEncoderState state;
    manchester_encoder_reset(&state);
    for (uint16_t bit_pos = 1U; bit_pos < FORD_V2_DATA_BITS; bit_pos++) {
        const uint8_t byte_idx = (uint8_t)(bit_pos / 8U);
        const uint8_t bit_idx = (uint8_t)(7U - (bit_pos % 8U));
        bool bit = ((raw_bytes[byte_idx] >> bit_idx) & 1U) != 0U;
        ManchesterEncoderResult result;
        while (!manchester_encoder_advance(&state, bit, &result)) {
            bool level = (result == ManchesterEncoderResultLongHigh) ||
                         (result == ManchesterEncoderResultShortHigh);
            bool is_long = (result == ManchesterEncoderResultLongLow) ||
                           (result == ManchesterEncoderResultLongHigh);
            pair(out, idx, max,
                 level ? (is_long ? te_long : te_short) : 0,
                 level ? 0 : (is_long ? te_long : te_short));
        }
        bool level = (result == ManchesterEncoderResultLongHigh) ||
                     (result == ManchesterEncoderResultShortHigh);
        bool is_long = (result == ManchesterEncoderResultLongLow) ||
                       (result == ManchesterEncoderResultLongHigh);
        pair(out, idx, max,
             level ? (is_long ? te_long : te_short) : 0,
             level ? 0 : (is_long ? te_long : te_short));
    }
    ManchesterEncoderResult fin = manchester_encoder_finish(&state);
    bool level = (fin == ManchesterEncoderResultLongHigh) ||
                 (fin == ManchesterEncoderResultShortHigh);
    bool is_long = (fin == ManchesterEncoderResultLongLow) ||
                   (fin == ManchesterEncoderResultLongHigh);
    pair(out, idx, max,
         level ? (is_long ? te_long : te_short) : 0,
         level ? 0 : (is_long ? te_long : te_short));

    return *idx;
}

static uint32_t encode_ford_v2(const uint8_t raw_bytes[FORD_V2_DATA_BYTES],
                                SubGhzRawPair *out,
                                uint32_t max_pairs, uint8_t reps)
{
    /* Approx pairs per burst. Exact depends on Manchester run-lengths. */
    const uint32_t burst_pairs =
        (FORD_V2_PREAMBLE_PAIRS * 2) + 2 + ((FORD_V2_DATA_BITS - 1) * 2) + 2;
    const uint32_t total = (burst_pairs * FORD_V2_BURST_COUNT + (FORD_V2_BURST_COUNT - 1)) * reps;
    if (total > max_pairs)
        return 0;

    uint32_t idx = 0;
    for (uint8_t r = 0; r < reps; r++) {
        for (uint8_t burst = 0; burst < FORD_V2_BURST_COUNT; burst++) {
            encode_ford_v2_burst(raw_bytes, out, &idx, max_pairs);
            if (burst + 1U < FORD_V2_BURST_COUNT) {
                pair(out, &idx, max_pairs, FORD_V2_INTER_BURST_US, 0);
            }
        }
    }
    return idx;
}

static uint32_t encode_ford_v2_wrapper(const SubGhzKeyParams *params,
                                        SubGhzRawPair *out,
                                        uint32_t max_pairs,
                                        uint8_t reps)
{
    /* key_value = 64-bit key (bytes 0..7); extra[0..4] = tail bytes 8..12.
     * bytes[0]=0x7F, bytes[1]=0xA7 sync word. */
    uint8_t raw[FORD_V2_DATA_BYTES] = {0};
    for (int i = 0; i < 8 && i < (int)sizeof(params->key_value); i++) {
        raw[i] = (uint8_t)(params->key_value >> (56 - i * 8));
    }
    /* Default sync word */
    if (raw[0] == 0 && raw[1] == 0) {
        raw[0] = 0x7FU;
        raw[1] = 0xA7U;
    }
    for (int i = 0; i < 5; i++) {
        raw[8 + i] = params->extra[i];
    }

    /* Set parity of button byte into byte[7] MSB */
    raw[7] = (raw[7] & 0x7FU) | (uint8_t)(ford_v2_uint8_parity(raw[6]) << 7);

    return encode_ford_v2(raw, out, max_pairs, reps);
}

/*============================================================================*/
/* Dispatcher                                                                  */
/*============================================================================*/

SubGhzProtoPirateId subghz_proto_pirate_find_by_name(const char *name)
{
    if (!name) return SubGhzProtoPirate_Unknown;
    for (uint8_t i = 0; i < subghz_proto_pirate_catalog_count; i++) {
        if (subghz_ascii_strcasecmp(subghz_proto_pirate_catalog[i].name, name) == 0)
            return subghz_proto_pirate_catalog[i].id;
    }
    return SubGhzProtoPirate_Unknown;
}

bool subghz_proto_pirate_is_supported(const char *name)
{
    return subghz_proto_pirate_find_by_name(name) != SubGhzProtoPirate_Unknown;
}

static uint32_t encode_unsupported(const SubGhzKeyParams *params,
                                    SubGhzRawPair *out, uint32_t max_pairs,
                                    uint8_t reps)
{
    (void)params; (void)out; (void)max_pairs; (void)reps;
    return 0;
}

typedef uint32_t (*ProtoPirateEncodeFn)(const SubGhzKeyParams *params,
                                         SubGhzRawPair *out,
                                         uint32_t max_pairs,
                                         uint8_t reps);

static uint32_t encode_kia_v7_wrapper(const SubGhzKeyParams *params,
                                       SubGhzRawPair *out,
                                       uint32_t max_pairs,
                                       uint8_t reps)
{
    /* For now use default header and pack fields from the lower 64 bits.
     * The Flipper Key value is treated as the raw 64-bit key; future UI
     * scenes can construct it from serial/button/counter fields. */
    uint32_t serial  = (uint32_t)((params->key_value >> 12) & 0x0FFFFFFF);
    uint8_t  button  = (uint8_t)(params->key_value & 0x0F);
    uint16_t counter = (uint16_t)((params->key_value >> 16) & 0xFFFF);
    uint8_t  header  = (uint8_t)((params->key_value >> 56) & 0xFF);
    if (header == 0) header = KIA_V7_DEFAULT_HEADER;

    uint64_t key = kia_v7_build_key(serial, button, counter, header);
    return encode_kia_v7(key, out, max_pairs, reps);
}

static const ProtoPirateEncodeFn encode_table[SubGhzProtoPirate_Count] = {
    [SubGhzProtoPirate_FordV0]      = encode_unsupported,
    [SubGhzProtoPirate_MazdaV0]     = encode_unsupported,
    [SubGhzProtoPirate_HondaStatic] = encode_unsupported,
    [SubGhzProtoPirate_KiaV0]       = encode_unsupported,
    [SubGhzProtoPirate_KiaV1]       = encode_unsupported,
    [SubGhzProtoPirate_KiaV2]       = encode_unsupported,
    [SubGhzProtoPirate_KiaV7]       = encode_kia_v7_wrapper,
    [SubGhzProtoPirate_RenaultV0]   = encode_unsupported,
    [SubGhzProtoPirate_ChryslerV0]  = encode_unsupported,
    [SubGhzProtoPirate_FiatV0]      = encode_unsupported,
    [SubGhzProtoPirate_Subaru]      = encode_unsupported,

    /* Tier B */
    [SubGhzProtoPirate_HondaV1]     = encode_honda_v1_wrapper,
    [SubGhzProtoPirate_HondaV2]     = encode_honda_v2_wrapper,
    [SubGhzProtoPirate_FordV2]      = encode_ford_v2_wrapper,
    [SubGhzProtoPirate_FordV1]      = encode_unsupported,
    [SubGhzProtoPirate_KiaV3]       = encode_unsupported,
    [SubGhzProtoPirate_KiaV4]       = encode_unsupported,
    [SubGhzProtoPirate_KiaV5]       = encode_unsupported,
    [SubGhzProtoPirate_FiatV1]      = encode_unsupported,
};

static uint32_t required_pairs_for_id(SubGhzProtoPirateId id, uint8_t reps)
{
    switch (id) {
    case SubGhzProtoPirate_KiaV7:
        return (KIA_V7_PREAMBLE_PAIRS + 1 + (64 * 2) + 1 + 1) * 2 * reps;
    case SubGhzProtoPirate_HondaV1:
        /* Per frame: preamble + gap + 12 start bits + 72 data bits + tail + gap.
         * Two CRC variants, each repeated twice -> 4 frames per repetition. */
        return ((HONDA_V1_PREAMBLE_PAIRS + 1 + 12 + (HONDA_V1_FRAME_BYTES * 8) + 1 + 1) * 4) * reps;
    case SubGhzProtoPirate_HondaV2:
        /* Preamble + sync + 81 bits worst-case as transitions + gap. */
        return (HONDA_V2_PREAMBLE_PAIRS + 3 +
                (81 * 2) + 1) * reps;
    case SubGhzProtoPirate_FordV2:
        return (((FORD_V2_PREAMBLE_PAIRS * 2) + 2 + ((FORD_V2_DATA_BITS - 1) * 2) + 2) *
                FORD_V2_BURST_COUNT + (FORD_V2_BURST_COUNT - 1)) * reps;
    default:
        return 0;
    }
}

uint32_t subghz_proto_pirate_encode(const SubGhzKeyParams *params,
                                     SubGhzRawPair         *out,
                                     uint32_t               max_pairs,
                                     uint8_t                repetitions)
{
    if (!params || !out || max_pairs == 0 || repetitions == 0)
        return 0;

    SubGhzProtoPirateId id = subghz_proto_pirate_find_by_name(params->protocol);
    if (id < 0 || (size_t)id >= SubGhzProtoPirate_Count)
        return 0;

    ProtoPirateEncodeFn fn = encode_table[id];
    if (!fn)
        return 0;

    return fn(params, out, max_pairs, repetitions);
}

uint32_t subghz_proto_pirate_required_pairs(const SubGhzKeyParams *params,
                                             uint8_t                repetitions)
{
    if (!params || repetitions == 0)
        return 0;

    SubGhzProtoPirateId id = subghz_proto_pirate_find_by_name(params->protocol);
    if (id < 0 || (size_t)id >= SubGhzProtoPirate_Count)
        return 0;

    return required_pairs_for_id(id, repetitions);
}
