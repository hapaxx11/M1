/* See COPYING.txt for license details. */

/**
 * @file   subghz_signal_fields.h
 * @brief  Per-protocol field extraction / assembly for the 64-bit Flipper
 *         SubGhz Key File representation — pure logic, host-testable
 *         (Phase 9a-1, foundation for the SignalSettings scene).
 *
 * The Saved → Settings flow needs to display and edit individual fields
 * (serial, button, counter) inside the opaque `uint64_t key` carried by
 * `flipper_subghz_signal_t`.  This module owns the bit/byte layouts for
 * each supported protocol family and provides reversible extract /
 * assemble helpers so the scene code never hard-codes shift amounts.
 *
 * Initial scope (Phase 9a-1): **KeeLoq family only** — KeeLoq, Star Line,
 * Jarolift.  These all use a 64-bit Flipper key whose layout depends on
 * the protocol name (KeeLoq/Jarolift vs Star Line — see
 * `subghz_keeloq_create.h` for the exact bit layouts).  The encrypted
 * 32-bit HOP word carries the rolling counter; decrypting it to expose
 * the counter for editing is a *separate* concern handled by the existing
 * `subghz_keeloq.c::keeloq_decrypt()` + per-file manufacturer-key
 * resolution and is wired up in Phase 9c.
 *
 * The module has zero hardware dependencies — no SI4463, no HAL, no
 * FreeRTOS, no FAT FS — and is fully testable on the host.
 *
 * M1 Project — Hapax fork
 */

#ifndef SUBGHZ_SIGNAL_FIELDS_H
#define SUBGHZ_SIGNAL_FIELDS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*/
/* KeeLoq-family extracted fields                                              */
/*============================================================================*/

/**
 * Plaintext (pre-cipher) representation of the three fields that the
 * SignalSettings scene displays/edits for a KeeLoq-family `.sub` file.
 *
 * `serial` carries the full 28-bit device serial (the low 6 bits are the
 * KeeLoq "discriminant" that gets baked into the plaintext HOP word).
 * `button` is the 4-bit button code.  `enc_hop` is the 32-bit encrypted
 * HOP word as it appears in the Flipper key — counter editing requires
 * decrypting it with the manufacturer key (Phase 9c).
 */
typedef struct {
    uint32_t serial;    /**< 28-bit device serial                  */
    uint8_t  button;    /**<  4-bit button code                    */
    uint32_t enc_hop;   /**< 32-bit encrypted HOP word (counter inside) */
} subghz_keeloq_fields_t;

/**
 * Return true iff @p protocol is a KeeLoq-family protocol whose 64-bit
 * Flipper-format key layout this module knows how to extract/assemble.
 *
 * Recognised (case-insensitive, prefix-terminated by NUL or space):
 *   - "KeeLoq"
 *   - "Star Line"
 *   - "Jarolift"
 *
 * @param[in] protocol  Protocol name string, may be NULL.
 * @return  true if the protocol is a supported KeeLoq family member.
 */
bool subghz_signal_fields_is_keeloq_family(const char *protocol);

/**
 * Extract the {serial, button, encrypted-hop} triplet from a 64-bit
 * Flipper-format KeeLoq-family key.
 *
 * The bit layout is protocol-dependent and mirrors the encoder's
 * `reconstruct_key()` exactly (see `subghz_keeloq_create.h` for the
 * forward direction):
 *
 *   KeeLoq / Jarolift:
 *     [63:60] button[3:0]
 *     [59:32] serial[27:0]
 *     [31: 0] encrypted HOP
 *
 *   Star Line:
 *     [63:32] encrypted HOP
 *     [31: 4] serial[27:0]
 *     [ 3: 0] button[3:0]
 *
 * @param[in]  protocol  Protocol name — must be a KeeLoq family member.
 * @param[in]  key       64-bit Flipper key as carried by
 *                       `flipper_subghz_signal_t::key`.
 * @param[out] out       Receives the extracted fields.  Must be non-NULL.
 * @retval true   Extraction succeeded.
 * @retval false  @p protocol is NULL/unsupported or @p out is NULL.
 *                On failure @p *out is zero-initialised.
 */
bool subghz_signal_fields_keeloq_extract(const char            *protocol,
                                          uint64_t               key,
                                          subghz_keeloq_fields_t *out);

/**
 * Inverse of @ref subghz_signal_fields_keeloq_extract — assemble a
 * 64-bit Flipper-format KeeLoq-family key from {serial, button,
 * encrypted-hop}, using the protocol-specific layout described above.
 *
 * Field masking: @p serial is masked to 28 bits, @p button to 4 bits.
 * @p enc_hop is taken verbatim as a 32-bit value.
 *
 * @param[in]  protocol  Protocol name — must be a KeeLoq family member.
 * @param[in]  fields    Fields to encode.  Must be non-NULL.
 * @param[out] key_out   Receives the assembled 64-bit Flipper key.
 *                       Must be non-NULL.
 * @retval true   Assembly succeeded.
 * @retval false  @p protocol is NULL/unsupported, or @p fields / @p key_out
 *                is NULL.  On failure @p *key_out is 0.
 */
bool subghz_signal_fields_keeloq_assemble(const char                   *protocol,
                                           const subghz_keeloq_fields_t *fields,
                                           uint64_t                     *key_out);

#ifdef __cplusplus
}
#endif

#endif /* SUBGHZ_SIGNAL_FIELDS_H */
