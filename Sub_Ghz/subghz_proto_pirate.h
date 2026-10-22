/* See COPYING.txt for license details. */

/*
 * subghz_proto_pirate.h
 *
 * Proto Pirate automotive keyfob encoder dispatcher.
 *
 * Provides pure-logic TX waveform generation for automotive protocols from
 * the ProtoPirate project (https://github.com/RocketGod-git/ProtoPirate).
 *
 * The dispatcher takes a protocol name and parsed key fields, builds the
 * protocol-specific packet, and emits SubGhzRawPair[] timing pairs suitable
 * for M1's existing KEY→RAW transmitter pipeline.
 *
 * This module is hardware-independent and host-testable.
 *
 * M1 Project — Hapax fork
 */

#ifndef SUBGHZ_PROTO_PIRATE_H
#define SUBGHZ_PROTO_PIRATE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "subghz_key_encoder.h"   /* SubGhzKeyParams, SubGhzRawPair */

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*/
/* Protocol catalog                                                            */
/*============================================================================*/

/** ProtoPirate protocols supported by the encoder dispatcher. */
typedef enum {
    SubGhzProtoPirate_FordV0,
    SubGhzProtoPirate_MazdaV0,
    SubGhzProtoPirate_HondaStatic,
    SubGhzProtoPirate_KiaV0,
    SubGhzProtoPirate_KiaV1,
    SubGhzProtoPirate_KiaV2,
    SubGhzProtoPirate_KiaV7,
    SubGhzProtoPirate_RenaultV0,
    SubGhzProtoPirate_ChryslerV0,
    SubGhzProtoPirate_FiatV0,
    SubGhzProtoPirate_Subaru,
    SubGhzProtoPirate_StarLine,

    /* --- Tier B --- */
    SubGhzProtoPirate_HondaV1,
    SubGhzProtoPirate_HondaV2,
    SubGhzProtoPirate_FordV1,
    SubGhzProtoPirate_FordV2,
    SubGhzProtoPirate_KiaV3,
    SubGhzProtoPirate_KiaV4,
    SubGhzProtoPirate_KiaV5,
    SubGhzProtoPirate_FiatV1,

    /* --- Keystore-gated (AES / AUT64) --- */
    SubGhzProtoPirate_KiaV6,
    SubGhzProtoPirate_VAG,

    SubGhzProtoPirate_Count,
    SubGhzProtoPirate_Unknown = -1,
} SubGhzProtoPirateId;

/** Static catalog entry: canonical ProtoPirate name and capabilities. */
typedef struct {
    SubGhzProtoPirateId id;
    const char         *name;       /**< Exact ProtoPirate protocol name */
    uint16_t            te_short;   /**< Short symbol timing (µs) */
    uint16_t            te_long;    /**< Long symbol timing (µs), 0 if not PWM */
    uint8_t             bit_count;  /**< Key bit width */
    bool                manchester; /**< True if encoded as Manchester/biphase */
} SubGhzProtoPirateDef;

/** Read-only catalog (lives in .rodata). */
extern const SubGhzProtoPirateDef subghz_proto_pirate_catalog[];
extern const uint8_t              subghz_proto_pirate_catalog_count;

/*============================================================================*/
/* API                                                                         */
/*============================================================================*/

/** Look up a ProtoPirate protocol by exact or case-insensitive name. */
SubGhzProtoPirateId subghz_proto_pirate_find_by_name(const char *name);

/** Return true if the protocol is implemented in this dispatcher. */
bool subghz_proto_pirate_is_supported(const char *name);
bool subghz_proto_pirate_parse_serial_line(const char *line, uint32_t *serial);

/**
 * Encode a keyfob signal into raw timing pairs.
 *
 * @param params       Input protocol name + 64-bit Flipper Key value + bit count.
 * @param out          Output array of SubGhzRawPair.
 * @param max_pairs    Capacity of @p out.
 * @param repetitions  Number of times to repeat the full packet.
 * @return Number of pairs written, or 0 on unsupported/overflow/error.
 */
uint32_t subghz_proto_pirate_encode(const SubGhzKeyParams *params,
                                     SubGhzRawPair         *out,
                                     uint32_t               max_pairs,
                                     uint8_t                repetitions);

/**
 * Return the number of raw pairs required for @p params and @p repetitions
 * without writing any data. Useful for buffer allocation.
 */
uint32_t subghz_proto_pirate_required_pairs(const SubGhzKeyParams *params,
                                             uint8_t                repetitions);

#define SUBGHZ_PROTO_PIRATE_COUNTER_CACHE_SIZE 8u

static inline uint32_t subghz_proto_pirate_counter_advance_mask(
    uint8_t protocol, uint32_t encoded_pair_count)
{
    if (encoded_pair_count == 0u)
        return 0u;
    if (protocol == SubGhzProtoPirate_VAG)
        return 0xFFFFFFu;
    if (protocol == SubGhzProtoPirate_KiaV6)
        return 0xFFFFFFFFu;
    if (protocol == SubGhzProtoPirate_KiaV3 ||
        protocol == SubGhzProtoPirate_KiaV4)
        return 0xFFFFu;
    return 0u;
}

typedef struct {
    uint32_t capture_hash;
    uint32_t counter;
} subghz_proto_pirate_counter_entry_t;

typedef struct {
    subghz_proto_pirate_counter_entry_t
        entries[SUBGHZ_PROTO_PIRATE_COUNTER_CACHE_SIZE];
    uint8_t active_index;
    uint8_t next_replace;
} subghz_proto_pirate_counter_state_t;

static inline uint32_t subghz_proto_pirate_counter_capture_hash(
    const char *path, uint8_t protocol, uint64_t key_signature)
{
    if (path == NULL)
        return 0;
    if (path[0] == '0' && path[1] == ':')
        path += 2;
    uint32_t hash = 2166136261u;
    while (*path != '\0')
    {
        hash ^= (uint8_t)*path++;
        hash *= 16777619u;
    }
    hash ^= protocol;
    hash *= 16777619u;
    for (uint8_t i = 0; i < 8u; i++)
    {
        hash ^= (uint8_t)(key_signature >> (i * 8u));
        hash *= 16777619u;
    }
    if (hash == 0)
        hash = 1u;
    return hash;
}

static inline uint32_t subghz_proto_pirate_counter_get(
    subghz_proto_pirate_counter_state_t *state, const char *path,
    uint8_t protocol, uint32_t initial_counter, uint64_t key_signature)
{
    const uint32_t capture_hash =
        subghz_proto_pirate_counter_capture_hash(path, protocol, key_signature);
    if (state == NULL)
        return initial_counter;

    for (uint8_t i = 0; i < SUBGHZ_PROTO_PIRATE_COUNTER_CACHE_SIZE; i++)
    {
        if (state->entries[i].capture_hash == capture_hash &&
            capture_hash != 0u)
        {
            state->active_index = i;
            return state->entries[i].counter;
        }
    }

    for (uint8_t i = 0; i < SUBGHZ_PROTO_PIRATE_COUNTER_CACHE_SIZE; i++)
    {
        if (state->entries[i].capture_hash == 0u)
        {
            state->active_index = i;
            state->entries[i].capture_hash = capture_hash;
            state->entries[i].counter = initial_counter;
            return initial_counter;
        }
    }

    state->active_index = state->next_replace;
    state->next_replace =
        (uint8_t)((state->next_replace + 1u) %
                  SUBGHZ_PROTO_PIRATE_COUNTER_CACHE_SIZE);
    state->entries[state->active_index].capture_hash = capture_hash;
    state->entries[state->active_index].counter = initial_counter;
    return initial_counter;
}

static inline void subghz_proto_pirate_counter_advance(
    subghz_proto_pirate_counter_state_t *state, uint32_t mask)
{
    if (state != NULL)
    {
        subghz_proto_pirate_counter_entry_t *entry =
            &state->entries[state->active_index];
        entry->counter = (entry->counter + 1u) & mask;
    }
}

#ifdef __cplusplus
}
#endif

#endif /* SUBGHZ_PROTO_PIRATE_H */
