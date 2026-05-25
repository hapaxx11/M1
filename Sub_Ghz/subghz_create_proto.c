/* See COPYING.txt for license details. */

/*
 * subghz_create_proto.c
 *
 * Pure-logic "Create from scratch" protocol catalog — see header for the
 * design rationale.  Zero hardware dependencies.
 *
 * Phase 8a of the Momentum parity migration.
 *
 * M1 Project — Hapax fork
 */

#include "subghz_create_proto.h"

/*============================================================================*/
/* Static protocol catalog                                                    */
/*============================================================================*/

/*
 * The five protocols below are the OOK-PWM rolling-code remotes that the
 * M1 can both build from a user-entered key and replay indefinitely via
 * the standard PWM key encoder.  Metadata mirrors the Bind New Remote
 * generator (see Sub_Ghz/subghz_new_remote_gen.c proto_specs[]) so the
 * two stay byte-for-byte consistent — Phase 8b will dedupe once the
 * SetType scene lands.
 *
 * Bit-count notes:
 *   - Alutech AT-4N: registry min_count_bit is 72, but the decoder also
 *     stores at most 64 bits.  64-bit keys transmit all data the
 *     decoder can recover within that limit.
 */
static const SubGhzCreateProtoSpec s_proto_catalog[SUBGHZ_CREATE_PROTO_COUNT] = {
    [SUBGHZ_CREATE_PROTO_CAME_ATOMO] = {
        .label       = "CAME Atomo 433",
        .proto_name  = "CAME Atomo",
        .freq_hz     = 433920000U,
        .bit_count   = 62U,
        .te          = 200U,
        .field_flags = SUBGHZ_CREATE_FIELD_KEY,
        .file_prefix = "CameAtomo",
    },
    [SUBGHZ_CREATE_PROTO_NICE_FLOR_S] = {
        .label       = "Nice FloR-S 433",
        .proto_name  = "Nice FloR-S",
        .freq_hz     = 433920000U,
        .bit_count   = 52U,
        .te          = 500U,
        .field_flags = SUBGHZ_CREATE_FIELD_KEY,
        .file_prefix = "NiceFlors",
    },
    [SUBGHZ_CREATE_PROTO_ALUTECH_AT4N] = {
        .label       = "Alutech AT-4N 433",
        .proto_name  = "Alutech AT-4N",
        .freq_hz     = 433920000U,
        .bit_count   = 64U,
        .te          = 400U,
        .field_flags = SUBGHZ_CREATE_FIELD_KEY,
        .file_prefix = "Alutech",
    },
    [SUBGHZ_CREATE_PROTO_DITEC_GOL4] = {
        .label       = "DITEC GOL4 433",
        .proto_name  = "DITEC_GOL4",
        .freq_hz     = 433920000U,
        .bit_count   = 54U,
        .te          = 400U,
        .field_flags = SUBGHZ_CREATE_FIELD_KEY,
        .file_prefix = "DitecGol4",
    },
    [SUBGHZ_CREATE_PROTO_KINGGATES_STYLO4K] = {
        .label       = "KingGates Stylo4k 433",
        .proto_name  = "KingGates Stylo4k",
        .freq_hz     = 433920000U,
        .bit_count   = 60U,
        .te          = 400U,
        .field_flags = SUBGHZ_CREATE_FIELD_KEY,
        .file_prefix = "KingGates",
    },
};

/*============================================================================*/
/* Helpers                                                                    */
/*============================================================================*/

static inline bool id_in_range(SubGhzCreateProtoId id)
{
    return (uint32_t)id < (uint32_t)SUBGHZ_CREATE_PROTO_COUNT;
}

/* Build a mask of @p bits low-order set bits.  bits must be in [0, 64]. */
static inline uint64_t low_mask_u64(uint32_t bits)
{
    if (bits == 0U) {
        return 0U;
    }
    if (bits >= 64U) {
        return ~(uint64_t)0U;
    }
    return ((uint64_t)1U << bits) - 1U;
}

/*============================================================================*/
/* Public API                                                                 */
/*============================================================================*/

uint32_t subghz_create_proto_count(void)
{
    return (uint32_t)SUBGHZ_CREATE_PROTO_COUNT;
}

const SubGhzCreateProtoSpec *subghz_create_proto_spec(SubGhzCreateProtoId id)
{
    if (!id_in_range(id)) {
        return (const SubGhzCreateProtoSpec *)0;
    }
    return &s_proto_catalog[id];
}

bool subghz_create_proto_has_field(SubGhzCreateProtoId id,
                                   SubGhzCreateFieldFlags field)
{
    if (!id_in_range(id)) {
        return false;
    }
    return (s_proto_catalog[id].field_flags & (uint16_t)field) != 0U;
}

bool subghz_create_proto_key_in_range(SubGhzCreateProtoId id,
                                      uint64_t              key_in,
                                      uint64_t             *key_out)
{
    if (!id_in_range(id)) {
        return false;
    }
    const uint64_t mask = low_mask_u64(s_proto_catalog[id].bit_count);
    const uint64_t masked = key_in & mask;
    if (key_out) {
        *key_out = masked;
    }
    return masked == key_in;
}
