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
};

static uint32_t required_pairs_for_id(SubGhzProtoPirateId id, uint8_t reps)
{
    switch (id) {
    case SubGhzProtoPirate_KiaV7:
        return (KIA_V7_PREAMBLE_PAIRS + 1 + (64 * 2) + 1 + 1) * 2 * reps;
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
