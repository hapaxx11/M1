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
#include "subghz_keeloq.h"
#include "subghz_blocks_math.h"
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
    [SubGhzProtoPirate_StarLine]    = { SubGhzProtoPirate_StarLine,    "Star Line",     250,  500, 64, false },

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

static bool pair_bool(SubGhzRawPair *out, uint32_t *idx, uint32_t max,
                      bool level, uint32_t duration)
{
    if (*idx >= max) return false;
    out[*idx].high_us = level ? duration : 0;
    out[*idx].low_us  = level ? 0 : duration;
    (*idx)++;
    return true;
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

static uint8_t reverse8_local(uint8_t x)
{
    x = (uint8_t)(((x & 0xF0U) >> 4) | ((x & 0x0FU) << 4));
    x = (uint8_t)(((x & 0xCCU) >> 2) | ((x & 0x33U) << 2));
    x = (uint8_t)(((x & 0xAAU) >> 1) | ((x & 0x55U) << 1));
    return x;
}

/*============================================================================*/
/* Kia V0 — Kia/Suzuki/Honda sub-formats                                       */
/*============================================================================*/

#define KIA_V0_TE_SHORT            250
#define KIA_V0_TE_LONG             500
#define KIA_V0_TYPE1_SYNC          750
#define KIA_V0_TYPE1_PREAMBLE_PAIRS 319
#define KIA_V0_TYPE2_PREAMBLE_PAIRS 320
#define KIA_V0_TAIL_PREAMBLE_PAIRS  15
#define KIA_V0_KIA_GAP             1000
#define KIA_V0_SUZUKI_GAP          2000

#define kia_v0_crc8_poly(data, len) subghz_protocol_blocks_crc8((data), (len), 0x7F, 0x00)

static const uint8_t kia_v0_honda_crc_table[16] = {
    0x4A, 0x25, 0x96, 0x4B, 0xA1, 0xD4, 0x6A, 0x35,
    0x9E, 0x4F, 0xA3, 0xD5, 0xEE, 0x77, 0xBF, 0xDB,
};

static uint8_t kia_v0_honda_header(uint8_t button)
{
    const uint8_t button_3bit = button & 0x07U;
    const uint8_t base = (button_3bit == 0x07U) ? 0x1AU : 0x0AU;
    return (uint8_t)(((button_3bit << 5U) & 0xE0U) | base);
}

static uint8_t kia_v0_honda_fold_counter(uint16_t counter)
{
    uint8_t value = 0;
    for (size_t i = 0; i < 16; i++) {
        if ((counter >> i) & 1U) value ^= kia_v0_honda_crc_table[i];
    }
    return value;
}

static uint8_t kia_v0_honda_crc(uint8_t header, uint16_t counter)
{
    uint8_t value = kia_v0_honda_fold_counter(counter);
    switch (header) {
    case 0xAA: value ^= 0xA5; break;
    case 0x2A: value ^= 0x21; break;
    case 0x6A: value ^= 0x15; break;
    case 0xFA: value ^= 0x73; break;
    default:   value ^= 0xC6; break;
    }
    return value;
}

static uint64_t kia_v0_build_honda_key(uint32_t serial, uint8_t button, uint16_t counter)
{
    const uint8_t header = kia_v0_honda_header(button);
    const uint8_t crc = kia_v0_honda_crc(header, counter);
    const uint8_t bytes[8] = {
        0xF0,
        reverse8_local((uint8_t)(counter >> 8U)),
        reverse8_local((uint8_t)counter),
        (uint8_t)(serial >> 16U),
        (uint8_t)(serial >> 8U),
        (uint8_t)serial,
        header,
        crc,
    };
    uint64_t key = 0;
    for (int i = 0; i < 8; i++) key = (key << 8) | bytes[i];
    return key;
}

static uint8_t kia_v0_family_crc(uint16_t counter, uint32_t serial, uint8_t button)
{
    const uint8_t bytes[6] = {
        (uint8_t)(counter >> 8U),
        (uint8_t)counter,
        (uint8_t)(serial >> 20U),
        (uint8_t)(serial >> 12U),
        (uint8_t)(serial >> 4U),
        (uint8_t)(((serial & 0x0FU) << 4U) | (button & 0x0FU)),
    };
    uint8_t crc = 0;
    for (size_t i = 0; i < sizeof(bytes); i++) crc ^= bytes[i];
    return crc;
}

static uint64_t kia_v0_build_kia_raw(uint32_t serial, uint8_t button, uint16_t counter)
{
    const uint32_t high = 0x0F000000UL | (((uint32_t)counter & 0xFFFFUL) << 8U) |
                          ((serial >> 20U) & 0xFFUL);
    const uint32_t low = (((uint32_t)serial & 0x000FFFFFUL) << 12U) |
                         (((uint32_t)button & 0x0FUL) << 8U);
    const uint64_t partial = ((uint64_t)high << 32U) | low;
    const uint8_t crc = kia_v0_family_crc(counter, serial, button);
    return partial | crc;
}

static uint8_t kia_v0_suzuki_crc8_from_fields(uint32_t serial, uint8_t button, uint32_t counter)
{
    const uint16_t cnt_u16 = (uint16_t)(counter & 0xFFFFU);
    const uint16_t c_sw = (uint16_t)((cnt_u16 << 8) | (cnt_u16 >> 8));
    const uint8_t buf[6] = {
        (uint8_t)(c_sw & 0xFFU),
        (uint8_t)((c_sw >> 8) & 0xFFU),
        (uint8_t)((serial >> 20) & 0xFFU),
        (uint8_t)((serial >> 12) & 0xFFU),
        (uint8_t)((serial >> 4) & 0xFFU),
        (uint8_t)((button & 0x0FU) | ((uint8_t)((uint32_t)serial << 4))),
    };
    return kia_v0_crc8_poly(buf, 6);
}

static uint64_t kia_v0_suzuki_shifted_key_from_fields(
    uint32_t serial, uint8_t button, uint32_t counter, uint8_t crc_byte)
{
    const uint32_t r8 = ((uint32_t)serial << 16) | ((uint32_t)crc_byte << 4) |
                        (((uint32_t)button & 0x0FU) << 12);
    const uint32_t r5 =
        (uint32_t)(((serial >> 16) & 0xFFFU) | (((uint32_t)(counter & 0xFFFFU)) << 12)) |
        0xF0000000U;
    return ((uint64_t)r5 << 32) | r8;
}

static uint64_t kia_v0_honda_transform(uint64_t data)
{
    uint8_t bytes[8];
    for (int i = 0; i < 8; i++) bytes[i] = (uint8_t)(data >> (56 - i * 8));
    for (size_t i = 0; i < 8; i++) bytes[i] = reverse8_local(bytes[i]);
    uint64_t key = 0;
    for (int i = 0; i < 8; i++) key = (key << 8) | bytes[i];
    return key;
}

static void kia_v0_append_short_pairs(SubGhzRawPair *out, uint32_t *idx, uint32_t max,
                                      uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) {
        pair(out, idx, max, KIA_V0_TE_SHORT, 0);
        pair(out, idx, max, 0, KIA_V0_TE_SHORT);
    }
}

static void kia_v0_append_data_pairs(SubGhzRawPair *out, uint32_t *idx, uint32_t max,
                                     uint64_t raw, uint8_t bits)
{
    for (int i = (int)bits - 1; i >= 0; i--) {
        bool bit = ((raw >> i) & 1ULL) != 0ULL;
        if (bit) {
            pair(out, idx, max, KIA_V0_TE_SHORT, 0);
            pair(out, idx, max, 0, KIA_V0_TE_LONG);
        } else {
            pair(out, idx, max, KIA_V0_TE_LONG, 0);
            pair(out, idx, max, 0, KIA_V0_TE_SHORT);
        }
    }
}

static uint32_t encode_kia_v0(const SubGhzKeyParams *params,
                               SubGhzRawPair *out,
                               uint32_t max_pairs,
                               uint8_t reps)
{
    uint32_t serial = params->serial ? params->serial : (uint32_t)((params->key_value >> 32) & 0x0FFFFFFFU);
    uint8_t  btn    = params->btn ? (uint8_t)params->btn : (uint8_t)((params->key_value >> 60) & 0x0FU);
    uint16_t cnt    = (uint16_t)(params->cnt ? params->cnt : (params->key_value & 0xFFFFU));

    /* Default to the Kia subtype. */
    uint8_t type = (params->extra[0] > 0 && params->extra[0] <= 3) ? params->extra[0] : 1;

    uint64_t raw;
    uint8_t bits;
    uint32_t burst_pairs;
    if (type == 3) { /* Honda V0 */
        raw  = kia_v0_build_honda_key(serial, btn & 0x07U, cnt);
        bits = 72;
        burst_pairs = 40 * 2 + 4 * 2 + bits * 2 + 1;
    } else if (type == 2) { /* Suzuki V0 */
        const uint8_t crc = kia_v0_suzuki_crc8_from_fields(serial, btn, cnt);
        raw = kia_v0_suzuki_shifted_key_from_fields(serial, btn, cnt, crc);
        bits = 64;
        burst_pairs = KIA_V0_TYPE2_PREAMBLE_PAIRS * 2 + bits * 2 + 3 +
                      KIA_V0_TAIL_PREAMBLE_PAIRS * 2 + bits * 2;
    } else { /* Kia */
        raw = kia_v0_build_kia_raw(serial, btn, cnt);
        bits = 61;
        burst_pairs = 2 + KIA_V0_TYPE1_PREAMBLE_PAIRS * 2 + bits * 2 + 2 +
                      KIA_V0_TAIL_PREAMBLE_PAIRS * 2 + bits * 2 + 1;
    }

    const uint32_t total = burst_pairs * reps;
    if (total > max_pairs) return 0;

    uint32_t idx = 0;
    for (uint8_t r = 0; r < reps; r++) {
        if (type == 3) {
            kia_v0_append_short_pairs(out, &idx, max_pairs, 40);
            for (size_t p = 0; p < 4; p++) {
                pair(out, &idx, max_pairs, KIA_V0_TE_LONG, 0);
                pair(out, &idx, max_pairs, 0, KIA_V0_TE_LONG);
            }
            kia_v0_append_data_pairs(out, &idx, max_pairs, kia_v0_honda_transform(raw), 56);
            pair(out, &idx, max_pairs, KIA_V0_TE_SHORT, 0);
            pair(out, &idx, max_pairs, 0, KIA_V0_KIA_GAP);
        } else if (type == 2) {
            kia_v0_append_short_pairs(out, &idx, max_pairs, KIA_V0_TYPE2_PREAMBLE_PAIRS);
            kia_v0_append_data_pairs(out, &idx, max_pairs, raw, bits);
            pair(out, &idx, max_pairs, 0, KIA_V0_SUZUKI_GAP);
            pair(out, &idx, max_pairs, KIA_V0_SUZUKI_GAP, 0);
            pair(out, &idx, max_pairs, 0, KIA_V0_TE_SHORT);
            kia_v0_append_short_pairs(out, &idx, max_pairs, KIA_V0_TAIL_PREAMBLE_PAIRS);
            kia_v0_append_data_pairs(out, &idx, max_pairs, raw, bits);
        } else {
            pair(out, &idx, max_pairs, KIA_V0_TYPE1_SYNC, 0);
            pair(out, &idx, max_pairs, 0, KIA_V0_TYPE1_SYNC);
            kia_v0_append_short_pairs(out, &idx, max_pairs, KIA_V0_TYPE1_PREAMBLE_PAIRS);
            kia_v0_append_data_pairs(out, &idx, max_pairs, raw, bits);
            pair(out, &idx, max_pairs, KIA_V0_TE_SHORT, 0);
            pair(out, &idx, max_pairs, 0, 1500);
            kia_v0_append_short_pairs(out, &idx, max_pairs, KIA_V0_TAIL_PREAMBLE_PAIRS);
            kia_v0_append_data_pairs(out, &idx, max_pairs, raw, bits);
            pair(out, &idx, max_pairs, KIA_V0_TE_SHORT, 0);
            pair(out, &idx, max_pairs, 0, KIA_V0_KIA_GAP);
        }
    }
    return idx;
}

/*============================================================================*/
/* Kia V1 — OOK PWM, 57-bit, 3 bursts                                          */
/*============================================================================*/

#define KIA_V1_TE_SHORT            800
#define KIA_V1_TE_LONG             1600
#define KIA_V1_HEADER_PULSES       90
#define KIA_V1_TOTAL_BURSTS        3
#define KIA_V1_INTER_BURST_GAP_US  25000

static uint8_t kia_v1_crc4(const uint8_t *bytes, int count, uint8_t offset)
{
    uint8_t crc = 0;
    for (int i = 0; i < count; i++) {
        uint8_t b = bytes[i];
        crc ^= ((b & 0x0F) ^ (b >> 4));
    }
    return (uint8_t)((crc + offset) & 0x0F);
}

static uint32_t encode_kia_v1(const SubGhzKeyParams *params,
                               SubGhzRawPair *out,
                               uint32_t max_pairs,
                               uint8_t reps)
{
    uint32_t serial = params->serial ? params->serial : (uint32_t)((params->key_value >> 24) & 0xFFFFFFFFU);
    uint8_t  btn    = params->btn ? (uint8_t)params->btn : (uint8_t)((params->key_value >> 16) & 0xFFU);
    uint16_t cnt    = (uint16_t)(params->cnt ? params->cnt : (params->key_value & 0xFFFFU));

    uint8_t cnt_high = (cnt >> 8) & 0x0F;
    uint8_t char_data[7];
    char_data[0] = (uint8_t)(serial >> 24);
    char_data[1] = (uint8_t)(serial >> 16);
    char_data[2] = (uint8_t)(serial >> 8);
    char_data[3] = (uint8_t)serial;
    char_data[4] = btn;
    char_data[5] = (uint8_t)(cnt & 0xFF);
    char_data[6] = cnt_high;
    uint8_t crc = kia_v1_crc4(char_data, 7, 1);

    uint64_t data = ((uint64_t)serial << 24) | ((uint64_t)btn << 16) |
                    ((uint64_t)(cnt & 0xFF) << 8) |
                    ((uint64_t)cnt_high << 4) | crc;

    const uint32_t burst_pairs = (KIA_V1_HEADER_PULSES * 2) + 1 + ((57U - 1) * 2);
    const uint32_t total = burst_pairs * KIA_V1_TOTAL_BURSTS * reps + (KIA_V1_TOTAL_BURSTS - 1) * reps;
    if (total > max_pairs) return 0;

    uint32_t idx = 0;
    for (uint8_t r = 0; r < reps; r++) {
        for (uint8_t burst = 0; burst < KIA_V1_TOTAL_BURSTS; burst++) {
            if (burst > 0) {
                pair(out, &idx, max_pairs, 0, KIA_V1_INTER_BURST_GAP_US);
            }
            for (int i = 0; i < KIA_V1_HEADER_PULSES; i++) {
                pair(out, &idx, max_pairs, 0, KIA_V1_TE_LONG);
                pair(out, &idx, max_pairs, KIA_V1_TE_LONG, 0);
            }
            pair(out, &idx, max_pairs, 0, KIA_V1_TE_SHORT);
            for (uint8_t i = 57; i > 1; i--) {
                bool bit = ((data >> (i - 2)) & 1ULL) != 0ULL;
                if (bit) {
                    pair(out, &idx, max_pairs, KIA_V1_TE_SHORT, 0);
                    pair(out, &idx, max_pairs, 0, KIA_V1_TE_SHORT);
                } else {
                    pair(out, &idx, max_pairs, 0, KIA_V1_TE_SHORT);
                    pair(out, &idx, max_pairs, KIA_V1_TE_SHORT, 0);
                }
            }
        }
    }
    return idx;
}

/*============================================================================*/
/* Kia V2 — OOK PWM, 53-bit, 2 bursts                                          */
/*============================================================================*/

#define KIA_V2_TE_SHORT            500
#define KIA_V2_TE_LONG             1000
#define KIA_V2_HEADER_PAIRS        252
#define KIA_V2_TOTAL_BURSTS        2

static uint8_t kia_v2_calculate_crc(uint64_t data)
{
    uint64_t data_without_crc = data >> 4;
    uint8_t bytes[6];
    for (int i = 0; i < 6; i++) bytes[i] = (uint8_t)((data_without_crc >> (i * 8)) & 0xFFU);
    uint8_t crc = 0;
    for (int i = 0; i < 6; i++) crc ^= (bytes[i] & 0x0F) ^ (bytes[i] >> 4);
    return (uint8_t)((crc + 1) & 0x0F);
}

static uint32_t encode_kia_v2(const SubGhzKeyParams *params,
                               SubGhzRawPair *out,
                               uint32_t max_pairs,
                               uint8_t reps)
{
    uint32_t serial = params->serial ? params->serial : (uint32_t)((params->key_value >> 20) & 0xFFFFFFFFU);
    uint8_t  btn    = params->btn ? (uint8_t)params->btn : (uint8_t)((params->key_value >> 16) & 0x0FU);
    uint16_t cnt    = (uint16_t)(params->cnt ? params->cnt : (params->key_value & 0xFFFFU));

    uint32_t uVar6 = ((uint32_t)(cnt & 0xFF) << 8) |
                     ((uint32_t)(btn & 0x0F) << 16) |
                     ((uint32_t)(cnt >> 4) & 0xF0);

    uint64_t data = (1ULL << 52) |
                    (((uint64_t)serial << 20) & 0xFFFFFFFFF00000ULL) |
                    (uint64_t)uVar6;
    data = (data & ~0x0FULL) | kia_v2_calculate_crc(data);

    const uint32_t burst_pairs = (KIA_V2_HEADER_PAIRS * 2) + 1 + ((53U - 1) * 2);
    const uint32_t total = burst_pairs * KIA_V2_TOTAL_BURSTS * reps;
    if (total > max_pairs) return 0;

    uint32_t idx = 0;
    for (uint8_t r = 0; r < reps; r++) {
        for (uint8_t burst = 0; burst < KIA_V2_TOTAL_BURSTS; burst++) {
            for (int i = 0; i < KIA_V2_HEADER_PAIRS; i++) {
                pair(out, &idx, max_pairs, 0, KIA_V2_TE_LONG);
                pair(out, &idx, max_pairs, KIA_V2_TE_LONG, 0);
            }
            pair(out, &idx, max_pairs, 0, KIA_V2_TE_SHORT);
            for (uint8_t i = 53; i > 1; i--) {
                bool bit = ((data >> (i - 2)) & 1ULL) != 0ULL;
                if (bit) {
                    pair(out, &idx, max_pairs, KIA_V2_TE_SHORT, 0);
                    pair(out, &idx, max_pairs, 0, KIA_V2_TE_SHORT);
                } else {
                    pair(out, &idx, max_pairs, 0, KIA_V2_TE_SHORT);
                    pair(out, &idx, max_pairs, KIA_V2_TE_SHORT, 0);
                }
            }
        }
    }
    return idx;
}

/*============================================================================*/
/* Subaru — OOK PWM, 64-bit                                                    */
/*============================================================================*/

#define SUBARU_TE_SHORT            800
#define SUBARU_TE_LONG             1600
#define SUBARU_PREAMBLE_PAIRS      75
#define SUBARU_GAP_US              2800
#define SUBARU_SYNC_US             2800

static void subaru_rotate_left_3bytes(uint8_t *b0, uint8_t *b1, uint8_t *b2, uint8_t count)
{
    for (uint8_t i = 0; i < count; i++) {
        uint8_t t = *b0;
        *b0 = (uint8_t)((*b0 << 1) | (*b1 >> 7));
        *b1 = (uint8_t)((*b1 << 1) | (*b2 >> 7));
        *b2 = (uint8_t)((*b2 << 1) | (t >> 7));
    }
}

static void subaru_encode_count(uint8_t *KB, uint16_t count)
{
    uint8_t lo = count & 0xFF;
    uint8_t hi = (count >> 8) & 0xFF;

    KB[4] &= ~0xC0;
    KB[5] &= ~0xC3;
    KB[6] &= ~0x03;

    if ((lo & 0x01) == 0) KB[4] |= 0x40;
    if ((lo & 0x02) == 0) KB[4] |= 0x80;
    if ((lo & 0x04) == 0) KB[5] |= 0x01;
    if ((lo & 0x08) == 0) KB[5] |= 0x02;
    if ((lo & 0x10) == 0) KB[6] |= 0x01;
    if ((lo & 0x20) == 0) KB[6] |= 0x02;
    if ((lo & 0x40) == 0) KB[5] |= 0x40;
    if ((lo & 0x80) == 0) KB[5] |= 0x80;

    uint8_t SER0 = KB[3];
    uint8_t SER1 = KB[1];
    uint8_t SER2 = KB[2];

    uint8_t total_rot = (uint8_t)(4 + lo);
    subaru_rotate_left_3bytes(&SER0, &SER1, &SER2, total_rot);

    const uint8_t rel = (uint8_t)(SER0 ^ KB[0]);
    KB[4] = (uint8_t)((KB[4] & 0xC0) | ((rel >> 2) & 0x3F));
    KB[5] = (uint8_t)((KB[5] & 0xCF) | ((rel << 4) & 0x30));

    uint8_t T1 = 0xFF;
    uint8_t T2 = 0xFF;

    if (hi & 0x04) T1 &= ~0x10;
    if (hi & 0x08) T1 &= ~0x20;
    if (hi & 0x02) T2 &= ~0x80;
    if (hi & 0x01) T2 &= ~0x40;
    if (hi & 0x40) T1 &= ~0x01;
    if (hi & 0x80) T1 &= ~0x02;
    if (hi & 0x20) T2 &= ~0x08;
    if (hi & 0x10) T2 &= ~0x04;

    uint8_t new_REG_SH1 = T1 ^ SER1;
    uint8_t new_REG_SH2 = T2 ^ SER2;

    KB[5] &= ~0x0C;
    KB[6] &= ~0xC0;

    KB[7] = (uint8_t)((KB[7] & 0xF0) | ((new_REG_SH1 >> 4) & 0x0F));

    if (new_REG_SH1 & 0x04) KB[5] |= 0x04;
    if (new_REG_SH1 & 0x08) KB[5] |= 0x08;
    if (new_REG_SH1 & 0x02) KB[6] |= 0x80;
    if (new_REG_SH1 & 0x01) KB[6] |= 0x40;

    KB[6] = (uint8_t)((KB[6] & 0xC3) | ((new_REG_SH2 >> 2) & 0x3C));
    KB[7] = (uint8_t)((KB[7] & 0x0F) | ((new_REG_SH2 << 4) & 0xF0));
}

static uint64_t subaru_build_key(uint32_t serial, uint8_t button, uint16_t count)
{
    uint8_t b[8];
    b[0] = button & 0x0F;
    b[1] = (uint8_t)(serial >> 16);
    b[2] = (uint8_t)(serial >> 8);
    b[3] = (uint8_t)serial;
    b[4] = 0;
    b[5] = 0;
    b[6] = 0;
    b[7] = 0;
    subaru_encode_count(b, count);
    uint64_t key = 0;
    for (int i = 0; i < 8; i++) key = (key << 8) | b[i];
    return key;
}

static uint32_t encode_subaru(const SubGhzKeyParams *params,
                               SubGhzRawPair *out,
                               uint32_t max_pairs,
                               uint8_t reps)
{
    uint32_t serial = params->serial ? params->serial : (uint32_t)((params->key_value >> 40) & 0xFFFFFFU);
    uint8_t  btn    = params->btn ? (uint8_t)params->btn : (uint8_t)((params->key_value >> 60) & 0x0FU);
    uint16_t cnt    = (uint16_t)(params->cnt ? params->cnt : (params->key_value & 0xFFFFU));

    uint64_t key = subaru_build_key(serial, btn, cnt);

    const uint32_t burst_pairs = (SUBARU_PREAMBLE_PAIRS * 2) + 2 + 64 * 2 + 2;
    const uint32_t total = burst_pairs * reps;
    if (total > max_pairs) return 0;

    uint32_t idx = 0;
    for (uint8_t r = 0; r < reps; r++) {
        for (int i = 0; i < SUBARU_PREAMBLE_PAIRS; i++) {
            pair(out, &idx, max_pairs, 0, SUBARU_TE_LONG);
            pair(out, &idx, max_pairs, SUBARU_TE_LONG, 0);
        }
        pair(out, &idx, max_pairs, 0, SUBARU_GAP_US);
        pair(out, &idx, max_pairs, SUBARU_SYNC_US, 0);

        for (int i = 63; i >= 0; i--) {
            bool bit = ((key >> i) & 1ULL) != 0ULL;
            if (bit) {
                pair(out, &idx, max_pairs, 0, SUBARU_TE_LONG);
                pair(out, &idx, max_pairs, SUBARU_TE_SHORT, 0);
            } else {
                pair(out, &idx, max_pairs, 0, SUBARU_TE_SHORT);
                pair(out, &idx, max_pairs, SUBARU_TE_LONG, 0);
            }
        }
        pair(out, &idx, max_pairs, 0, SUBARU_GAP_US);
        pair(out, &idx, max_pairs, SUBARU_SYNC_US, 0);
    }
    return idx;
}

/*============================================================================*/
/* Ford V0 — differential Manchester, 80-bit, 6 bursts                         */
/*============================================================================*/

#define FORD_V0_TE_SHORT           250
#define FORD_V0_TE_LONG            500
#define FORD_V0_PREAMBLE_PAIRS     4
#define FORD_V0_GAP_US             3500
#define FORD_V0_TOTAL_BURSTS       6
#define FORD_V0_INTER_BURST_US     50000

static const uint8_t ford_v0_crc_matrix[64] = {
    0xDA, 0xB5, 0x55, 0x6A, 0xAA, 0xAA, 0xAA, 0xD5,
    0xB6, 0x6C, 0xCC, 0xD9, 0x99, 0x99, 0x99, 0xB3,
    0x71, 0xE3, 0xC3, 0xC7, 0x87, 0x87, 0x87, 0x8F,
    0x0F, 0xE0, 0x3F, 0xC0, 0x7F, 0x80, 0x7F, 0x80,
    0x00, 0x1F, 0xFF, 0xC0, 0x00, 0x7F, 0xFF, 0x80,
    0x00, 0x00, 0x00, 0x3F, 0xFF, 0xFF, 0xFF, 0x80,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7F,
    0x23, 0x12, 0x94, 0x84, 0x35, 0xF4, 0x55, 0x84,
};

static uint8_t ford_v0_calculate_checksum(uint32_t serial, uint32_t count, uint8_t button)
{
    return (uint8_t)((((count >> 24) & 0xFF) + ((count >> 16) & 0xFF) +
                      ((count >> 8) & 0xFF) + (count & 0xFF) +
                      ((serial >> 24) & 0xFF) + ((serial >> 16) & 0xFF) +
                      ((serial >> 8) & 0xFF) + (serial & 0xFF) + (button << 3)) & 0xFF);
}

static uint8_t ford_v0_calculate_crc(uint8_t *buf)
{
    uint8_t crc = 0;
    for (int row = 0; row < 8; row++) {
        uint8_t xor_sum = 0;
        for (int col = 0; col < 8; col++) {
            xor_sum ^= (uint8_t)(ford_v0_crc_matrix[row * 8 + col] & buf[col + 1]);
        }
        uint8_t parity = subghz_protocol_blocks_parity8(xor_sum);
        if (parity) crc |= (uint8_t)(1 << row);
    }
    return crc;
}

static void ford_v0_encode_key(uint8_t header_byte, uint32_t serial, uint8_t button,
                                uint32_t count, uint8_t checksum, uint64_t *key1)
{
    uint8_t buf[8] = {0};
    buf[0] = header_byte;
    buf[1] = (uint8_t)((serial >> 24) & 0xFF);
    buf[2] = (uint8_t)((serial >> 16) & 0xFF);
    buf[3] = (uint8_t)((serial >> 8) & 0xFF);
    buf[4] = (uint8_t)(serial & 0xFF);
    buf[5] = (uint8_t)(((button & 0x0F) << 3) | ((count >> 16) & 0x0F));

    uint8_t count_mid = (uint8_t)((count >> 8) & 0xFF);
    uint8_t count_low = (uint8_t)(count & 0xFF);

    uint8_t post_xor_6 = (uint8_t)((count_mid & 0xAA) | (count_low & 0x55));
    uint8_t post_xor_7 = (uint8_t)((count_low & 0xAA) | (count_mid & 0x55));

    uint8_t parity = 0;
    uint8_t tmp = checksum;
    while (tmp) {
        parity ^= (tmp & 1);
        tmp >>= 1;
    }
    bool parity_bit = (checksum != 0) ? (parity != 0) : false;

    if (parity_bit) {
        uint8_t xor_byte = post_xor_7;
        for (int i = 1; i <= 5; i++) buf[i] ^= xor_byte;
        buf[6] = (uint8_t)(post_xor_6 ^ xor_byte);
        buf[7] = post_xor_7;
    } else {
        uint8_t xor_byte = post_xor_6;
        for (int i = 1; i <= 5; i++) buf[i] ^= xor_byte;
        buf[6] = post_xor_6;
        buf[7] = (uint8_t)(post_xor_7 ^ xor_byte);
    }

    *key1 = 0;
    for (int i = 0; i < 8; i++) *key1 = (*key1 << 8) | buf[i];
}

static bool ford_v0_add_level(SubGhzRawPair *out, uint32_t *idx, uint32_t max,
                               bool level, uint32_t duration)
{
    if (*idx >= max) return false;
    out[*idx].high_us = level ? duration : 0;
    out[*idx].low_us  = level ? 0 : duration;
    (*idx)++;
    return true;
}

static bool ford_v0_add_bit(SubGhzRawPair *out, uint32_t *idx, uint32_t max,
                             bool *prev_bit, bool bit)
{
    const uint32_t te_short = FORD_V0_TE_SHORT;
    const uint32_t te_long  = FORD_V0_TE_LONG;
    if (!*prev_bit && !bit) {
        if (!ford_v0_add_level(out, idx, max, true, te_short) ||
            !ford_v0_add_level(out, idx, max, false, te_short)) return false;
    } else if (!*prev_bit && bit) {
        if (!ford_v0_add_level(out, idx, max, true, te_long)) return false;
    } else if (*prev_bit && !bit) {
        if (!ford_v0_add_level(out, idx, max, false, te_long)) return false;
    } else {
        if (!ford_v0_add_level(out, idx, max, false, te_short) ||
            !ford_v0_add_level(out, idx, max, true, te_short)) return false;
    }
    *prev_bit = bit;
    return true;
}

static uint32_t encode_ford_v0(const SubGhzKeyParams *params,
                                SubGhzRawPair *out,
                                uint32_t max_pairs,
                                uint8_t reps)
{
    uint8_t  header = (uint8_t)((params->key_value >> 56) & 0xFF);
    uint32_t serial = params->serial ? params->serial : (uint32_t)(params->key_value >> 32);
    uint8_t  button = params->btn ? (uint8_t)params->btn : (uint8_t)((params->key_value >> 16) & 0xFF);
    uint32_t count  = params->cnt ? params->cnt : (uint32_t)(params->key_value & 0xFFFFFFU);

    uint8_t checksum = ford_v0_calculate_checksum(serial, count, button);
    uint64_t key1;
    ford_v0_encode_key(header, serial, button, count, checksum, &key1);

    uint8_t buf[9] = {0};
    for (int i = 0; i < 8; i++) buf[i] = (uint8_t)(key1 >> (56 - i * 8));
    buf[8] = checksum;
    uint8_t crc = (uint8_t)(ford_v0_calculate_crc(buf) ^ 0x80);
    uint16_t key2 = (uint16_t)((checksum << 8) | crc);

    uint64_t tx_key1 = ~key1;
    uint16_t tx_key2 = ~key2;

    const uint32_t burst_pairs = 2 + (FORD_V0_PREAMBLE_PAIRS * 2) + 2 + 79 * 2 + 16 * 2;
    const uint32_t total = (burst_pairs * FORD_V0_TOTAL_BURSTS + (FORD_V0_TOTAL_BURSTS - 1)) * reps;
    if (total > max_pairs) return 0;

    uint32_t idx = 0;
    for (uint8_t r = 0; r < reps; r++) {
        for (uint8_t burst = 0; burst < FORD_V0_TOTAL_BURSTS; burst++) {
            if (!ford_v0_add_level(out, &idx, max_pairs, true, FORD_V0_TE_SHORT) ||
                !ford_v0_add_level(out, &idx, max_pairs, false, FORD_V0_TE_LONG)) return 0;
            for (int i = 0; i < FORD_V0_PREAMBLE_PAIRS; i++) {
                if (!ford_v0_add_level(out, &idx, max_pairs, true, FORD_V0_TE_LONG) ||
                    !ford_v0_add_level(out, &idx, max_pairs, false, FORD_V0_TE_LONG)) return 0;
            }
            if (!ford_v0_add_level(out, &idx, max_pairs, true, FORD_V0_TE_SHORT) ||
                !ford_v0_add_level(out, &idx, max_pairs, false, FORD_V0_GAP_US)) return 0;

            bool prev = (bool)((tx_key1 >> 62) & 1ULL);
            if (prev) {
                if (!ford_v0_add_level(out, &idx, max_pairs, true, FORD_V0_TE_LONG)) return 0;
            } else {
                if (!ford_v0_add_level(out, &idx, max_pairs, true, FORD_V0_TE_SHORT) ||
                    !ford_v0_add_level(out, &idx, max_pairs, false, FORD_V0_TE_LONG)) return 0;
            }
            for (int bit = 61; bit >= 0; bit--) {
                if (!ford_v0_add_bit(out, &idx, max_pairs, &prev,
                                     ((tx_key1 >> bit) & 1ULL) != 0ULL)) return 0;
            }
            for (int bit = 15; bit >= 0; bit--) {
                if (!ford_v0_add_bit(out, &idx, max_pairs, &prev,
                                     ((tx_key2 >> bit) & 1U) != 0U)) return 0;
            }
            if (burst + 1 < FORD_V0_TOTAL_BURSTS) {
                if (!ford_v0_add_level(out, &idx, max_pairs, false, FORD_V0_INTER_BURST_US)) return 0;
            }
        }
    }
    return idx;
}

/*============================================================================*/
/* Honda Static — FM Manchester-ish, 64-bit                                    */
/*============================================================================*/

#define HONDA_STATIC_SYNC_TIME_US     700
#define HONDA_STATIC_ELEMENT_TIME_US  63
#define HONDA_STATIC_PREAMBLE_COUNT   160
#define HONDA_STATIC_BITS             64

static void honda_static_set_bits(uint8_t *data, uint8_t start, uint8_t count, uint32_t value)
{
    for (uint8_t i = 0; i < count; i++) {
        const uint8_t bit_index = start + i;
        const uint8_t byte_index = bit_index >> 3U;
        const uint8_t shift = ((uint8_t)~bit_index) & 0x07U;
        const uint8_t mask = (uint8_t)(1U << shift);
        const bool bit = ((value >> (count - 1U - i)) & 1U) != 0U;
        if (bit) data[byte_index] |= mask;
        else     data[byte_index] &= (uint8_t)~mask;
    }
}

static void honda_static_build_packet(const SubGhzKeyParams *params, uint8_t packet[8])
{
    memset(packet, 0, 8);
    uint32_t serial = params->serial ? params->serial : (uint32_t)((params->key_value >> 28) & 0x0FFFFFFFU);
    uint8_t  button = params->btn ? (uint8_t)params->btn : (uint8_t)(params->key_value & 0x0FU);
    uint32_t counter = params->cnt ? params->cnt : (uint32_t)((params->key_value >> 4) & 0x00FFFFFFU);

    honda_static_set_bits(packet, 0, 4, button & 0x0FU);
    honda_static_set_bits(packet, 4, 28, serial);
    honda_static_set_bits(packet, 32, 24, counter);

    uint8_t checksum = 0;
    for (size_t i = 0; i < 7; i++) checksum ^= packet[i];
    honda_static_set_bits(packet, 56, 8, checksum);
}

static uint32_t encode_honda_static(const SubGhzKeyParams *params,
                                     SubGhzRawPair *out,
                                     uint32_t max_pairs,
                                     uint8_t reps)
{
    uint8_t packet[8];
    honda_static_build_packet(params, packet);

    const uint32_t pairs_per_rep = 1 + HONDA_STATIC_PREAMBLE_COUNT + (HONDA_STATIC_BITS * 2) + 1;
    const uint32_t total = pairs_per_rep * reps;
    if (total > max_pairs) return 0;

    uint32_t idx = 0;
    for (uint8_t r = 0; r < reps; r++) {
        pair(out, &idx, max_pairs, HONDA_STATIC_SYNC_TIME_US, 0);
        for (size_t i = 0; i < HONDA_STATIC_PREAMBLE_COUNT; i++) {
            bool level = (i & 1U) != 0U;
            pair(out, &idx, max_pairs, level ? HONDA_STATIC_ELEMENT_TIME_US : 0,
                 level ? 0 : HONDA_STATIC_ELEMENT_TIME_US);
        }
        for (uint8_t bit = 0; bit < HONDA_STATIC_BITS; bit++) {
            bool value = ((packet[bit >> 3U] >> (((uint8_t)~bit) & 0x07U)) & 1U) != 0U;
            pair(out, &idx, max_pairs, value ? 0 : HONDA_STATIC_ELEMENT_TIME_US,
                 value ? HONDA_STATIC_ELEMENT_TIME_US : 0);
            pair(out, &idx, max_pairs, value ? HONDA_STATIC_ELEMENT_TIME_US : 0,
                 value ? 0 : HONDA_STATIC_ELEMENT_TIME_US);
        }
        bool last = (packet[7] & 1U) != 0U;
        pair(out, &idx, max_pairs, last ? 0 : HONDA_STATIC_SYNC_TIME_US,
             last ? HONDA_STATIC_SYNC_TIME_US : 0);
    }
    return idx;
}

/*============================================================================*/
/* Mazda V0 — Manchester, 64-bit                                               */
/*============================================================================*/

#define MAZDA_V0_TE_SHORT      250
#define MAZDA_V0_TE_LONG       500
#define MAZDA_V0_GAP_US        0xCB20
#define MAZDA_V0_SYNC_BYTE     0xD7
#define MAZDA_V0_TAIL_BYTE     0x5A
#define MAZDA_V0_PREAMBLE_ONES 16

static uint8_t mazda_v0_calculate_checksum(uint32_t serial, uint8_t button, uint32_t counter)
{
    counter &= 0xFFFFFU;
    return (uint8_t)(((serial >> 24) & 0xFF) + ((serial >> 16) & 0xFF) +
                     ((serial >> 8) & 0xFF) + (serial & 0xFF) +
                     ((counter >> 8) & 0xFF) + (counter & 0xFF) +
                     ((((counter >> 16) & 0x0F) | ((button & 0x0F) << 4)) & 0xFF));
}

static uint64_t mazda_v0_encode_key(uint32_t serial, uint8_t button, uint32_t counter)
{
    uint8_t data[8];
    counter &= 0xFFFFFU;
    button &= 0x0F;

    data[0] = (uint8_t)((serial >> 24) & 0xFF);
    data[1] = (uint8_t)((serial >> 16) & 0xFF);
    data[2] = (uint8_t)((serial >> 8) & 0xFF);
    data[3] = (uint8_t)(serial & 0xFF);
    data[4] = (uint8_t)((button << 4) | ((counter >> 16) & 0x0F));
    data[5] = (uint8_t)((counter >> 8) & 0xFF);
    data[6] = (uint8_t)(counter & 0xFF);
    data[7] = mazda_v0_calculate_checksum(serial, button, counter);

    const uint8_t stored_5 = (uint8_t)((data[6] & 0x55) | (data[5] & 0xAA));
    const uint8_t stored_6 = (uint8_t)((data[6] & 0xAA) | (data[5] & 0x55));
    const uint8_t xor_mask = (uint8_t)(stored_5 ^ stored_6);
    const bool replace_second = subghz_protocol_blocks_parity8(data[7]) == 0;
    const uint8_t forward_mask = replace_second ? stored_5 : stored_6;

    data[5] = replace_second ? stored_5 : xor_mask;
    data[6] = replace_second ? xor_mask : stored_6;

    for (size_t i = 0; i < 5; i++) data[i] ^= forward_mask;

    uint64_t key = 0;
    for (int i = 0; i < 8; i++) key = (key << 8) | data[i];
    return key;
}

static void mazda_v0_append_byte(SubGhzRawPair *out, uint32_t *idx, uint32_t max, uint8_t value)
{
    for (int bit_i = 7; bit_i >= 0; bit_i--) {
        bool bit = ((value >> bit_i) & 1U) != 0U;
        pair(out, idx, max, bit ? MAZDA_V0_TE_SHORT : 0, bit ? 0 : MAZDA_V0_TE_SHORT);
        pair(out, idx, max, bit ? 0 : MAZDA_V0_TE_SHORT, bit ? MAZDA_V0_TE_SHORT : 0);
    }
}

static uint32_t encode_mazda_v0(const SubGhzKeyParams *params,
                                 SubGhzRawPair *out,
                                 uint32_t max_pairs,
                                 uint8_t reps)
{
    uint32_t serial = params->serial ? params->serial : (uint32_t)(params->key_value >> 32);
    uint8_t  button = params->btn ? (uint8_t)params->btn : (uint8_t)((params->key_value >> 60) & 0x0FU);
    uint32_t counter = params->cnt ? params->cnt : (uint32_t)(params->key_value & 0xFFFFFU);

    uint64_t key64 = mazda_v0_encode_key(serial, button, counter);

    const uint32_t pairs_per_rep = (12 + 3 + 8 + 1) * 16 + 2;
    const uint32_t total = pairs_per_rep * reps;
    if (total > max_pairs) return 0;

    uint32_t idx = 0;
    for (uint8_t r = 0; r < reps; r++) {
        for (size_t p = 0; p < 12; p++) mazda_v0_append_byte(out, &idx, max_pairs, 0xFF);
        pair(out, &idx, max_pairs, 0, MAZDA_V0_GAP_US);
        mazda_v0_append_byte(out, &idx, max_pairs, 0xFF);
        mazda_v0_append_byte(out, &idx, max_pairs, 0xFF);
        mazda_v0_append_byte(out, &idx, max_pairs, MAZDA_V0_SYNC_BYTE);
        for (int bi = 0; bi < 8; bi++) {
            uint8_t raw = (uint8_t)((key64 >> (56 - bi * 8)) & 0xFF);
            mazda_v0_append_byte(out, &idx, max_pairs, (uint8_t)~raw);
        }
        mazda_v0_append_byte(out, &idx, max_pairs, MAZDA_V0_TAIL_BYTE);
        pair(out, &idx, max_pairs, 0, MAZDA_V0_GAP_US);
    }
    return idx;
}

/*============================================================================*/
/* Chrysler V0 — PWM, 80-bit dual payload                                      */
/*============================================================================*/

#define CHRYSLER_V0_TE_SHORT         300
#define CHRYSLER_V0_TE_LONG_A        3400
#define CHRYSLER_V0_TE_LONG_B        3700
#define CHRYSLER_V0_TE_ONE_SHORT     600
#define CHRYSLER_V0_FRAME_GAP        15600
#define CHRYSLER_V0_PREAMBLE_PAIRS   24

static const uint8_t chrysler_v0_xor_table[16] = {
    0x0F, 0x02, 0x40, 0x0C, 0x30, 0x0E, 0x70, 0x08,
    0x10, 0x0A, 0x50, 0xF4, 0x2F, 0xF6, 0x6F, 0xF0,
};

static uint8_t chrysler_v0_reverse6(uint32_t value)
{
    uint8_t out = 0;
    for (int i = 0; i < 6; i++) {
        out = (uint8_t)((out << 1) | (value & 1U));
        value >>= 1;
    }
    return out;
}

static void chrysler_v0_transform_block(const uint8_t in[9], uint8_t out[9],
                                         uint32_t key, uint8_t button)
{
    uint8_t mask = chrysler_v0_xor_table[key & 0x0FU];
    if (button == 1U) mask ^= (key & 1U) ? 0xF0U : 0x0FU;
    for (size_t i = 0; i < 9; i++) out[i] = (uint8_t)(in[i] ^ mask);
}

static void chrysler_v0_build_payload(const uint8_t plain[9], uint8_t counter,
                                       uint8_t button, uint8_t header_low2,
                                       uint8_t payload[10])
{
    uint8_t transformed[9];
    chrysler_v0_transform_block(plain, transformed, counter, button);
    payload[0] = (uint8_t)((chrysler_v0_reverse6(counter) << 2U) | (header_low2 & 0x03U));
    memcpy(&payload[1], transformed, 9);
}

static uint32_t encode_chrysler_v0(const SubGhzKeyParams *params,
                                    SubGhzRawPair *out,
                                    uint32_t max_pairs,
                                    uint8_t reps)
{
    uint8_t plain_a[9], plain_b[9];
    memset(plain_a, 0, 9);
    memset(plain_b, 0, 9);
    for (int i = 0; i < 9 && i < (int)sizeof(params->extra); i++) plain_a[i] = params->extra[i];
    memcpy(plain_b, plain_a, 9);

    uint8_t header_low2 = (uint8_t)(params->key_value & 0x03U);
    uint8_t button = params->btn ? (uint8_t)params->btn : 1;
    uint8_t counter = (uint8_t)(params->cnt & 0x3FU);
    if (counter & 1U) counter = (uint8_t)((counter - 1U) & 0x3FU);
    uint8_t counter_b = (counter == 0) ? 0x3FU : (uint8_t)(counter - 1U);

    uint8_t payload_a[10], payload_b[10];
    chrysler_v0_build_payload(plain_a, counter, button, header_low2, payload_a);
    chrysler_v0_build_payload(plain_b, counter_b, button, header_low2, payload_b);

    const uint32_t pairs_per_rep =
        (CHRYSLER_V0_PREAMBLE_PAIRS * 2 + 2 + 80 * 2) * 2;
    const uint32_t total = pairs_per_rep * reps;
    if (total > max_pairs) return 0;

    uint32_t idx = 0;
    for (uint8_t r = 0; r < reps; r++) {
        for (uint8_t pass = 0; pass < 2; pass++) {
            const uint8_t *payload = (pass == 0) ? payload_a : payload_b;
            for (size_t p = 0; p < CHRYSLER_V0_PREAMBLE_PAIRS; p++) {
                pair(out, &idx, max_pairs, CHRYSLER_V0_TE_SHORT, 0);
                pair(out, &idx, max_pairs, 0, CHRYSLER_V0_TE_LONG_B);
            }
            pair(out, &idx, max_pairs, CHRYSLER_V0_TE_SHORT, 0);
            pair(out, &idx, max_pairs, 0, CHRYSLER_V0_FRAME_GAP);
            for (uint8_t bit = 0; bit < 80; bit++) {
                bool value = ((payload[bit >> 3] >> (7 - (bit & 7))) & 1U) != 0U;
                pair(out, &idx, max_pairs,
                     value ? CHRYSLER_V0_TE_ONE_SHORT : CHRYSLER_V0_TE_SHORT, 0);
                pair(out, &idx, max_pairs, 0,
                     value ? CHRYSLER_V0_TE_LONG_A : CHRYSLER_V0_TE_LONG_B);
            }
        }
    }
    return idx;
}

/*============================================================================*/
/* Fiat V0 — differential Manchester, 64-bit + 6-bit endbyte, 3 bursts         */
/*============================================================================*/

#define FIAT_V0_TE_SHORT       200
#define FIAT_V0_TE_LONG        400
#define FIAT_V0_PREAMBLE_PAIRS 150
#define FIAT_V0_GAP_US         800
#define FIAT_V0_TOTAL_BURSTS   3
#define FIAT_V0_INTER_BURST_US 25000

static bool fiat_v0_add_level(SubGhzRawPair *out, uint32_t *idx, uint32_t max,
                               bool level, uint32_t duration)
{
    if (*idx >= max) return false;
    out[*idx].high_us = level ? duration : 0;
    out[*idx].low_us  = level ? 0 : duration;
    (*idx)++;
    return true;
}

static bool fiat_v0_add_bit(SubGhzRawPair *out, uint32_t *idx, uint32_t max,
                             bool *prev_bit, bool bit)
{
    if (!*prev_bit && !bit) {
        if (!fiat_v0_add_level(out, idx, max, true, FIAT_V0_TE_SHORT) ||
            !fiat_v0_add_level(out, idx, max, false, FIAT_V0_TE_SHORT)) return false;
    } else if (!*prev_bit && bit) {
        if (!fiat_v0_add_level(out, idx, max, true, FIAT_V0_TE_LONG)) return false;
    } else if (*prev_bit && !bit) {
        if (!fiat_v0_add_level(out, idx, max, false, FIAT_V0_TE_LONG)) return false;
    } else {
        if (!fiat_v0_add_level(out, idx, max, false, FIAT_V0_TE_SHORT) ||
            !fiat_v0_add_level(out, idx, max, true, FIAT_V0_TE_SHORT)) return false;
    }
    *prev_bit = bit;
    return true;
}

static uint32_t encode_fiat_v0(const SubGhzKeyParams *params,
                                SubGhzRawPair *out,
                                uint32_t max_pairs,
                                uint8_t reps)
{
    uint32_t hop = (uint32_t)(params->key_value >> 32);
    uint32_t fix = (uint32_t)(params->key_value & 0xFFFFFFFFU);
    uint8_t endbyte = params->btn ? (uint8_t)params->btn : (uint8_t)(params->key_value & 0x7FU);
    uint8_t endbyte_to_send = endbyte >> 1;

    uint64_t data = ((uint64_t)hop << 32) | fix;

    const uint32_t burst_pairs =
        (FIAT_V0_PREAMBLE_PAIRS * 2) + 1 + 64 * 2 + 6 * 2 + 2;
    const uint32_t total = (burst_pairs * FIAT_V0_TOTAL_BURSTS + (FIAT_V0_TOTAL_BURSTS - 1)) * reps;
    if (total > max_pairs) return 0;

    uint32_t idx = 0;
    for (uint8_t r = 0; r < reps; r++) {
        for (uint8_t burst = 0; burst < FIAT_V0_TOTAL_BURSTS; burst++) {
            if (burst > 0) {
                if (!fiat_v0_add_level(out, &idx, max_pairs, false, FIAT_V0_INTER_BURST_US)) return 0;
            }
            for (int i = 0; i < FIAT_V0_PREAMBLE_PAIRS; i++) {
                if (!fiat_v0_add_level(out, &idx, max_pairs, true, FIAT_V0_TE_SHORT) ||
                    !fiat_v0_add_level(out, &idx, max_pairs, false, FIAT_V0_TE_SHORT)) return 0;
            }
            /* Replace last short LOW with gap */
            if (idx > 0) out[idx - 1].low_us = FIAT_V0_GAP_US;

            bool first_bit = ((data >> 63) & 1ULL) != 0ULL;
            bool prev = first_bit;
            if (first_bit) {
                if (!fiat_v0_add_level(out, &idx, max_pairs, true, FIAT_V0_TE_LONG)) return 0;
            } else {
                if (!fiat_v0_add_level(out, &idx, max_pairs, true, FIAT_V0_TE_SHORT) ||
                    !fiat_v0_add_level(out, &idx, max_pairs, false, FIAT_V0_TE_LONG)) return 0;
            }
            for (int bit = 62; bit >= 0; bit--) {
                if (!fiat_v0_add_bit(out, &idx, max_pairs, &prev,
                                     ((data >> bit) & 1ULL) != 0ULL)) return 0;
            }
            for (int bit = 5; bit >= 0; bit--) {
                if (!fiat_v0_add_bit(out, &idx, max_pairs, &prev,
                                     ((endbyte_to_send >> bit) & 1U) != 0U)) return 0;
            }
            if (prev) {
                if (!fiat_v0_add_level(out, &idx, max_pairs, false, FIAT_V0_TE_SHORT)) return 0;
            }
            if (!fiat_v0_add_level(out, &idx, max_pairs, false, FIAT_V0_TE_SHORT * 8)) return 0;
        }
    }
    return idx;
}

/*============================================================================*/
/* Renault V0 — Manchester, 82-bit, 3 bursts                                   */
/*============================================================================*/

#define RENAULT_V0_TE_DEFAULT_US    125
#define RENAULT_V0_MIN_BITS         0x52
#define RENAULT_V0_UPLOAD_CAPACITY  0x258

static const uint32_t renault_v0_matrix_low[42] = {
    0x00000001, 0x04000029, 0x0000001B, 0x00000000, 0x00000001, 0x05220124,
    0x00000001, 0x00088410, 0x60132D1D, 0x60170F87, 0x00000000, 0x002000A9,
    0x20863E01, 0x24BB3755, 0x640199A4, 0x24225C43, 0x607886F1, 0x6007A101,
    0x66672A10, 0x4651623F, 0x43380BBF, 0x20237F84, 0x4245755E, 0x60AAF581,
    0x22722DAD, 0x27C617F7, 0x46DE8F1B, 0x231DEC51, 0x03ACAA0B, 0x22D2BF81,
    0x626EF6AE, 0x40441F95, 0x00000001, 0x00000000, 0x20B9A590, 0x656C8E86,
    0x60129F96, 0x2368F667, 0x442A1A5C, 0x04C43242, 0x22198640, 0x23D6B958,
};
static const uint32_t renault_v0_matrix_high[42] = {
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00001004, 0x00000000, 0x00000000,
    0x0000100C, 0x00000004, 0x00000004, 0x00001004, 0x0000100C, 0x0000000C,
    0x00000004, 0x00001008, 0x00001008, 0x00001000, 0x00001008, 0x00000004,
    0x0000000C, 0x00000000, 0x0000000C, 0x00000000, 0x00000008, 0x00000004,
    0x0000100C, 0x0000000C, 0x00000000, 0x00000000, 0x00000008, 0x00001008,
    0x0000000C, 0x00001000, 0x00000000, 0x0000100C, 0x00001000, 0x00001008,
};

static void renault_v0_set_split_bit(uint32_t *low, uint32_t *high, uint8_t bit)
{
    if (bit < 32) *low |= 1UL << bit;
    else          *high |= 1UL << (bit - 32);
}

static uint8_t renault_v0_parity32(uint32_t value)
{
    uint8_t parity = 0;
    while (value) {
        parity ^= (value & 1U);
        value >>= 1;
    }
    return parity;
}

static void renault_v0_build_key(uint32_t serial, uint8_t button, uint8_t counter,
                                  uint64_t *out_data, uint32_t *out_key2)
{
    uint8_t vars[7];
    vars[0] = (button == 0x0AU) ? 1U : 0U;
    for (uint8_t bit = 0; bit < 6; bit++) vars[bit + 1] = (uint8_t)((counter >> bit) & 1U);

    uint32_t mask_low = 1U;
    uint32_t mask_high = 0U;
    uint8_t mask_bit = 1;

    for (uint8_t i = 0; i < 7; i++, mask_bit++) {
        if (vars[i]) renault_v0_set_split_bit(&mask_low, &mask_high, mask_bit);
    }
    for (uint8_t i = 0; i < 6; i++) {
        for (uint8_t j = (uint8_t)(i + 1); j < 7; j++, mask_bit++) {
            if (vars[i] & vars[j]) renault_v0_set_split_bit(&mask_low, &mask_high, mask_bit);
        }
    }
    for (uint8_t i = 0; i < 6; i++) {
        for (uint8_t j = (uint8_t)(i + 1); j < 7; j++) {
            for (uint8_t k = (uint8_t)(j + 1); k < 7; k++, mask_bit++) {
                if (vars[i] & vars[j] & vars[k]) renault_v0_set_split_bit(&mask_low, &mask_high, mask_bit);
            }
        }
    }

    uint8_t parity_bits[42];
    for (size_t row = 0; row < 42; row++) {
        const uint32_t mixed = (renault_v0_matrix_low[row] & mask_low) ^
                               (renault_v0_matrix_high[row] & mask_high);
        parity_bits[row] = renault_v0_parity32(mixed);
    }
    if (counter & 0x40U) parity_bits[41] ^= 1U;
    if ((counter >> 7U) != 0U) parity_bits[40] ^= 1U;

    uint32_t data_low = ((uint32_t)counter) << 24;
    uint32_t data_high = (serial << 8) | button;
    for (uint8_t i = 0; i < 24; i++) {
        if (parity_bits[i]) data_low |= 1UL << (23U - i);
    }
    uint32_t key2 = 0;
    for (uint8_t i = 24; i < 42; i++) {
        if (parity_bits[i]) key2 |= 1UL << (41U - i);
    }

    *out_data = ((uint64_t)data_high << 32U) | data_low;
    *out_key2 = key2;
}

static bool renault_v0_emit_decoded_bit(SubGhzRawPair *out, uint32_t *idx, uint32_t max,
                                         uint8_t *state, uint32_t te_short, bool bit)
{
    const uint32_t te_long = te_short * 2U;
    if (*state == 1U) {
        if (bit) {
            return pair_bool(out, idx, max, false, te_short) &&
                   pair_bool(out, idx, max, true, te_short);
        }
        *state = 2U;
        return pair_bool(out, idx, max, false, te_long);
    }
    if (bit) {
        *state = 1U;
        return pair_bool(out, idx, max, true, te_long);
    }
    return pair_bool(out, idx, max, true, te_short) &&
           pair_bool(out, idx, max, false, te_short);
}

static bool renault_v0_get_bit_msb82(uint64_t data, uint32_t key2, uint8_t bit_index)
{
    if (bit_index <= 0x3FU) return ((data >> (63U - bit_index)) & 1ULL) != 0ULL;
    return ((key2 >> (0x51U - bit_index)) & 1U) != 0U;
}

static uint32_t encode_renault_v0(const SubGhzKeyParams *params,
                                   SubGhzRawPair *out,
                                   uint32_t max_pairs,
                                   uint8_t reps)
{
    uint32_t serial = params->serial ? params->serial : (uint32_t)(params->key_value >> 40);
    uint8_t  button = params->btn ? (uint8_t)params->btn : (uint8_t)((params->key_value >> 32) & 0xFFU);
    uint8_t  counter = (uint8_t)(params->cnt ? params->cnt : ((params->key_value >> 24) & 0xFFU));

    uint64_t data;
    uint32_t key2;
    renault_v0_build_key(serial, button, counter, &data, &key2);

    const uint8_t preamble_pairs = 16;
    const uint8_t burst_count = 3;
    const uint32_t inter_burst_low = 1500;
    const uint32_t final_low = 1500;

    const uint32_t burst_pairs = 1 + (preamble_pairs * 2) + (RENAULT_V0_MIN_BITS * 2) + 2;
    const uint32_t total = (burst_pairs * burst_count + (burst_count - 1)) * reps;
    if (total > max_pairs) return 0;

    uint32_t idx = 0;
    for (uint8_t r = 0; r < reps; r++) {
        for (uint8_t burst = 0; burst < burst_count; burst++) {
            pair(out, &idx, max_pairs, true, 1000);
            uint8_t state = 1U;
            for (uint8_t p = 0; p < preamble_pairs; p++) {
                if (!renault_v0_emit_decoded_bit(out, &idx, max_pairs, &state,
                                                  RENAULT_V0_TE_DEFAULT_US, true)) return 0;
            }
            for (uint8_t bit = 0; bit < RENAULT_V0_MIN_BITS; bit++) {
                if (!renault_v0_emit_decoded_bit(out, &idx, max_pairs, &state,
                                                  RENAULT_V0_TE_DEFAULT_US,
                                                  renault_v0_get_bit_msb82(data, key2, bit))) return 0;
            }
            if (state == 2U) {
                if (!pair_bool(out, &idx, max_pairs, true, RENAULT_V0_TE_DEFAULT_US)) return 0;
            }
            uint32_t trailing = (burst + 1 < burst_count) ? inter_burst_low : final_low;
            if (!pair_bool(out, &idx, max_pairs, false, trailing)) return 0;
        }
    }
    return idx;
}

/*============================================================================*/
/* Star Line — KeeLoq rolling-code, 64-bit PWM                                 */
/*============================================================================*/

#define STAR_LINE_TE_SHORT  250
#define STAR_LINE_TE_LONG   500
#define STAR_LINE_PREAMBLE_PAIRS 6

static uint32_t encode_star_line(const SubGhzKeyParams *params,
                                  SubGhzRawPair *out,
                                  uint32_t max_pairs,
                                  uint8_t reps)
{
    uint32_t serial = params->serial ? params->serial : (uint32_t)(params->key_value >> 32);
    uint8_t  btn    = params->btn ? (uint8_t)params->btn : (uint8_t)((params->key_value >> 60) & 0x0FU);
    uint16_t cnt    = (uint16_t)(params->cnt ? params->cnt : (params->key_value & 0xFFFFU));

    uint64_t mfr_key = 0;
    for (int i = 0; i < 8; i++) mfr_key = (mfr_key << 8) | params->extra[i];

    uint32_t fix = ((uint32_t)btn << 24) | serial;
    uint32_t decrypt = ((uint32_t)btn << 24) | ((serial & 0xFF) << 16) | cnt;

    uint64_t device_key;
    if (mfr_key != 0) {
        device_key = keeloq_learn_simple(serial, mfr_key);
    } else {
        /* Without a manufacturer key, decrypt cannot be computed. Use the raw
         * key_value low 32 bits as the hop word so TX still produces a signal. */
        device_key = 0;
    }
    uint32_t hop = (device_key != 0) ? keeloq_encrypt(decrypt, device_key) : (uint32_t)(params->key_value & 0xFFFFFFFFU);
    uint64_t yek = ((uint64_t)fix << 32) | hop;
    uint64_t data = subghz_protocol_blocks_reverse_key(yek, 64);

    const uint32_t pairs_per_rep = (STAR_LINE_PREAMBLE_PAIRS * 2) + (64 * 2);
    const uint32_t total = pairs_per_rep * reps;
    if (total > max_pairs) return 0;

    uint32_t idx = 0;
    for (uint8_t r = 0; r < reps; r++) {
        for (uint32_t p = 0; p < STAR_LINE_PREAMBLE_PAIRS; p++) {
            pair(out, &idx, max_pairs, STAR_LINE_TE_LONG * 2, 0);
            pair(out, &idx, max_pairs, 0, STAR_LINE_TE_LONG * 2);
        }
        for (int i = 63; i >= 0; i--) {
            bool bit = ((data >> i) & 1ULL) != 0ULL;
            if (bit) {
                pair(out, &idx, max_pairs, STAR_LINE_TE_LONG, 0);
                pair(out, &idx, max_pairs, 0, STAR_LINE_TE_LONG);
            } else {
                pair(out, &idx, max_pairs, STAR_LINE_TE_SHORT, 0);
                pair(out, &idx, max_pairs, 0, STAR_LINE_TE_SHORT);
            }
        }
    }
    return idx;
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
/* Ford V1 — Manchester, 136-bit, CRC-16, inverse-block scramble, 6 bursts     */
/*============================================================================*/

#define FORD_V1_TE_SHORT           65
#define FORD_V1_TE_LONG            130
#define FORD_V1_PREAMBLE_PAIRS     400
#define FORD_V1_DATA_BYTES         17
#define FORD_V1_DATA_BITS          136
#define FORD_V1_BURST_COUNT        6
#define FORD_V1_INTER_BURST_US     50000
#define FORD_V1_LAST_BURST_GAP_US  260

static const uint8_t ford_v1_burst_pkt4_vals[FORD_V1_BURST_COUNT] =
    {0x08, 0x00, 0x10, 0x08, 0x00, 0x10};

static void ford_v1_encode_inverse_block(uint8_t block[9])
{
    uint8_t sum = 0;
    for (size_t i = 1; i <= 7; i++) {
        sum = (uint8_t)(sum + block[i]);
    }

    const uint8_t p6 = block[6];
    const uint8_t p7 = block[7];
    const uint8_t post6 = (uint8_t)((p6 & 0xAAU) | (p7 & 0x55U));
    const uint8_t post7 = (uint8_t)((p7 & 0xAAU) | (p6 & 0x55U));
    const uint8_t xorv = (uint8_t)(post6 ^ post7);

    uint8_t xor_byte;
    if ((__builtin_popcount((unsigned int)sum) & 1) != 0) {
        block[6] = xorv;
        block[7] = post7;
        xor_byte = post7;
    } else {
        block[6] = post6;
        block[7] = xorv;
        xor_byte = post6;
    }

    for (size_t i = 1; i <= 5; i++) {
        block[i] ^= xor_byte;
    }
}

static void ford_v1_encode_air_9bytes(const uint8_t plain9[9], uint8_t air9_out[9])
{
    uint8_t block[9];
    memcpy(block, plain9, 9);
    ford_v1_encode_inverse_block(block);
    memcpy(air9_out, block, 9);
}

static void ford_v1_plain_apply_fields(uint8_t plain9[9],
                                        uint32_t serial, uint8_t btn, uint32_t cnt)
{
    uint8_t chk = (uint8_t)(plain9[8] - plain9[6] - plain9[7] - plain9[5]);
    plain9[0] = (uint8_t)(serial & 0xFFU);
    plain9[1] = (uint8_t)((serial >> 24) & 0xFFU);
    plain9[2] = (uint8_t)((serial >> 16) & 0xFFU);
    plain9[3] = (uint8_t)((serial >> 8) & 0xFFU);
    plain9[5] = (uint8_t)(((btn & 0x0FU) << 4) | ((cnt >> 16) & 0x0FU));
    plain9[6] = (uint8_t)((cnt >> 8) & 0xFFU);
    plain9[7] = (uint8_t)(cnt & 0xFFU);
    plain9[8] = (uint8_t)(chk + plain9[7] + plain9[6] + plain9[5]);
}

static void ford_v1_build_raw_from_fields(uint32_t serial, uint8_t btn, uint32_t cnt,
                                           const uint8_t seed_plain9[9],
                                           uint8_t raw17[FORD_V1_DATA_BYTES])
{
    uint8_t plain9[9];
    memcpy(plain9, seed_plain9, 9);
    ford_v1_plain_apply_fields(plain9, serial, btn, cnt);

    uint8_t air9[9];
    ford_v1_encode_air_9bytes(plain9, air9);
    memcpy(&raw17[6], air9, 9);

    /* Fill key1 low 32 bits with a sane default; callers normally supply raw bytes. */
    raw17[0] = 0;
    raw17[1] = 0;
    raw17[2] = 0;
    raw17[3] = (uint8_t)((serial >> 24) & 0xFFU);
    raw17[4] = (uint8_t)((serial >> 16) & 0xFFU);
    raw17[5] = (uint8_t)((serial >> 8) & 0xFFU);

    uint16_t c = subghz_protocol_blocks_crc16(&raw17[3], 12, 0x1021, 0x0000);
    raw17[15] = (uint8_t)(c >> 8);
    raw17[16] = (uint8_t)(c & 0xFFU);
}

static uint32_t encode_ford_v1_burst(const uint8_t pkt[FORD_V1_DATA_BYTES],
                                      SubGhzRawPair *out,
                                      uint32_t *idx, uint32_t max,
                                      bool last_burst)
{
    const uint32_t te_short = FORD_V1_TE_SHORT;
    const uint32_t te_long  = FORD_V1_TE_LONG;

    for (uint32_t i = 0; i < FORD_V1_PREAMBLE_PAIRS; i++) {
        pair(out, idx, max, te_long, te_long);
    }
    pair(out, idx, max, te_long, te_short);

    for (size_t by = 0; by < FORD_V1_DATA_BYTES; by++) {
        uint8_t b = pkt[by];
        for (int bit_i = 7; bit_i >= 0; bit_i--) {
            bool bit = ((b >> bit_i) & 1U) != 0U;
            pair(out, idx, max, bit ? te_short : 0, bit ? 0 : te_short);
            pair(out, idx, max, bit ? 0 : te_short, bit ? te_short : 0);
        }
    }

    pair(out, idx, max, 0, last_burst ? FORD_V1_LAST_BURST_GAP_US : FORD_V1_INTER_BURST_US);
    return *idx;
}

static uint32_t encode_ford_v1(const uint8_t raw17[FORD_V1_DATA_BYTES],
                                SubGhzRawPair *out,
                                uint32_t max_pairs, uint8_t reps)
{
    const uint32_t burst_pairs =
        FORD_V1_PREAMBLE_PAIRS + 1 + (FORD_V1_DATA_BYTES * 16) + 1;
    const uint32_t total = burst_pairs * FORD_V1_BURST_COUNT * reps;
    if (total > max_pairs)
        return 0;

    uint32_t idx = 0;
    for (uint8_t r = 0; r < reps; r++) {
        for (uint8_t burst = 0; burst < FORD_V1_BURST_COUNT; burst++) {
            uint8_t pkt[FORD_V1_DATA_BYTES];
            memcpy(pkt, raw17, FORD_V1_DATA_BYTES);
            pkt[4] = ford_v1_burst_pkt4_vals[burst];
            uint16_t c = subghz_protocol_blocks_crc16(&pkt[3], 12, 0x1021, 0x0000);
            pkt[15] = (uint8_t)(c >> 8);
            pkt[16] = (uint8_t)(c & 0xFFU);
            encode_ford_v1_burst(pkt, out, &idx, max_pairs,
                                 (burst + 1U == FORD_V1_BURST_COUNT) && (r + 1U == reps));
        }
    }
    return idx;
}

static uint32_t encode_ford_v1_wrapper(const SubGhzKeyParams *params,
                                        SubGhzRawPair *out,
                                        uint32_t max_pairs,
                                        uint8_t reps)
{
    uint8_t raw17[FORD_V1_DATA_BYTES] = {0};

    /* Try to use supplied raw bytes first. */
    bool has_raw = false;
    for (int i = 0; i < 8; i++) {
        raw17[i] = (uint8_t)(params->key_value >> (56 - i * 8));
        if (raw17[i] != 0) has_raw = true;
    }
    for (int i = 0; i < 9; i++) {
        raw17[8 + i] = params->extra[i];
        if (params->extra[i] != 0) has_raw = true;
    }

    if (!has_raw && params->serial != 0) {
        uint8_t seed_plain9[9] = {0};
        ford_v1_build_raw_from_fields(params->serial, (uint8_t)params->btn, params->cnt,
                                       seed_plain9, raw17);
    }

    /* Patch key1 low bits to the ProtoPirate default if key1 looks empty. */
    if ((raw17[0] | raw17[1] | raw17[2]) == 0) {
        uint32_t lo = ((uint32_t)raw17[3] << 24) | ((uint32_t)raw17[4] << 16) |
                      ((uint32_t)raw17[5] << 8) | raw17[6];
        lo = (lo & 0xFF00FFFFU) | 0x80000U;
        raw17[3] = (uint8_t)(lo >> 24);
        raw17[4] = (uint8_t)(lo >> 16);
        raw17[5] = (uint8_t)(lo >> 8);
        raw17[6] = (uint8_t)(lo & 0xFFU);
    }

    return encode_ford_v1(raw17, out, max_pairs, reps);
}

/*============================================================================*/
/* Kia V3/V4 — PWM, 64-bit + 4-bit CRC, KeeLoq simple learning, CRC sweep      */
/*============================================================================*/

#define KIA_V3_V4_TE_SHORT       400
#define KIA_V3_V4_TE_LONG        800
#define KIA_V3_V4_SYNC_US        1200
#define KIA_V3_V4_END_US         800
#define KIA_V3_V4_PREAMBLE_PAIRS 12
#define KIA_V3_V4_DATA_BITS      64
#define KIA_V3_V4_CRC_BITS       4
#define KIA_V3_V4_CRC_COUNT      16

static uint64_t kia_v3_v4_build_tx_bitstream(uint32_t serial, uint8_t btn,
                                              uint32_t encrypted)
{
    const uint32_t serial_btn = (serial & 0x0FFFFFFFU) | ((uint32_t)(btn & 0x0FU) << 28);
    const uint64_t key = ((uint64_t)serial_btn << 32) | (uint64_t)encrypted;
    /* Reverse the full 64-bit key (LSB-first on air). */
    uint64_t rev = 0;
    for (int i = 0; i < 64; i++) {
        rev = (rev << 1) | ((key >> i) & 1ULL);
    }
    return rev;
}

static void kia_v3_v4_emit_bit_pwm(SubGhzRawPair *out, uint32_t *idx, uint32_t max,
                                    bool bit, bool v4)
{
    const uint32_t first_us  = bit ? KIA_V3_V4_TE_SHORT : KIA_V3_V4_TE_LONG;
    const uint32_t second_us = bit ? KIA_V3_V4_TE_LONG  : KIA_V3_V4_TE_SHORT;
    if (v4) {
        pair(out, idx, max, 0, first_us);
        pair(out, idx, max, second_us, 0);
    } else {
        pair(out, idx, max, first_us, 0);
        pair(out, idx, max, 0, second_us);
    }
}

static uint32_t encode_kia_v3_v4(const SubGhzKeyParams *params,
                                  SubGhzRawPair *out,
                                  uint32_t max_pairs,
                                  uint8_t reps,
                                  bool v4)
{
    /* Each "repetition" encodes one CRC sweep value.  The caller's reps field
     * is interpreted as the number of CRC values to emit (max 16). */
    const uint8_t sweep_reps = (reps > KIA_V3_V4_CRC_COUNT) ? KIA_V3_V4_CRC_COUNT : reps;
    const uint32_t burst_pairs =
        (KIA_V3_V4_PREAMBLE_PAIRS * 2) + 2 +
        (KIA_V3_V4_DATA_BITS * 2) +
        (KIA_V3_V4_CRC_BITS * 2) + 2;
    const uint32_t total = burst_pairs * sweep_reps;
    if (total > max_pairs)
        return 0;

    uint32_t serial = params->serial ? params->serial : (uint32_t)((params->key_value >> 32) & 0x0FFFFFFFU);
    uint8_t  btn    = params->btn ? (uint8_t)params->btn : (uint8_t)((params->key_value >> 60) & 0x0FU);
    uint32_t cnt    = params->cnt ? params->cnt : (uint32_t)(params->key_value & 0xFFFFU);

    /* Manufacturer key from extra[0..7]; default to the ProtoPirate KIA_KEY1 if zero. */
    uint64_t mfr_key = 0;
    for (int i = 0; i < 8; i++) {
        mfr_key = (mfr_key << 8) | params->extra[i];
    }
    if (mfr_key == 0) {
        /* ProtoPirate KIA_KEY1 default placeholder; real deployments set extra[]. */
        mfr_key = 0x0000000000000000ULL;
    }

    const uint32_t plaintext = (cnt & 0xFFFFU) |
                               ((serial & 0x3FFU) << 16) |
                               ((uint32_t)(btn & 0x0FU) << 28);
    const uint64_t device_key = keeloq_learn_simple(serial & 0x0FFFFFFFU, mfr_key);
    const uint32_t encrypted  = keeloq_encrypt(plaintext, device_key);
    const uint64_t tx_key     = kia_v3_v4_build_tx_bitstream(serial, btn, encrypted);

    uint32_t idx = 0;
    for (uint8_t crc_iter = 0; crc_iter < sweep_reps; crc_iter++) {
        for (uint32_t p = 0; p < KIA_V3_V4_PREAMBLE_PAIRS; p++) {
            if (v4) {
                pair(out, &idx, max_pairs, 0, KIA_V3_V4_TE_SHORT);
                pair(out, &idx, max_pairs, KIA_V3_V4_TE_SHORT, 0);
            } else {
                pair(out, &idx, max_pairs, KIA_V3_V4_TE_SHORT, 0);
                pair(out, &idx, max_pairs, 0, KIA_V3_V4_TE_SHORT);
            }
        }
        if (v4) {
            pair(out, &idx, max_pairs, 0, KIA_V3_V4_TE_SHORT);
            pair(out, &idx, max_pairs, KIA_V3_V4_SYNC_US, 0);
        } else {
            pair(out, &idx, max_pairs, KIA_V3_V4_TE_SHORT, 0);
            pair(out, &idx, max_pairs, 0, KIA_V3_V4_SYNC_US);
        }

        for (int i = 63; i >= 0; i--) {
            bool bit = ((tx_key >> i) & 1ULL) != 0ULL;
            kia_v3_v4_emit_bit_pwm(out, &idx, max_pairs, bit, v4);
        }

        const uint8_t crc = crc_iter & 0x0FU;
        for (int b = 3; b >= 0; b--) {
            bool bit = ((crc >> b) & 1U) != 0U;
            kia_v3_v4_emit_bit_pwm(out, &idx, max_pairs, bit, v4);
        }

        if (v4) {
            pair(out, &idx, max_pairs, 0, KIA_V3_V4_END_US);
            pair(out, &idx, max_pairs, KIA_V3_V4_END_US, 0);
        } else {
            pair(out, &idx, max_pairs, KIA_V3_V4_END_US, 0);
            pair(out, &idx, max_pairs, 0, KIA_V3_V4_END_US);
        }
    }
    return idx;
}

static uint32_t encode_kia_v3_wrapper(const SubGhzKeyParams *params,
                                       SubGhzRawPair *out,
                                       uint32_t max_pairs,
                                       uint8_t reps)
{
    return encode_kia_v3_v4(params, out, max_pairs, reps, false);
}

static uint32_t encode_kia_v4_wrapper(const SubGhzKeyParams *params,
                                       SubGhzRawPair *out,
                                       uint32_t max_pairs,
                                       uint8_t reps)
{
    return encode_kia_v3_v4(params, out, max_pairs, reps, true);
}

/*============================================================================*/
/* Kia V5 — Manchester, 64-bit + 3-bit CRC, custom 32-bit mixer                */
/*============================================================================*/

#define KIA_V5_TE_SHORT       400
#define KIA_V5_TE_LONG        800
#define KIA_V5_PREAMBLE_PAIRS 200
#define KIA_V5_DATA_BITS      64
#define KIA_V5_CRC_BITS       3

static void kia_v5_build_keystore_from_mfkey(uint8_t result[8], uint64_t mfkey)
{
    for (int i = 0; i < 8; i++) {
        result[i] = (uint8_t)((mfkey >> ((7 - i) * 8)) & 0xFFU);
    }
}

static uint32_t kia_v5_mixer_encode(uint32_t serial, uint16_t counter, uint8_t button,
                                     uint64_t mfkey)
{
    uint8_t keystore[8];
    kia_v5_build_keystore_from_mfkey(keystore, mfkey);

    uint8_t state_a = (uint8_t)(((serial >> 8) & 0x0FU) | ((button & 0x0FU) << 4));
    uint8_t state_b = (uint8_t)((counter >> 8) & 0xFFU);
    uint8_t state_c = (uint8_t)(serial & 0xFFU);
    uint8_t state_d = (uint8_t)(counter & 0xFFU);

    int ks_idx = 0;
    for (int round_i = 0; round_i < 18; round_i++) {
        uint8_t r = keystore[ks_idx];
        ks_idx = (ks_idx + 1) & 0x07;

        uint8_t running_d = state_d;
        for (int step = 0; step < 8; step++) {
            uint8_t base;
            if ((state_a & 0x80U) == 0) {
                base = (state_a & 0x04U) == 0 ? 0x74U : 0x2EU;
            } else {
                base = (state_a & 0x04U) == 0 ? 0x3AU : 0x5CU;
            }

            if (state_c & 0x10U) {
                base = (uint8_t)(((base >> 4) & 0x0FU) | ((base & 0x0FU) << 4));
            }
            if (state_b & 0x02U) {
                base = (uint8_t)((base & 0x3FU) << 2);
            }

            uint8_t base_final = base;
            if (running_d & 0x02U) {
                base_final = (uint8_t)((base & 0x7FU) << 1);
            }

            const bool carry_b = (state_b & 0x01U) != 0;
            const bool carry_c = (state_c & 0x01U) != 0;
            const bool carry_a = (state_a & 0x01U) != 0;

            uint8_t new_d = (uint8_t)(running_d >> 1);
            if (carry_b) new_d |= 0x80U;

            running_d ^= state_c;

            state_b = (uint8_t)(state_b >> 1);
            if (carry_c) state_b |= 0x80U;

            state_c = (uint8_t)(state_c >> 1);
            if (carry_a) state_c |= 0x80U;

            const uint8_t feedback = (uint8_t)(((running_d ^ r) << 7) ^ base_final);
            state_a = (uint8_t)(state_a >> 1);
            if (feedback & 0x80U) state_a |= 0x80U;

            r = (uint8_t)(r >> 1);
            running_d = new_d;
        }
        state_d = running_d;
    }

    return ((uint32_t)state_a << 24) | ((uint32_t)state_c << 16) |
           ((uint32_t)state_b << 8) | (uint32_t)state_d;
}

static uint8_t kia_v5_calculate_crc(uint64_t data)
{
    uint8_t crc = 0;
    for (int i = 63; i >= 0; i--) {
        const uint8_t bit = (uint8_t)((data >> i) & 1ULL);
        const uint8_t shifted_out = (crc >> 1U) & 1U;
        crc = (uint8_t)(((crc & 1U) << 1U) | bit);
        if (shifted_out) {
            crc ^= 3U;
        }
    }
    return (uint8_t)(crc & 3U);
}

static void kia_v5_emit_manchester_bit(SubGhzRawPair *out, uint32_t *idx, uint32_t max,
                                        bool bit_value)
{
    const uint32_t te = KIA_V5_TE_SHORT;
    if (bit_value) {
        pair(out, idx, max, 0, te);
        pair(out, idx, max, te, 0);
    } else {
        pair(out, idx, max, te, 0);
        pair(out, idx, max, 0, te);
    }
}

static uint32_t encode_kia_v5(const SubGhzKeyParams *params,
                               SubGhzRawPair *out,
                               uint32_t max_pairs,
                               uint8_t reps)
{
    const uint32_t burst_pairs =
        (KIA_V5_PREAMBLE_PAIRS * 2) + 4 +
        ((KIA_V5_DATA_BITS + KIA_V5_CRC_BITS) * 2) + 2;
    const uint32_t total = burst_pairs * reps;
    if (total > max_pairs)
        return 0;

    uint32_t serial = params->serial ? params->serial : (uint32_t)((params->key_value >> 32) & 0x0FFFFFFFU);
    uint8_t  btn    = params->btn ? (uint8_t)params->btn : (uint8_t)((params->key_value >> 60) & 0x0FU);
    uint32_t cnt    = params->cnt ? params->cnt : (uint32_t)(params->key_value & 0xFFFFU);

    uint64_t mfkey = 0;
    for (int i = 0; i < 8; i++) {
        mfkey = (mfkey << 8) | params->extra[i];
    }

    const uint32_t mixer = kia_v5_mixer_encode(serial & 0x0FFFFFFFU,
                                                (uint16_t)(cnt & 0xFFFFU),
                                                btn, mfkey);
    const uint64_t yek_new = ((uint64_t)btn << 60) |
                             ((uint64_t)(serial & 0x0FFFFFFFU) << 32) |
                             (uint64_t)mixer;

    uint64_t replay_data = 0;
    for (int i = 0; i < 8; i++) {
        const uint8_t b = (uint8_t)((yek_new >> (i * 8)) & 0xFFU);
        replay_data |= ((uint64_t)reverse8_local(b) << ((7 - i) * 8));
    }

    const uint8_t replay_crc = kia_v5_calculate_crc(replay_data);

    uint32_t idx = 0;
    for (uint8_t r = 0; r < reps; r++) {
        for (uint32_t p = 0; p < KIA_V5_PREAMBLE_PAIRS; p++) {
            pair(out, &idx, max_pairs, KIA_V5_TE_SHORT, 0);
            pair(out, &idx, max_pairs, 0, KIA_V5_TE_SHORT);
        }

        pair(out, &idx, max_pairs, 0, KIA_V5_TE_SHORT);
        pair(out, &idx, max_pairs, KIA_V5_TE_LONG, 0);
        pair(out, &idx, max_pairs, 0, KIA_V5_TE_SHORT);
        pair(out, &idx, max_pairs, KIA_V5_TE_SHORT, 0);

        for (int b = (int)KIA_V5_DATA_BITS - 1; b >= 0; b--) {
            bool bit = ((replay_data >> b) & 1ULL) != 0ULL;
            kia_v5_emit_manchester_bit(out, &idx, max_pairs, bit);
        }

        kia_v5_emit_manchester_bit(out, &idx, max_pairs, false);
        kia_v5_emit_manchester_bit(out, &idx, max_pairs, ((replay_crc >> 1U) & 1U) != 0U);
        kia_v5_emit_manchester_bit(out, &idx, max_pairs, (replay_crc & 1U) != 0U);

        pair(out, &idx, max_pairs, 0, KIA_V5_TE_SHORT);
        pair(out, &idx, max_pairs, KIA_V5_TE_SHORT, 0);
    }
    return idx;
}

/*============================================================================*/
/* Fiat V1 — OOK PWM, 104-bit, Hitag2-like BCM authenticator                   */
/*============================================================================*/

#define FIAT_V1_TE_SHORT       250
#define FIAT_V1_TE_LONG        500
#define FIAT_V1_WIRE_BITS      104
#define FIAT_V1_WIRE_BYTES     13
#define FIAT_V1_LEAD_IN_US     2033
#define FIAT_V1_GAP_US         3252

/* First known default key from ProtoPirate (used when no key is supplied). */
static const uint8_t fiat_v1_default_key[6] = {0xB7, 0x92, 0x80, 0xAE, 0xCC, 0x37};

static uint8_t fiat_v1_frame_xor(const uint8_t raw[FIAT_V1_WIRE_BYTES])
{
    uint8_t value = 0x01U;
    for (uint8_t i = 0; i < FIAT_V1_WIRE_BYTES - 1; i++) {
        value ^= raw[i];
    }
    return value;
}

static void fiat_v1_build_raw(uint8_t raw[FIAT_V1_WIRE_BYTES],
                               uint32_t uid, uint8_t button,
                               uint16_t control, uint32_t auth,
                               uint8_t tail_bits)
{
    memset(raw, 0, FIAT_V1_WIRE_BYTES);
    raw[1] = 0x01U;
    raw[2] = (uint8_t)(uid >> 24);
    raw[3] = (uint8_t)(uid >> 16);
    raw[4] = (uint8_t)(uid >> 8);
    raw[5] = (uint8_t)uid;
    raw[6] = (uint8_t)(((button & 0x0FU) << 4) | ((control >> 6) & 0x0FU));
    raw[7] = (uint8_t)(((control & 0x3FU) << 2) | ((auth >> 30) & 0x03U));
    raw[8]  = (uint8_t)(auth >> 22);
    raw[9]  = (uint8_t)(auth >> 14);
    raw[10] = (uint8_t)(auth >> 6);
    raw[11] = (uint8_t)((auth << 2) | (tail_bits & 0x03U));
    raw[12] = fiat_v1_frame_xor(raw);
}

static uint8_t fiat_v1_truth(uint32_t table, uint8_t index)
{
    return (uint8_t)((table >> index) & 1U);
}

static uint8_t fiat_v1_filter_index(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
{
    return (uint8_t)((a << 3) | (b << 2) | (c << 1) | d);
}

static uint8_t fiat_v1_bcm_hitag2_filter(const uint8_t state[6])
{
    uint8_t group = 0;
    group |= fiat_v1_truth(
        0x2c79U,
        fiat_v1_filter_index(
            (state[0] >> 1) & 1, (state[0] >> 2) & 1,
            (state[0] >> 4) & 1, (state[0] >> 5) & 1));
    group |= (uint8_t)(fiat_v1_truth(
                           0x6671U,
                           fiat_v1_filter_index(
                               state[1] & 1, (state[1] >> 1) & 1,
                               (state[1] >> 3) & 1, (state[1] >> 7) & 1))
                       << 1);
    group |= (uint8_t)(fiat_v1_truth(
                           0x6671U,
                           fiat_v1_filter_index(
                               (state[3] >> 5) & 1, state[2] & 1,
                               (state[2] >> 2) & 1, (state[2] >> 6) & 1))
                       << 2);
    group |= (uint8_t)(fiat_v1_truth(
                           0x6671U,
                           fiat_v1_filter_index(
                               (state[4] >> 6) & 1, state[3] & 1,
                               (state[3] >> 2) & 1, (state[3] >> 3) & 1))
                       << 3);
    group |= (uint8_t)(fiat_v1_truth(
                           0x2c79U,
                           fiat_v1_filter_index(
                               (state[5] >> 1) & 1, (state[5] >> 3) & 1,
                               (state[5] >> 4) & 1, (state[4] >> 5) & 1))
                       << 4);
    return fiat_v1_truth(0x7907287bUL, group);
}

static uint8_t fiat_v1_parity8(uint8_t value)
{
    value ^= (uint8_t)(value >> 4);
    value ^= (uint8_t)(value >> 2);
    value ^= (uint8_t)(value >> 1);
    return value & 1U;
}

static uint8_t fiat_v1_bcm_hitag2_feedback(const uint8_t state[6])
{
    static const uint8_t masks[6] = {0xb3, 0x80, 0x83, 0x22, 0x00, 0x73};
    uint8_t feedback = 0;
    for (uint8_t i = 0; i < 6; i++) {
        feedback ^= fiat_v1_parity8((uint8_t)(state[i] & masks[i]));
    }
    return feedback & 1U;
}

static void fiat_v1_bcm_hitag2_shift(uint8_t state[6], uint8_t input)
{
    for (uint8_t i = 0; i < 5; i++) {
        state[i] = (uint8_t)((state[i] << 1) | (state[i + 1] >> 7));
    }
    state[5] = (uint8_t)((state[5] << 1) | (input & 1U));
}

static uint8_t fiat_v1_input_bit_u32_be(uint32_t value, uint8_t index)
{
    return (uint8_t)((value >> (31U - index)) & 1U);
}

static uint8_t fiat_v1_input_bit_bytes_be(const uint8_t bytes[6], uint8_t index)
{
    return (uint8_t)((bytes[index >> 3] >> (7U - (index & 7U))) & 1U);
}

static uint32_t fiat_v1_bcm_generate_authenticator(uint32_t uid, uint8_t button,
                                                    uint16_t control,
                                                    const uint8_t key[6],
                                                    uint32_t epoch)
{
    uint8_t state[6] = {
        (uint8_t)(uid >> 24), (uint8_t)(uid >> 16),
        (uint8_t)(uid >> 8),  (uint8_t)uid,
        key[4], key[5],
    };

    const uint32_t iv = ((epoch & 0x3FFFFUL) << 14) |
                        (((uint32_t)control & 0x3FFUL) << 4) |
                        ((uint32_t)button & 0x0FU);

    for (uint8_t i = 0; i < 32; i++) {
        const uint8_t input = fiat_v1_input_bit_u32_be(iv, i) ^
                              fiat_v1_input_bit_bytes_be(key, i) ^
                              fiat_v1_bcm_hitag2_filter(state);
        fiat_v1_bcm_hitag2_shift(state, input);
    }

    uint32_t authenticator = 0;
    for (uint8_t i = 0; i < 32; i++) {
        authenticator = (authenticator << 1) | fiat_v1_bcm_hitag2_filter(state);
        fiat_v1_bcm_hitag2_shift(state, fiat_v1_bcm_hitag2_feedback(state));
    }
    return authenticator;
}

static uint32_t encode_fiat_v1(const SubGhzKeyParams *params,
                                SubGhzRawPair *out,
                                uint32_t max_pairs,
                                uint8_t reps)
{
    const uint32_t pairs_per_rep = 1 + FIAT_V1_WIRE_BITS + 1;
    const uint32_t total = pairs_per_rep * reps;
    if (total > max_pairs)
        return 0;

    uint32_t uid   = params->serial ? params->serial : (uint32_t)(params->key_value >> 32);
    uint8_t  btn   = params->btn ? (uint8_t)params->btn : (uint8_t)((params->key_value >> 60) & 0x0FU);
    uint16_t ctrl  = (uint16_t)(params->cnt ? (params->cnt & 0x3FFU) : ((params->key_value >> 16) & 0x3FFU));
    uint32_t epoch = (uint32_t)((params->key_value >> 26) & 0x3FFFFU);

    uint8_t key[6];
    for (int i = 0; i < 6; i++) {
        key[i] = params->extra[i];
    }
    if (key[0] == 0 && key[1] == 0 && key[2] == 0 && key[3] == 0 && key[4] == 0 && key[5] == 0) {
        memcpy(key, fiat_v1_default_key, 6);
    }

    /* If extra[6] holds tail bits, use them; otherwise default to 0. */
    uint8_t tail_bits = params->extra[6] & 0x03U;

    uint32_t auth = fiat_v1_bcm_generate_authenticator(uid, btn, ctrl, key, epoch);

    uint8_t raw[FIAT_V1_WIRE_BYTES];
    fiat_v1_build_raw(raw, uid, btn, ctrl, auth, tail_bits);

    uint32_t idx = 0;
    for (uint8_t r = 0; r < reps; r++) {
        pair(out, &idx, max_pairs, FIAT_V1_LEAD_IN_US, 0);
        for (uint8_t bit_index = 0; bit_index < FIAT_V1_WIRE_BITS; bit_index++) {
            bool bit = ((raw[bit_index >> 3] >> (7U - (bit_index & 7U))) & 1U) != 0U;
            pair(out, &idx, max_pairs,
                 bit ? FIAT_V1_TE_SHORT : 0,
                 bit ? 0 : FIAT_V1_TE_SHORT);
        }
        pair(out, &idx, max_pairs, 0, FIAT_V1_GAP_US);
    }
    return idx;
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
    [SubGhzProtoPirate_FordV0]      = encode_ford_v0,
    [SubGhzProtoPirate_MazdaV0]     = encode_mazda_v0,
    [SubGhzProtoPirate_HondaStatic] = encode_honda_static,
    [SubGhzProtoPirate_KiaV0]       = encode_kia_v0,
    [SubGhzProtoPirate_KiaV1]       = encode_kia_v1,
    [SubGhzProtoPirate_KiaV2]       = encode_kia_v2,
    [SubGhzProtoPirate_KiaV7]       = encode_kia_v7_wrapper,
    [SubGhzProtoPirate_RenaultV0]   = encode_renault_v0,
    [SubGhzProtoPirate_ChryslerV0]  = encode_chrysler_v0,
    [SubGhzProtoPirate_FiatV0]      = encode_fiat_v0,
    [SubGhzProtoPirate_Subaru]      = encode_subaru,
    [SubGhzProtoPirate_StarLine]    = encode_star_line,

    /* Tier B */
    [SubGhzProtoPirate_HondaV1]     = encode_honda_v1_wrapper,
    [SubGhzProtoPirate_HondaV2]     = encode_honda_v2_wrapper,
    [SubGhzProtoPirate_FordV2]      = encode_ford_v2_wrapper,
    [SubGhzProtoPirate_FordV1]      = encode_ford_v1_wrapper,
    [SubGhzProtoPirate_KiaV3]       = encode_kia_v3_wrapper,
    [SubGhzProtoPirate_KiaV4]       = encode_kia_v4_wrapper,
    [SubGhzProtoPirate_KiaV5]       = encode_kia_v5,
    [SubGhzProtoPirate_FiatV1]      = encode_fiat_v1,
};

static uint32_t required_pairs_for_id(SubGhzProtoPirateId id, uint8_t reps)
{
    switch (id) {
    case SubGhzProtoPirate_KiaV7:
        return (KIA_V7_PREAMBLE_PAIRS + 1 + (64 * 2) + 1 + 1) * 2 * reps;
    case SubGhzProtoPirate_FordV0:
        return ((2 + (FORD_V0_PREAMBLE_PAIRS * 2) + 2 + (79 * 2) + (16 * 2)) *
                FORD_V0_TOTAL_BURSTS + (FORD_V0_TOTAL_BURSTS - 1)) * reps;
    case SubGhzProtoPirate_MazdaV0:
        return (((12 + 3 + 8 + 1) * 16) + 2) * reps;
    case SubGhzProtoPirate_HondaStatic:
        return (1 + HONDA_STATIC_PREAMBLE_COUNT + (HONDA_STATIC_BITS * 2) + 1) * reps;
    case SubGhzProtoPirate_KiaV0:
        /* Worst case is the Suzuki subtype (type 2). */
        return ((KIA_V0_TYPE2_PREAMBLE_PAIRS * 2) + (64 * 2) + 3 +
                (KIA_V0_TAIL_PREAMBLE_PAIRS * 2) + (64 * 2)) * reps;
    case SubGhzProtoPirate_KiaV1:
        return (((KIA_V1_HEADER_PULSES * 2) + 1 + ((57U - 1) * 2)) * KIA_V1_TOTAL_BURSTS +
                (KIA_V1_TOTAL_BURSTS - 1)) * reps;
    case SubGhzProtoPirate_KiaV2:
        return (((KIA_V2_HEADER_PAIRS * 2) + 1 + ((53U - 1) * 2)) * KIA_V2_TOTAL_BURSTS) * reps;
    case SubGhzProtoPirate_RenaultV0:
        return ((1 + (16 * 2) + (RENAULT_V0_MIN_BITS * 2) + 2) * 3 + 2) * reps;
    case SubGhzProtoPirate_ChryslerV0:
        return ((CHRYSLER_V0_PREAMBLE_PAIRS * 2 + 2 + (80 * 2)) * 2) * reps;
    case SubGhzProtoPirate_FiatV0:
        return (((FIAT_V0_PREAMBLE_PAIRS * 2) + 1 + (64 * 2) + (6 * 2) + 2) * FIAT_V0_TOTAL_BURSTS +
                (FIAT_V0_TOTAL_BURSTS - 1)) * reps;
    case SubGhzProtoPirate_Subaru:
        return ((SUBARU_PREAMBLE_PAIRS * 2) + 2 + (64 * 2) + 2) * reps;
    case SubGhzProtoPirate_StarLine:
        return ((STAR_LINE_PREAMBLE_PAIRS * 2) + (64 * 2)) * reps;
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
    case SubGhzProtoPirate_FordV1:
        return (FORD_V1_PREAMBLE_PAIRS + 1 + (FORD_V1_DATA_BYTES * 16) + 1) *
               FORD_V1_BURST_COUNT * reps;
    case SubGhzProtoPirate_KiaV3:
    case SubGhzProtoPirate_KiaV4:
        return ((KIA_V3_V4_PREAMBLE_PAIRS * 2) + 2 +
                (KIA_V3_V4_DATA_BITS * 2) +
                (KIA_V3_V4_CRC_BITS * 2) + 2) * KIA_V3_V4_CRC_COUNT;
    case SubGhzProtoPirate_KiaV5:
        return ((KIA_V5_PREAMBLE_PAIRS * 2) + 4 +
                ((KIA_V5_DATA_BITS + KIA_V5_CRC_BITS) * 2) + 2) * reps;
    case SubGhzProtoPirate_FiatV1:
        return (1 + FIAT_V1_WIRE_BITS + 1) * reps;
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
