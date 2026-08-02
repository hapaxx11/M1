/* SPDX-License-Identifier: GPL-2.0-or-later */
/* See COPYING.txt for license details. */

/*
 * subghz_protocol_ignore.h
 *
 * Sub-GHz protocol ignore filter ("protocol groups").
 *
 * Momentum exposes an *ignore filter* in the Read settings that lets the
 * receiver drop whole CATEGORIES of signals rather than toggling individual
 * protocols one by one.  In Momentum each protocol carries a category tag
 * (`SubGhzProtocol.filter`) and the receiver keeps an `ignore_filter` bitmask
 * of ignored categories; a decode is accepted only when the protocol's
 * category does NOT intersect the ignored set:
 *
 *     (protocol->filter & receiver->ignore_filter) == 0
 *
 * This module provides the same category-group behaviour for the M1 (Hapax
 * fork).  Instead of a per-protocol 1:1 toggle list, the user ignores broad
 * groups such as "Vehicles" (car alarms / keyfobs) or "Gates" (garage / gate
 * / barrier remotes), and every protocol that belongs to an ignored group is
 * skipped by all Sub-GHz reading features:
 *
 *   - Read              (live capture — subghz_pulse_handler)
 *   - Read Raw / Decode Raw / Playlist decode
 *   - RF Rosetta Signal ID and Smart ID     (subghz_registry_decode_try_fn)
 *
 * Design notes
 * ------------
 *   - Group MEMBERSHIP is data-driven from the protocol registry: the Weather
 *     and TPMS groups are derived from each protocol's `type`, and the
 *     remaining groups (Vehicles, Gates, Sensors, Pagers) from a small
 *     name-keyed table kept in subghz_protocol_ignore.c.  Membership is
 *     therefore defined in ONE place; adding a protocol to a group is a single
 *     table entry, mirroring Momentum's per-protocol `filter` tag.
 *   - The ignore STATE is a compact bitmask of ignored groups
 *     (`SubGhzIgnoreGroup` bits).  The hot-path query
 *     subghz_ignore_is_ignored(index) maps a registry index to its group mask
 *     (cached) and tests it against the ignored-group set — no allocation, no
 *     per-call string work after the first lookup.
 *   - Persistence uses the hex form of the ignored-group bitmask, so it fits
 *     comfortably inside the settings file's 64-byte per-line writer.  Because
 *     the mask is keyed by group id (not registry index), adding new protocols
 *     to an existing group does NOT invalidate a saved setting.
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
 * Maximum number of protocols the group cache can track.  Must be >= the
 * protocol registry size.  Matches LEGACY_PROTOCOL_MAX (128) in
 * m1_sub_ghz_decenc.c and the registry's _Static_assert upper bound.
 */
#define SUBGHZ_IGNORE_MAX_PROTOCOLS   128

/*============================================================================*/
/* Ignore groups (categories)                                                 */
/*============================================================================*/

/**
 * User-facing ignore categories.  Keep the order stable — the persisted
 * bitmask is keyed by these ids, and the Protocol Filter UI lists them in
 * this order.  Append new groups at the end so saved settings keep meaning.
 */
typedef enum {
    SubGhzIgnoreGroupWeather = 0,  /**< Weather stations (derived from type) */
    SubGhzIgnoreGroupTPMS,         /**< Tire-pressure sensors (derived from type) */
    SubGhzIgnoreGroupVehicles,     /**< Car alarms / automotive keyfobs */
    SubGhzIgnoreGroupGates,        /**< Garage / gate / barrier remotes */
    SubGhzIgnoreGroupSensors,      /**< Security / door / misc sensors */
    SubGhzIgnoreGroupPagers,       /**< Paging / powerline (POCSAG, X10, …) */
    SubGhzIgnoreGroupCount         /**< Number of groups (not a real group) */
} SubGhzIgnoreGroup;

/** Bit value for a group id (for the ignored-group / membership bitmasks). */
#define SUBGHZ_IGNORE_GROUP_BIT(g)   ((uint32_t)1u << (g))

/** Mask covering every valid group. */
#define SUBGHZ_IGNORE_GROUP_ALL_MASK \
    (SUBGHZ_IGNORE_GROUP_BIT(SubGhzIgnoreGroupCount) - 1u)

/* Number of hex characters produced by subghz_ignore_serialize_hex()
 * (32-bit ignored-group bitmask = 8 nibbles), plus room for the NUL. */
#define SUBGHZ_IGNORE_HEX_LEN         8
#define SUBGHZ_IGNORE_HEX_BUFSZ       (SUBGHZ_IGNORE_HEX_LEN + 1)

/*============================================================================*/
/* Ignore-set state                                                           */
/*============================================================================*/

/** Clear the ignore set (no group ignored — every protocol is decoded). */
void subghz_ignore_reset(void);

/** Build the protocol->group cache outside RX hot paths. */
void subghz_ignore_cache_warmup(void);

/**
 * Hot-path query: is the protocol at registry @p index currently ignored?
 *
 * Returns true when ANY group the protocol belongs to is in the ignored set.
 * Bounds-checked: an out-of-range index (or a protocol with no group) returns
 * false, so the RX decode loops can call this unconditionally for every
 * registry index.
 */
bool subghz_ignore_is_ignored(uint16_t index);

/*============================================================================*/
/* Per-group API — used by the Protocol Filter UI and persistence             */
/*============================================================================*/

/** True if group @p g is currently ignored. */
bool subghz_ignore_group_get(SubGhzIgnoreGroup g);

/** Set / clear the ignored flag for group @p g (no-op if out of range). */
void subghz_ignore_group_set(SubGhzIgnoreGroup g, bool ignored);

/** Toggle the ignored flag for group @p g (no-op if out of range). */
void subghz_ignore_group_toggle(SubGhzIgnoreGroup g);

/** Number of groups currently ignored. */
uint16_t subghz_ignore_group_ignored_count(void);

/** Human-readable name of group @p g ("Weather", "Vehicles", …). */
const char *subghz_ignore_group_name(SubGhzIgnoreGroup g);

/** Number of registry protocols that belong to group @p g. */
uint16_t subghz_ignore_group_protocol_count(SubGhzIgnoreGroup g);

/**
 * Group bitmask a protocol belongs to (bits are SUBGHZ_IGNORE_GROUP_BIT()).
 * Returns 0 for an out-of-range index or an uncategorised protocol.  Exposed
 * for the UI and unit tests; the RX hot path uses subghz_ignore_is_ignored().
 */
uint32_t subghz_ignore_group_mask_of(uint16_t index);

/*============================================================================*/
/* Hex-bitmask serialization — used for settings persistence                  */
/*============================================================================*/

/**
 * Serialize the ignored-group set as a fixed-width, big-endian hex string.
 *
 * Writes exactly SUBGHZ_IGNORE_HEX_LEN hex characters followed by a NUL when
 * @p buflen is large enough (>= SUBGHZ_IGNORE_HEX_BUFSZ).  Returns the number
 * of characters written (excluding the NUL); 0 if @p buf is NULL or too small.
 */
size_t subghz_ignore_serialize_hex(char *buf, size_t buflen);

/**
 * Parse a hex string previously produced by subghz_ignore_serialize_hex().
 *
 * Clears the set first, then applies the parsed bits (masked to valid groups).
 * Leading/trailing whitespace is skipped; parsing stops at the first non-hex
 * character.  A NULL or empty string simply clears the set.
 */
void subghz_ignore_deserialize_hex(const char *hex);

/*============================================================================*/
/* Name-based query — resolves via the protocol registry                      */
/*============================================================================*/

/**
 * True if the protocol named @p name is currently ignored (because one of its
 * groups is ignored).  Returns false if @p name is NULL or not in the
 * registry.
 */
bool subghz_ignore_is_ignored_name(const char *name);

#ifdef __cplusplus
}
#endif

#endif /* SUBGHZ_PROTOCOL_IGNORE_H */
