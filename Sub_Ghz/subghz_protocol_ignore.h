/* SPDX-License-Identifier: GPL-2.0-or-later */
/* See COPYING.txt for license details. */

/*
 * subghz_protocol_ignore.h
 *
 * Sub-GHz protocol ignore-list ("protocol filter").
 *
 * Momentum/Unleashed expose a per-protocol enable/disable list in the Read
 * settings so the receiver can skip protocols the user is not interested in.
 * This module provides the same capability for the M1 (Hapax fork): a compact
 * bitset, indexed by registry index, that every decode loop consults before
 * running a protocol's decoder.
 *
 * The ignore set is honoured by ALL Sub-GHz reading features:
 *   - Read              (live capture — subghz_pulse_handler)
 *   - Read Raw / Decode Raw / Playlist decode
 *   - RF Rosetta Signal ID and Smart ID     (subghz_registry_decode_try_fn)
 *
 * Design notes
 * ------------
 *   - The core bitset operations (reset / set / is_ignored / count / hex
 *     serialize) are pure logic with ZERO dependency on the protocol registry,
 *     so they can be unit-tested in isolation and called from the RX hot path
 *     without any lookups.
 *   - The name-based helpers (set_name / is_ignored_name) resolve a protocol
 *     name to its registry index via subghz_protocol_find_by_name(); they are
 *     used by the UI (Protocol Filter scene) and are the only functions that
 *     touch the registry.
 *   - Persistence uses the hex bitmask form (index-based) so a full 128-bit
 *     set fits comfortably inside the settings file's 64-byte per-line writer.
 *     Like the other index-based Sub-GHz settings (freq_idx, mod_idx, …), the
 *     mask is tied to registry order; new protocols are appended to the end of
 *     the registry so existing bits keep their meaning.
 *
 * M1 Project — Hapax fork
 */

#ifndef SUBGHZ_PROTOCOL_IGNORE_H
#define SUBGHZ_PROTOCOL_IGNORE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Maximum number of protocols the ignore set can track.  Must be >= the
 * protocol registry size.  Matches LEGACY_PROTOCOL_MAX (128) in
 * m1_sub_ghz_decenc.c and the registry's _Static_assert upper bound.
 */
#define SUBGHZ_IGNORE_MAX_PROTOCOLS   128

/* Number of hex characters produced by subghz_ignore_serialize_hex()
 * (SUBGHZ_IGNORE_MAX_PROTOCOLS / 4), plus room for the NUL terminator. */
#define SUBGHZ_IGNORE_HEX_LEN         (SUBGHZ_IGNORE_MAX_PROTOCOLS / 4)
#define SUBGHZ_IGNORE_HEX_BUFSZ       (SUBGHZ_IGNORE_HEX_LEN + 1)

/*============================================================================*/
/* Core bitset API — pure, registry-independent                               */
/*============================================================================*/

/** Clear the ignore set (nothing ignored — every protocol is decoded). */
void subghz_ignore_reset(void);

/**
 * Query whether the protocol at @p index is ignored.
 *
 * Bounds-checked: an out-of-range index returns false so the RX hot path can
 * call this unconditionally for every registry index.
 */
bool subghz_ignore_is_ignored(uint16_t index);

/** Set / clear the ignore flag for the protocol at @p index (no-op if OOR). */
void subghz_ignore_set(uint16_t index, bool ignored);

/** Toggle the ignore flag for the protocol at @p index (no-op if OOR). */
void subghz_ignore_toggle(uint16_t index);

/** Number of protocols currently marked ignored. */
uint16_t subghz_ignore_count(void);

/*============================================================================*/
/* Hex-bitmask serialization — pure, used for settings persistence            */
/*============================================================================*/

/**
 * Serialize the ignore set as a fixed-width, big-endian hex string.
 *
 * Writes exactly SUBGHZ_IGNORE_HEX_LEN hex characters followed by a NUL when
 * @p buflen is large enough (>= SUBGHZ_IGNORE_HEX_BUFSZ).  The most-significant
 * word (highest indices) is emitted first so the string reads like a big
 * integer.  Returns the number of characters written (excluding the NUL); 0 if
 * @p buf is NULL or @p buflen is too small.
 */
size_t subghz_ignore_serialize_hex(char *buf, size_t buflen);

/**
 * Parse a hex string previously produced by subghz_ignore_serialize_hex().
 *
 * Clears the set first, then applies the parsed bits.  Leading/trailing
 * whitespace is skipped; parsing stops at the first non-hex character.  A NULL
 * or empty string simply clears the set.
 */
void subghz_ignore_deserialize_hex(const char *hex);

/*============================================================================*/
/* Name-based helpers — resolve via the protocol registry                     */
/*============================================================================*/

/**
 * Query the ignore flag by protocol name (case-insensitive registry lookup).
 * Returns false if the name is NULL or not found in the registry.
 */
bool subghz_ignore_is_ignored_name(const char *name);

/**
 * Set / clear the ignore flag by protocol name.
 * Returns true if the name resolved to a registry protocol, false otherwise.
 */
bool subghz_ignore_set_name(const char *name, bool ignored);

#ifdef __cplusplus
}
#endif

#endif /* SUBGHZ_PROTOCOL_IGNORE_H */
