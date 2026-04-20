/* See COPYING.txt for license details. */

/**
 * @file  subghz_keeloq_encoder.h
 * @brief KeeLoq counter-mode rolling-code encoder.
 *
 * Given a captured KeeLoq rolling-code packet (from a Flipper .sub file or an
 * M1-captured signal), and a loaded manufacturer key, this module:
 *   1. Derives the per-device key using the appropriate learning mode.
 *   2. Decrypts the captured encrypted hop word.
 *   3. Increments the 16-bit counter.
 *   4. Re-encrypts the hop word.
 *   5. Encodes the resulting packet as OOK PWM timing pairs.
 *
 * Supported protocols:
 *   - KeeLoq (Bit: 64 Flipper format)
 *   - Star Line (Bit: 64 Flipper format)
 *   - Jarolift (Bit: 72 M1 format — HOP high 24 bits only; replay of
 *     Flipper-captured 64-bit Jarolift packets is fully supported)
 *
 * Key format (64-bit, Flipper SubGhz Key File compatible):
 *   bits [63:32] = FIX = (button[3:0] << 28) | serial[27:0]
 *   bits [31: 0] = HOP = encrypted rolling-code word
 *
 * Star Line key format:
 *   bits [63:32] = HOP = encrypted rolling-code word
 *   bits [31: 4] = serial[27:0]
 *   bits [ 3: 0] = button[3:0]
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "subghz_keeloq.h"
#include "subghz_keeloq_mfkeys.h"

/*============================================================================*/
/* Result codes                                                                */
/*============================================================================*/

typedef enum {
    KEELOQ_ENC_OK            = 0,   /**< Success */
    KEELOQ_ENC_NO_KEY        = 1,   /**< Manufacturer key not found */
    KEELOQ_ENC_BAD_BITS      = 2,   /**< Unsupported bit count */
    KEELOQ_ENC_BAD_PROTOCOL  = 3,   /**< Protocol not supported */
    KEELOQ_ENC_NOMEM         = 4,   /**< Allocation failure */
} KeeLoqEncResult;

/*============================================================================*/
/* OOK PWM timing                                                              */
/*============================================================================*/

/** Short pulse duration (µs) for KeeLoq OOK PWM. */
#define KEELOQ_TE_SHORT  400U
/** Long pulse duration (µs) for KeeLoq OOK PWM. */
#define KEELOQ_TE_LONG   800U
/** Number of preamble pulses (alternating HIGH/LOW at te_short). */
#define KEELOQ_PREAMBLE_PULSES  12U

/*============================================================================*/
/* SubGhzRawPair compatibility shim                                            */
/*============================================================================*/

#ifndef SUBGHZ_RAW_PAIR_DEFINED
#define SUBGHZ_RAW_PAIR_DEFINED
typedef struct {
    uint32_t high_us;   /**< HIGH pulse duration in µs */
    uint32_t low_us;    /**< LOW pulse duration in µs */
} SubGhzRawPair;
#endif

/*============================================================================*/
/* Packet parameter struct                                                     */
/*============================================================================*/

typedef struct {
    const char *protocol;    /**< Protocol name string (e.g. "KeeLoq") */
    const char *manufacture; /**< Manufacturer name from .sub file "Manufacture:" */
    uint64_t    key_value;   /**< Raw 64-bit key from .sub file */
    uint32_t    bit_count;   /**< Bit count from .sub file "Bit:" */
    uint32_t    te;          /**< TE from .sub file (0 = use default 400 µs) */
} KeeLoqEncParams;

/*============================================================================*/
/* API                                                                         */
/*============================================================================*/

/**
 * @brief  Encode a KeeLoq counter-mode replay packet into OOK PWM pairs.
 *
 * Looks up the manufacturer key by name in the loaded keystore, derives the
 * device key, increments the hop counter, re-encrypts, and fills @p pairs
 * with the OOK PWM timing output.
 *
 * Call this function for each repetition.  The function encodes exactly one
 * repetition (preamble + data bits).  Allocates @p pairs dynamically if
 * @p pairs_out is non-NULL; the caller must free the buffer.
 *
 * @param[in]  params      Packet parameters.
 * @param[out] pairs_out   Set to a malloc'd array of timing pairs.
 * @param[out] npairs_out  Number of pairs written.
 * @param[in]  reps        Number of repetitions to encode.
 * @return KEELOQ_ENC_OK on success, error code otherwise.
 */
KeeLoqEncResult keeloq_encode_replay(
    const KeeLoqEncParams *params,
    SubGhzRawPair        **pairs_out,
    uint32_t              *npairs_out,
    uint8_t                reps);

/**
 * @brief  Compute the number of OOK PWM pairs for one KeeLoq repetition.
 * @param  preamble_pulses  Number of preamble pulse-pair entries.
 * @param  data_bits        Number of data bits.
 * @return Total pairs per repetition (preamble + data + guard).
 */
uint32_t keeloq_pairs_per_rep(uint32_t preamble_pulses, uint32_t data_bits);

/**
 * @brief  Check if a protocol name is KeeLoq-cipher based and may support
 *         counter-mode replay when a manufacturer key is available.
 *
 * Returns true for "KeeLoq", "Star Line", "Jarolift".
 *
 * @param  protocol  Protocol name string (case-insensitive).
 * @return true if counter-mode replay may be possible.
 */
bool keeloq_is_keeloq_protocol(const char *protocol);
