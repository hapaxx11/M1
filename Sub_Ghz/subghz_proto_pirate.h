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

#ifdef __cplusplus
}
#endif

#endif /* SUBGHZ_PROTO_PIRATE_H */
