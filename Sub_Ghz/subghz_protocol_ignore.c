/* SPDX-License-Identifier: GPL-2.0-or-later */
/* See COPYING.txt for license details. */

/*
 * subghz_protocol_ignore.c
 *
 * Implementation of the Sub-GHz protocol ignore-list.  See
 * subghz_protocol_ignore.h for the rationale and API contract.
 *
 * M1 Project — Hapax fork
 */

#include "subghz_protocol_ignore.h"
#include "subghz_protocol_registry.h"

/*============================================================================*/
/* Internal state                                                             */
/*============================================================================*/

#define IGNORE_WORD_BITS   32
#define IGNORE_WORD_COUNT  (SUBGHZ_IGNORE_MAX_PROTOCOLS / IGNORE_WORD_BITS)

/* One bit per protocol index; bit set == ignored.  Word 0 holds indices
 * 0..31, word 1 holds 32..63, and so on. */
static uint32_t g_ignore_bits[IGNORE_WORD_COUNT];

/*============================================================================*/
/* Core bitset API                                                            */
/*============================================================================*/

void subghz_ignore_reset(void)
{
    for (unsigned w = 0; w < IGNORE_WORD_COUNT; w++)
        g_ignore_bits[w] = 0;
}

bool subghz_ignore_is_ignored(uint16_t index)
{
    if (index >= SUBGHZ_IGNORE_MAX_PROTOCOLS)
        return false;
    return (g_ignore_bits[index / IGNORE_WORD_BITS] &
            (1u << (index % IGNORE_WORD_BITS))) != 0;
}

void subghz_ignore_set(uint16_t index, bool ignored)
{
    if (index >= SUBGHZ_IGNORE_MAX_PROTOCOLS)
        return;
    uint32_t mask = (1u << (index % IGNORE_WORD_BITS));
    if (ignored)
        g_ignore_bits[index / IGNORE_WORD_BITS] |= mask;
    else
        g_ignore_bits[index / IGNORE_WORD_BITS] &= ~mask;
}

void subghz_ignore_toggle(uint16_t index)
{
    if (index >= SUBGHZ_IGNORE_MAX_PROTOCOLS)
        return;
    g_ignore_bits[index / IGNORE_WORD_BITS] ^=
        (1u << (index % IGNORE_WORD_BITS));
}

uint16_t subghz_ignore_count(void)
{
    uint16_t total = 0;
    for (unsigned w = 0; w < IGNORE_WORD_COUNT; w++)
    {
        uint32_t v = g_ignore_bits[w];
        while (v)                       /* Kernighan popcount — portable */
        {
            v &= (v - 1);
            total++;
        }
    }
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

    size_t pos = 0;
    /* Emit most-significant word first so the string reads like a big
     * integer (highest protocol indices on the left). */
    for (int w = IGNORE_WORD_COUNT - 1; w >= 0; w--)
    {
        uint32_t word = g_ignore_bits[w];
        for (int nib = (IGNORE_WORD_BITS / 4) - 1; nib >= 0; nib--)
            buf[pos++] = hex_digit((uint8_t)((word >> (nib * 4)) & 0xF));
    }
    buf[pos] = '\0';
    return pos;
}

void subghz_ignore_deserialize_hex(const char *hex)
{
    subghz_ignore_reset();
    if (!hex)
        return;

    /* Skip leading whitespace. */
    while (*hex == ' ' || *hex == '\t')
        hex++;

    /* Count the run of hex characters so we can right-align the value into
     * the bit words (the serializer is fixed-width, but be tolerant of
     * shorter/longer strings). */
    size_t len = 0;
    while (hex[len] != '\0' && hex_value(hex[len]) >= 0)
        len++;

    /* Walk from the least-significant nibble (rightmost char) upward. */
    for (size_t i = 0; i < len; i++)
    {
        char c = hex[len - 1 - i];
        int v = hex_value(c);
        if (v < 0)
            break;
        uint16_t base_bit = (uint16_t)(i * 4);
        for (int b = 0; b < 4; b++)
        {
            if (v & (1 << b))
                subghz_ignore_set((uint16_t)(base_bit + b), true);
        }
    }
}

/*============================================================================*/
/* Name-based helpers (registry-backed)                                       */
/*============================================================================*/

bool subghz_ignore_is_ignored_name(const char *name)
{
    int16_t idx = subghz_protocol_find_by_name(name);
    if (idx < 0)
        return false;
    return subghz_ignore_is_ignored((uint16_t)idx);
}

bool subghz_ignore_set_name(const char *name, bool ignored)
{
    int16_t idx = subghz_protocol_find_by_name(name);
    if (idx < 0)
        return false;
    subghz_ignore_set((uint16_t)idx, ignored);
    return true;
}
