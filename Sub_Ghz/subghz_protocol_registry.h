/* See COPYING.txt for license details. */

/*
 * subghz_protocol_registry.h
 *
 * Flipper/Momentum-compatible Sub-GHz protocol registry.
 *
 * This header defines the protocol descriptor and registry types that mirror
 * the Flipper Zero firmware architecture (lib/subghz/protocols/).  The goal
 * is to minimise the manual work needed when porting a new protocol from
 * Flipper/Momentum by providing:
 *
 *   1.  A protocol descriptor struct with the same fields Flipper uses
 *       (name, type, timing constants, decode function pointer).
 *
 *   2.  A flat registry array that auto-enumerates protocol indices —
 *       adding a new protocol is a single line in the array.
 *
 *   3.  Flipper-compatible helper macros (DURATION_DIFF, subghz_protocol_blocks_add_bit)
 *       so ported decode logic compiles with minimal changes.
 *
 * M1 Project — Hapax fork
 */

#ifndef SUBGHZ_PROTOCOL_REGISTRY_H
#define SUBGHZ_PROTOCOL_REGISTRY_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

/*============================================================================*/
/* Protocol Type / Flag / Filter — mirrors Flipper's lib/subghz/types.h       */
/*============================================================================*/

/** Protocol behaviour type (Flipper: SubGhzProtocolType) */
typedef enum {
    SubGhzProtocolTypeStatic  = 0,   /**< Fixed (non-rolling) code */
    SubGhzProtocolTypeDynamic = 1,   /**< Rolling / encrypted code */
    SubGhzProtocolTypeRAW     = 2,   /**< Raw capture */
    SubGhzProtocolTypeWeather = 3,   /**< Weather station sensor (M1-specific extension) */
    SubGhzProtocolTypeTPMS    = 4,   /**< Tire-pressure sensor   (M1-specific extension) */
} SubGhzProtocolType;

/** Protocol capability / frequency flags (Flipper: SubGhzProtocolFlag) */
typedef enum {
    SubGhzProtocolFlag_315       = (1u << 0),
    SubGhzProtocolFlag_433       = (1u << 1),
    SubGhzProtocolFlag_868       = (1u << 2),
    SubGhzProtocolFlag_AM        = (1u << 3),
    SubGhzProtocolFlag_FM        = (1u << 4),
    SubGhzProtocolFlag_Decodable = (1u << 5),
    SubGhzProtocolFlag_Load      = (1u << 6),
    SubGhzProtocolFlag_Save      = (1u << 7),
    SubGhzProtocolFlag_Send      = (1u << 8),
} SubGhzProtocolFlag;

/** Category filter (matches Flipper SubGhzProtocolFilter values) */
typedef enum {
    SubGhzProtocolFilter_Auto      = 0,   /**< Accept in auto-detect scan */
    SubGhzProtocolFilter_Weather   = 1,   /**< Weather sensor only */
    SubGhzProtocolFilter_TPMS      = 2,   /**< TPMS only */
    SubGhzProtocolFilter_Industrial= 3,   /**< Industrial / paging (POCSAG, X10) */
} SubGhzProtocolFilter;

/*============================================================================*/
/* Timing constants block — mirrors Flipper's SubGhzBlockConst                */
/*============================================================================*/

typedef struct {
    uint16_t te_short;              /**< Short symbol duration (μs) */
    uint16_t te_long;               /**< Long  symbol duration (μs) */
    uint16_t te_delta;              /**< Timing tolerance (μs, absolute — Flipper style) */
    uint8_t  te_tolerance_pct;      /**< Timing tolerance (%, M1 legacy) — used when te_delta==0 */
    uint8_t  preamble_bits;         /**< Pulse pairs to skip as preamble */
    uint16_t min_count_bit_for_found; /**< Minimum valid bits */
} SubGhzBlockConst;

/*============================================================================*/
/* M1 decode function signature (unchanged from existing decoders)            */
/*============================================================================*/

/**
 * Decoder function pointer.
 *
 * @param protocol_index  Index of this protocol in the registry.
 * @param pulse_count     Number of pulses in the global pulse_times[] buffer.
 * @return 0 on successful decode, 1 on failure.
 *
 * The function reads pulses from subghz_decenc_ctl.pulse_times[] and on
 * success writes results into subghz_decenc_ctl.n64_decodedvalue, etc.
 * This is the existing M1 convention — no changes needed in decoder bodies.
 */
typedef uint8_t (*SubGhzDecodeFn)(uint16_t protocol_index, uint16_t pulse_count);

/*============================================================================*/
/* Protocol Descriptor — one per protocol                                     */
/*============================================================================*/

/**
 * Complete protocol descriptor.
 *
 * Combines the Flipper SubGhzProtocol metadata with M1's SubGHz_protocol_t
 * timing parameters and decode function pointer.  Adding a new protocol
 * requires filling ONE of these structs and adding it to the registry array.
 */
typedef struct {
    /* --- Identity (Flipper-compatible) --- */
    const char          *name;      /**< Flipper SUBGHZ_PROTOCOL_*_NAME constant */
    SubGhzProtocolType   type;      /**< Static / Dynamic / RAW / Weather / TPMS */
    uint32_t             flags;     /**< SubGhzProtocolFlag bitmask */
    SubGhzProtocolFilter filter;    /**< Category filter */

    /* --- Timing (M1 + Flipper hybrid) --- */
    SubGhzBlockConst     timing;    /**< Pulse timing parameters */

    /* --- Decode (M1 convention) --- */
    SubGhzDecodeFn       decode;    /**< Decoder function (NULL = not implemented) */
} SubGhzProtocolDef;

/*============================================================================*/
/* Protocol Registry                                                          */
/*============================================================================*/

/** Global registry array (defined in subghz_protocol_registry.c) */
extern const SubGhzProtocolDef subghz_protocol_registry[];

/** Number of protocols in the registry (defined in subghz_protocol_registry.c) */
extern const uint16_t subghz_protocol_registry_count;

/*============================================================================*/
/* Registry Lookup Helpers                                                     */
/*============================================================================*/

/** Find a protocol index by its Flipper-compatible name.  Returns -1 if not found. */
int16_t subghz_protocol_find_by_name(const char *name);

/** Get the protocol descriptor by index.  Returns NULL if out of range. */
const SubGhzProtocolDef* subghz_protocol_get(uint16_t index);

/** Get the protocol name by index (equivalent to old protocol_text[]).  Returns NULL if out of range. */
const char* subghz_protocol_get_name(uint16_t index);

/*============================================================================*/
/* Flipper-Compatible Helper Macros                                           */
/*============================================================================*/

/**
 * DURATION_DIFF(a, b) — absolute difference between two durations.
 * Mirrors Flipper's DURATION_DIFF macro from lib/subghz/blocks/math.h.
 * Requires <stdlib.h> for abs() — included at the top of this header.
 */
#ifndef DURATION_DIFF
#define DURATION_DIFF(a, b)  ((uint32_t)abs((int32_t)(a) - (int32_t)(b)))
#endif

/**
 * Bit accumulator helpers — mirror Flipper's SubGhzBlockDecoder helpers.
 * These operate on a (uint64_t *decode_data, uint16_t *decode_count_bit) pair.
 */
static inline void subghz_protocol_blocks_add_bit(uint64_t *decode_data,
                                                    uint16_t *decode_count_bit,
                                                    uint8_t bit)
{
    *decode_data = (*decode_data << 1) | (bit & 1u);
    (*decode_count_bit)++;
}

/**
 * Reverse key bits — mirrors Flipper's subghz_protocol_blocks_reverse_key().
 */
static inline uint64_t subghz_protocol_blocks_reverse_key(uint64_t key, uint8_t count_bit)
{
    uint64_t result = 0;
    for (uint8_t i = 0; i < count_bit; i++) {
        result = (result << 1) | (key & 1u);
        key >>= 1;
    }
    return result;
}

/*============================================================================*/
/* Convenience Macro for Declaring a Protocol                                 */
/*============================================================================*/

/**
 * SUBGHZ_PROTOCOL_DEFINE(varname, ...)
 *
 * Expands to a static const SubGhzProtocolDef.  Use in the registry array:
 *
 *     const SubGhzProtocolDef subghz_protocol_registry[] = {
 *         SUBGHZ_PROTOCOL_DEFINE(
 *             .name   = "Princeton",
 *             .type   = SubGhzProtocolTypeStatic,
 *             .flags  = SubGhzProtocolFlag_433 | SubGhzProtocolFlag_AM |
 *                       SubGhzProtocolFlag_Decodable | SubGhzProtocolFlag_Save |
 *                       SubGhzProtocolFlag_Send,
 *             .filter = SubGhzProtocolFilter_Auto,
 *             .timing = { .te_short=370, .te_long=1140, .te_tolerance_pct=20,
 *                         .min_count_bit_for_found=24 },
 *             .decode = subghz_decode_princeton,
 *         ),
 *         // ... more protocols ...
 *     };
 *
 * The macro is trivial (just curly braces) but documents intent and makes
 * grep/search easier across the codebase.
 */
#define SUBGHZ_PROTOCOL_DEFINE(...) { __VA_ARGS__ }

#endif /* SUBGHZ_PROTOCOL_REGISTRY_H */
