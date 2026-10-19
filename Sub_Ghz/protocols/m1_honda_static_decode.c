/* See COPYING.txt for license details. */

/*
 * M1 Sub-GHz decoder for the ProtoPirate "Honda Static" protocol.
 *
 * Decode algorithm derived from RocketGod-git/ProtoPirate
 * protocols/honda_static.c (GPLv3). The TX encoder remains in
 * Sub_Ghz/subghz_proto_pirate.c; this file implements RX identification only.
 *
 * M1 Project -- Hapax fork
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "m1_sub_ghz_decenc.h"

#define HONDA_STATIC_BIT_COUNT       64u
#define HONDA_STATIC_MIN_SYMBOLS     36u
#define HONDA_STATIC_SHORT_BASE_US   28u
#define HONDA_STATIC_SHORT_SPAN_US   70u
#define HONDA_STATIC_LONG_BASE_US    61u
#define HONDA_STATIC_LONG_SPAN_US    130u
#define HONDA_STATIC_SYMBOL_CAPACITY 512u
#define HONDA_STATIC_SYMBOL_BYTES    (HONDA_STATIC_SYMBOL_CAPACITY / 8u)
#define HONDA_STATIC_PREAMBLE_MAX_TRANSITIONS 19u

typedef struct {
    uint32_t serial;
    uint32_t counter;
    uint8_t button;
    uint8_t checksum;
} honda_static_fields_t;

typedef struct {
    uint16_t count;
    uint8_t bits[HONDA_STATIC_SYMBOL_BYTES];
} honda_static_symbols_t;

static uint32_t honda_static_get_bits(const uint8_t *data, uint8_t start,
                                      uint8_t count)
{
    uint32_t value = 0;
    for (uint8_t i = 0; i < count; i++)
    {
        const uint8_t index = start + i;
        value = (value << 1) |
                ((data[index >> 3] >> (((uint8_t)~index) & 7u)) & 1u);
    }
    return value;
}

static uint8_t honda_static_reverse8(uint8_t value)
{
    value = (uint8_t)((value >> 4) | (value << 4));
    value = (uint8_t)(((value & 0xCCu) >> 2) | ((value & 0x33u) << 2));
    return (uint8_t)(((value & 0xAAu) >> 1) | ((value & 0x55u) << 1));
}

static bool honda_static_button_valid(uint8_t button)
{
    return button <= 9u && ((0x336u >> button) & 1u) != 0u;
}

static bool honda_static_serial_valid(uint32_t serial)
{
    return serial != 0u && serial != 0x0FFFFFFFu;
}

bool m1_honda_static_parse_packet(const uint8_t packet[9], bool reverse,
                                  uint32_t *serial, uint32_t *counter,
                                  uint8_t *button, uint8_t *checksum)
{
    if (!packet)
        return false;

    uint8_t normalized[9];
    memcpy(normalized, packet, sizeof(normalized));
    if (reverse)
    {
        for (size_t i = 0; i < sizeof(normalized); i++)
            normalized[i] = honda_static_reverse8(normalized[i]);
    }

    const uint8_t btn = (uint8_t)honda_static_get_bits(normalized, 0, 4);
    const uint32_t sn = honda_static_get_bits(normalized, 4, 28);
    const uint32_t cnt = honda_static_get_bits(normalized, 32, 24);
    uint8_t crc = 0;
    for (size_t i = 0; i < 7; i++)
        crc ^= normalized[i];

    if (!reverse && crc != honda_static_get_bits(normalized, 56, 8))
        return false;
    if (!honda_static_button_valid(btn) || !honda_static_serial_valid(sn))
        return false;

    if (serial) *serial = sn;
    if (counter) *counter = cnt;
    if (button) *button = btn;
    if (checksum) *checksum = crc;
    return true;
}

static uint64_t honda_static_compact_key(const honda_static_fields_t *fields)
{
    return ((uint64_t)(fields->button & 0x0Fu) << 56) |
           ((uint64_t)fields->serial << 24) | fields->counter;
}

static void honda_static_symbol_set(honda_static_symbols_t *symbols,
                                    uint16_t index, bool value)
{
    const uint8_t byte = index >> 3;
    const uint8_t mask = (uint8_t)(1u << (((uint8_t)~index) & 7u));
    if (value)
        symbols->bits[byte] |= mask;
    else
        symbols->bits[byte] &= (uint8_t)~mask;
}

static bool honda_static_symbol_get(const honda_static_symbols_t *symbols,
                                    uint16_t index)
{
    return (symbols->bits[index >> 3] >> (((uint8_t)~index) & 7u)) & 1u;
}

static bool honda_static_manchester_pack(
    const honda_static_symbols_t *symbols, uint16_t start, bool inverted,
    uint8_t packet[9])
{
    memset(packet, 0, 9);
    uint16_t pos = start;
    uint8_t bit_count = 0;

    while ((uint16_t)(pos + 1u) < symbols->count && bit_count < 64u)
    {
        const bool a = honda_static_symbol_get(symbols, pos);
        const bool b = honda_static_symbol_get(symbols, pos + 1u);
        if (a == b)
        {
            pos++;
            continue;
        }

        const bool bit = inverted ? (!a && b) : (a && !b);
        if (bit)
            packet[bit_count >> 3] |=
                (uint8_t)(1u << (((uint8_t)~bit_count) & 7u));
        bit_count++;
        pos += 2u;
    }
    return bit_count >= HONDA_STATIC_BIT_COUNT;
}

static bool honda_static_parse_symbols(const honda_static_symbols_t *symbols,
                                       bool inverted,
                                       honda_static_fields_t *fields,
                                       uint64_t *key)
{
    uint16_t index = 1;
    uint16_t transitions = 0;
    while (index < symbols->count)
    {
        if (honda_static_symbol_get(symbols, index) !=
            honda_static_symbol_get(symbols, index - 1u))
        {
            transitions++;
        }
        else
        {
            if (transitions > HONDA_STATIC_PREAMBLE_MAX_TRANSITIONS)
                break;
            transitions = 0;
        }
        index++;
    }
    if (index >= symbols->count)
        return false;

    while ((uint16_t)(index + 1u) < symbols->count &&
           honda_static_symbol_get(symbols, index) ==
               honda_static_symbol_get(symbols, index + 1u))
    {
        index++;
    }

    uint8_t packet[9];
    if (!honda_static_manchester_pack(symbols, index, inverted, packet))
        return false;

    honda_static_fields_t decoded;
    memset(&decoded, 0, sizeof(decoded));
    if (!m1_honda_static_parse_packet(
            packet, false, &decoded.serial, &decoded.counter,
            &decoded.button, &decoded.checksum))
    {
        if (inverted ||
            !m1_honda_static_parse_packet(
                packet, true, &decoded.serial, &decoded.counter,
                &decoded.button, &decoded.checksum))
            return false;
    }

    *fields = decoded;
    *key = honda_static_compact_key(&decoded);
    return true;
}

static bool honda_static_try_decode(const honda_static_symbols_t *symbols,
                                    uint16_t protocol_index)
{
    if (symbols->count < HONDA_STATIC_MIN_SYMBOLS)
        return false;

    honda_static_fields_t fields;
    uint64_t key = 0;
    if (!honda_static_parse_symbols(symbols, true, &fields, &key) &&
        !honda_static_parse_symbols(symbols, false, &fields, &key))
        return false;

    subghz_decenc_ctl.n64_decodedvalue = key;
    subghz_decenc_ctl.n32_serialnumber = fields.serial;
    subghz_decenc_ctl.n32_rollingcode = fields.counter;
    subghz_decenc_ctl.n32_key2 = fields.checksum;
    subghz_decenc_ctl.n8_buttonid = fields.button;
    subghz_decenc_ctl.ndecodedbitlength = HONDA_STATIC_BIT_COUNT;
    subghz_decenc_ctl.ndecodeddelay = 0;
    subghz_decenc_ctl.ndecodedprotocol = protocol_index;
    return true;
}

uint8_t subghz_decode_honda_static(uint16_t protocol_index, uint16_t pulse_count)
{
    honda_static_symbols_t symbols;
    memset(&symbols, 0, sizeof(symbols));

    for (uint16_t i = 0; i < pulse_count; i++)
    {
        const uint16_t duration = subghz_decenc_ctl.pulse_times[i];
        const bool level = (i & 1u) == 0u;
        uint8_t cells = 0;

        if (duration >= HONDA_STATIC_SHORT_BASE_US &&
            duration - HONDA_STATIC_SHORT_BASE_US <= HONDA_STATIC_SHORT_SPAN_US)
        {
            cells = 1;
        }
        else if (duration >= HONDA_STATIC_LONG_BASE_US &&
                 duration - HONDA_STATIC_LONG_BASE_US <= HONDA_STATIC_LONG_SPAN_US)
        {
            cells = 2;
        }
        else
        {
            if (honda_static_try_decode(&symbols, protocol_index))
                return 0;
            symbols.count = 0;
            continue;
        }

        for (uint8_t cell = 0; cell < cells; cell++)
        {
            if (symbols.count < HONDA_STATIC_SYMBOL_CAPACITY)
            {
                honda_static_symbol_set(&symbols, symbols.count, level);
                symbols.count++;
            }
        }
    }

    return honda_static_try_decode(&symbols, protocol_index) ? 0 : 1;
}
