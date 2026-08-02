/* SPDX-License-Identifier: GPL-2.0-or-later */
/* See COPYING.txt for license details. */

/*
 * subghz_protocol_ignore.c
 *
 * Implementation of the Sub-GHz protocol ignore filter (category groups).
 * See subghz_protocol_ignore.h for the rationale and API contract.
 *
 * M1 Project — Hapax fork
 */

#include "subghz_protocol_ignore.h"
#include "subghz_protocol_registry.h"

/*============================================================================*/
/* Ignored-group state                                                        */
/*============================================================================*/

/* Bitmask of ignored groups; bit SUBGHZ_IGNORE_GROUP_BIT(g) set == ignored. */
static uint32_t g_ignored_groups;

/*============================================================================*/
/* Group membership (data-driven from the registry)                           */
/*============================================================================*/

/*
 * Name-keyed membership table for the groups that are NOT derived from the
 * protocol `type` (Weather / TPMS are derived).  Case-insensitive match on the
 * registry `name`.  Several registry entries may share a name (e.g. the
 * Chamberlain "Cham_Code" variants or the two "Scher-Khan" flavours) — they
 * all resolve to the same group, which is the intended behaviour.
 *
 * Adding a protocol to a group is a single line here, mirroring Momentum's
 * per-protocol `SubGhzProtocol.filter` category tag.
 */
typedef struct {
    const char *name;
    uint32_t    groups;   /* OR of SUBGHZ_IGNORE_GROUP_BIT(...) */
} subghz_group_map_t;

#define GB(g)   (SUBGHZ_IGNORE_GROUP_BIT(g))

static const subghz_group_map_t k_group_map[] = {
    /* ── Vehicles — car alarms / automotive keyfobs ─────────────────────── */
    { "Star Line",         GB(SubGhzIgnoreGroupVehicles) },
    { "Scher-Khan",        GB(SubGhzIgnoreGroupVehicles) },
    { "Toyota",            GB(SubGhzIgnoreGroupVehicles) },
    { "KIA Seed",          GB(SubGhzIgnoreGroupVehicles) },
    { "Revers_RB2",        GB(SubGhzIgnoreGroupVehicles) },

    /* ── Gates — garage / gate / barrier remotes ────────────────────────── */
    { "Princeton",         GB(SubGhzIgnoreGroupGates) },
    { "CAME",              GB(SubGhzIgnoreGroupGates) },
    { "CAME TWEE",         GB(SubGhzIgnoreGroupGates) },
    { "CAME Atomo",        GB(SubGhzIgnoreGroupGates) },
    { "Nice FLO",          GB(SubGhzIgnoreGroupGates) },
    { "Nice FloR-S",       GB(SubGhzIgnoreGroupGates) },
    { "FAAC SLH",          GB(SubGhzIgnoreGroupGates) },
    { "Hormann HSM",       GB(SubGhzIgnoreGroupGates) },
    { "Hormann BiSecur",   GB(SubGhzIgnoreGroupGates) },
    { "Marantec",          GB(SubGhzIgnoreGroupGates) },
    { "Marantec24",        GB(SubGhzIgnoreGroupGates) },
    { "Somfy Telis",       GB(SubGhzIgnoreGroupGates) },
    { "Somfy Keytis",      GB(SubGhzIgnoreGroupGates) },
    { "Security+ 2.0",     GB(SubGhzIgnoreGroupGates) },
    { "Security+ 1.0",     GB(SubGhzIgnoreGroupGates) },
    { "Centurion",         GB(SubGhzIgnoreGroupGates) },
    { "KingGates Stylo4k", GB(SubGhzIgnoreGroupGates) },
    { "Alutech AT-4N",     GB(SubGhzIgnoreGroupGates) },
    { "Nero Radio",        GB(SubGhzIgnoreGroupGates) },
    { "Nero Sketch",       GB(SubGhzIgnoreGroupGates) },
    { "Beninca ARC",       GB(SubGhzIgnoreGroupGates) },
    { "DITEC_GOL4",        GB(SubGhzIgnoreGroupGates) },
    { "GateTX",            GB(SubGhzIgnoreGroupGates) },
    { "Dooya",             GB(SubGhzIgnoreGroupGates) },
    { "Roger",             GB(SubGhzIgnoreGroupGates) },
    { "Linear",            GB(SubGhzIgnoreGroupGates) },
    { "LinearDelta3",      GB(SubGhzIgnoreGroupGates) },
    { "Clemsa",            GB(SubGhzIgnoreGroupGates) },
    { "Doitrand",          GB(SubGhzIgnoreGroupGates) },
    { "Dickert_MAHS",      GB(SubGhzIgnoreGroupGates) },
    { "Elplast",           GB(SubGhzIgnoreGroupGates) },
    { "Phoenix_V2",        GB(SubGhzIgnoreGroupGates) },
    { "Feron",             GB(SubGhzIgnoreGroupGates) },
    { "BETT",              GB(SubGhzIgnoreGroupGates) },
    { "MegaCode",          GB(SubGhzIgnoreGroupGates) },
    { "Mastercode",        GB(SubGhzIgnoreGroupGates) },
    { "Cham_Code",         GB(SubGhzIgnoreGroupGates) },
    { "Jarolift",          GB(SubGhzIgnoreGroupGates) },
    { "Hollarm",           GB(SubGhzIgnoreGroupGates) },
    { "Nord ICE",          GB(SubGhzIgnoreGroupGates) },

    /* ── Sensors — security / door / misc sensors ───────────────────────── */
    { "Magellan",          GB(SubGhzIgnoreGroupSensors) },
    { "Honeywell",         GB(SubGhzIgnoreGroupSensors) },
    { "Honeywell_WDB",     GB(SubGhzIgnoreGroupSensors) },
    { "KeyFinder",         GB(SubGhzIgnoreGroupSensors) },

    /* ── Pagers — paging / powerline home automation ────────────────────── */
    { "POCSAG",            GB(SubGhzIgnoreGroupPagers) },
    { "PCSG Generic",      GB(SubGhzIgnoreGroupPagers) },
    { "X10",               GB(SubGhzIgnoreGroupPagers) },
    { "FireCracker",       GB(SubGhzIgnoreGroupPagers) },
};

#define K_GROUP_MAP_COUNT  ((size_t)(sizeof(k_group_map) / sizeof(k_group_map[0])))

/* Display names, indexed by SubGhzIgnoreGroup id. */
static const char *const k_group_names[SubGhzIgnoreGroupCount] = {
    [SubGhzIgnoreGroupWeather]  = "Weather",
    [SubGhzIgnoreGroupTPMS]     = "TPMS",
    [SubGhzIgnoreGroupVehicles] = "Vehicles",
    [SubGhzIgnoreGroupGates]    = "Gates",
    [SubGhzIgnoreGroupSensors]  = "Sensors",
    [SubGhzIgnoreGroupPagers]   = "Pagers",
};

/*----------------------------------------------------------------------------*/
/* Group-mask cache — built once from the (const) registry.                   */
/*----------------------------------------------------------------------------*/

static uint32_t g_group_cache[SUBGHZ_IGNORE_MAX_PROTOCOLS];
static bool    g_cache_built;

static int ascii_lower(int c)
{
    return (c >= 'A' && c <= 'Z') ? (c - 'A' + 'a') : c;
}

static bool name_equals_ci(const char *a, const char *b)
{
    if (!a || !b)
        return false;
    while (*a && *b)
    {
        if (ascii_lower((unsigned char)*a) != ascii_lower((unsigned char)*b))
            return false;
        a++;
        b++;
    }
    return *a == *b;
}

static uint32_t compute_group_mask(uint16_t index)
{
    if (index >= subghz_protocol_registry_count)
        return 0;

    uint32_t mask = 0;

    /* Weather / TPMS are derived directly from the protocol type. */
    switch (subghz_protocol_registry[index].type)
    {
        case SubGhzProtocolTypeWeather:
            mask |= GB(SubGhzIgnoreGroupWeather);
            break;
        case SubGhzProtocolTypeTPMS:
            mask |= GB(SubGhzIgnoreGroupTPMS);
            break;
        default:
            break;
    }

    /* Remaining groups come from the name-keyed membership table. */
    const char *name = subghz_protocol_registry[index].name;
    if (name)
    {
        for (size_t k = 0; k < K_GROUP_MAP_COUNT; k++)
        {
            if (name_equals_ci(name, k_group_map[k].name))
            {
                mask |= k_group_map[k].groups;
                break;
            }
        }
    }

    return mask;
}

static void ensure_cache(void)
{
    if (g_cache_built)
        return;

    uint16_t n = subghz_protocol_registry_count;
    if (n > SUBGHZ_IGNORE_MAX_PROTOCOLS)
        n = SUBGHZ_IGNORE_MAX_PROTOCOLS;

    for (uint16_t i = 0; i < n; i++)
        g_group_cache[i] = compute_group_mask(i);

    g_cache_built = true;
}

/*============================================================================*/
/* Ignore-set state                                                           */
/*============================================================================*/

void subghz_ignore_reset(void)
{
    g_ignored_groups = 0;
}

uint32_t subghz_ignore_group_mask_of(uint16_t index)
{
    if (index >= subghz_protocol_registry_count ||
        index >= SUBGHZ_IGNORE_MAX_PROTOCOLS)
        return 0;
    ensure_cache();
    return g_group_cache[index];
}

bool subghz_ignore_is_ignored(uint16_t index)
{
    return (subghz_ignore_group_mask_of(index) & g_ignored_groups) != 0;
}

/*============================================================================*/
/* Per-group API                                                              */
/*============================================================================*/

bool subghz_ignore_group_get(SubGhzIgnoreGroup g)
{
    if (g >= SubGhzIgnoreGroupCount)
        return false;
    return (g_ignored_groups & SUBGHZ_IGNORE_GROUP_BIT(g)) != 0;
}

void subghz_ignore_group_set(SubGhzIgnoreGroup g, bool ignored)
{
    if (g >= SubGhzIgnoreGroupCount)
        return;
    if (ignored)
        g_ignored_groups |= SUBGHZ_IGNORE_GROUP_BIT(g);
    else
        g_ignored_groups &= ~SUBGHZ_IGNORE_GROUP_BIT(g);
}

void subghz_ignore_group_toggle(SubGhzIgnoreGroup g)
{
    if (g >= SubGhzIgnoreGroupCount)
        return;
    g_ignored_groups ^= SUBGHZ_IGNORE_GROUP_BIT(g);
}

uint16_t subghz_ignore_group_ignored_count(void)
{
    uint16_t total = 0;
    uint32_t v = g_ignored_groups & SUBGHZ_IGNORE_GROUP_ALL_MASK;
    while (v)                       /* Kernighan popcount — portable */
    {
        v &= (v - 1);
        total++;
    }
    return total;
}

const char *subghz_ignore_group_name(SubGhzIgnoreGroup g)
{
    if (g >= SubGhzIgnoreGroupCount)
        return "";
    return k_group_names[g];
}

uint16_t subghz_ignore_group_protocol_count(SubGhzIgnoreGroup g)
{
    if (g >= SubGhzIgnoreGroupCount)
        return 0;

    uint32_t bit = SUBGHZ_IGNORE_GROUP_BIT(g);
    uint16_t n   = subghz_protocol_registry_count;
    if (n > SUBGHZ_IGNORE_MAX_PROTOCOLS)
        n = SUBGHZ_IGNORE_MAX_PROTOCOLS;

    uint16_t total = 0;
    for (uint16_t i = 0; i < n; i++)
        if (subghz_ignore_group_mask_of(i) & bit)
            total++;
    return total;
}

/*============================================================================*/
/* Hex-bitmask serialization                                                  */
/*============================================================================*/

static char hex_digit(uint8_t nibble)
{
    return (nibble < 10) ? (char)('0' + nibble) : (char)('A' + nibble - 10);
}

static int hex_value(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

size_t subghz_ignore_serialize_hex(char *buf, size_t buflen)
{
    if (!buf || buflen < SUBGHZ_IGNORE_HEX_BUFSZ)
        return 0;

    uint32_t v = g_ignored_groups & SUBGHZ_IGNORE_GROUP_ALL_MASK;
    for (int nib = SUBGHZ_IGNORE_HEX_LEN - 1; nib >= 0; nib--)
        buf[SUBGHZ_IGNORE_HEX_LEN - 1 - nib] =
            hex_digit((uint8_t)((v >> (nib * 4)) & 0xF));
    buf[SUBGHZ_IGNORE_HEX_LEN] = '\0';
    return SUBGHZ_IGNORE_HEX_LEN;
}

void subghz_ignore_deserialize_hex(const char *hex)
{
    subghz_ignore_reset();
    if (!hex)
        return;

    while (*hex == ' ' || *hex == '\t')
        hex++;

    uint32_t v = 0;
    while (*hex != '\0')
    {
        int d = hex_value(*hex);
        if (d < 0)
            break;
        v = (v << 4) | (uint32_t)d;   /* low 32 bits retained on overflow */
        hex++;
    }

    g_ignored_groups = v & SUBGHZ_IGNORE_GROUP_ALL_MASK;
}

/*============================================================================*/
/* Name-based query (registry-backed)                                         */
/*============================================================================*/

bool subghz_ignore_is_ignored_name(const char *name)
{
    int16_t idx = subghz_protocol_find_by_name(name);
    if (idx < 0)
        return false;
    return subghz_ignore_is_ignored((uint16_t)idx);
}
