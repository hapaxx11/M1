/* See COPYING.txt for license details. */

/*
 * M1 Sub-GHz decoder for the ProtoPirate "Ford V2" protocol.
 *
 * Frame parsing derived from RocketGod-git/ProtoPirate protocols/ford_v2.c
 * (GPLv3). M1 receives complete pulse windows, so RX locates the 476 µs sync
 * boundary and decodes the following run-length-compressed Manchester cells.
 *
 * M1 Project -- Hapax fork
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "m1_sub_ghz_decenc.h"

#define FORD_V2_TE_SHORT       200u
#define FORD_V2_TE_LONG        400u
#define FORD_V2_TE_DELTA       260u
#define FORD_V2_SYNC_US        476u
#define FORD_V2_PREAMBLE_MIN   64u
#define FORD_V2_DATA_BITS      104u
#define FORD_V2_DATA_BYTES     13u
#define FORD_V2_SYNC_0         0x7Fu
#define FORD_V2_SYNC_1         0xA7u
#define FORD_V2_CELL_CAPACITY  (FORD_V2_DATA_BITS * 2u + 8u)

static bool ford_v2_is_short(uint16_t duration)
{
    return get_diff(duration, FORD_V2_TE_SHORT) < FORD_V2_TE_DELTA;
}

static bool ford_v2_is_long(uint16_t duration)
{
    return get_diff(duration, FORD_V2_TE_LONG) < FORD_V2_TE_DELTA;
}

static bool ford_v2_is_sync(uint16_t duration)
{
    return get_diff(duration, FORD_V2_SYNC_US) < FORD_V2_TE_DELTA;
}

static bool ford_v2_sync_is_nearer(uint16_t duration)
{
    return get_diff(duration, FORD_V2_SYNC_US) <
           get_diff(duration, FORD_V2_TE_SHORT);
}

static bool ford_v2_button_valid(uint8_t button)
{
    return button == 0x10u || button == 0x11u || button == 0x13u ||
           button == 0x14u || button == 0x15u;
}

bool m1_ford_v2_parse_frame(const uint8_t raw[FORD_V2_DATA_BYTES],
                            uint64_t *key, uint64_t *tail_raw,
                            uint32_t *serial, uint16_t *counter,
                            uint32_t *tail31, uint8_t *button)
{
    if (!raw || raw[0] != FORD_V2_SYNC_0 || raw[1] != FORD_V2_SYNC_1 ||
        !ford_v2_button_valid(raw[6]))
        return false;

    const uint16_t cnt = (((uint16_t)(raw[7] & 0x7Fu)) << 9) |
                         ((uint16_t)raw[8] << 1) | (raw[9] >> 7);
    if ((raw[7] & 0x7Fu) != (uint8_t)((cnt >> 9) & 0x7Fu) ||
        raw[8] != (uint8_t)((cnt >> 1) & 0xFFu) ||
        (raw[9] >> 7) != (uint8_t)(cnt & 1u))
        return false;

    uint64_t key_value = 0;
    uint64_t tail_value = 0;
    for (uint8_t i = 0; i < 8; i++)
        key_value = (key_value << 8) | raw[i];
    for (uint8_t i = 0; i < 5; i++)
        tail_value = (tail_value << 8) | raw[8u + i];

    if (key) *key = key_value;
    if (tail_raw) *tail_raw = tail_value;
    if (serial)
        *serial = ((uint32_t)raw[2] << 24) | ((uint32_t)raw[3] << 16) |
                  ((uint32_t)raw[4] << 8) | raw[5];
    if (counter) *counter = cnt;
    if (tail31)
        *tail31 = (((uint32_t)(raw[9] & 0x7Fu)) << 24) |
                  ((uint32_t)raw[10] << 16) | ((uint32_t)raw[11] << 8) |
                  raw[12];
    if (button) *button = raw[6];
    return true;
}

static bool ford_v2_commit(const uint8_t raw[FORD_V2_DATA_BYTES],
                           uint16_t protocol_index)
{
    uint64_t key = 0;
    uint64_t tail_raw = 0;
    uint32_t serial = 0;
    uint32_t tail31 = 0;
    uint16_t counter = 0;
    uint8_t button = 0;
    if (!m1_ford_v2_parse_frame(raw, &key, &tail_raw, &serial,
                                &counter, &tail31, &button))
        return false;

    subghz_decenc_ctl.n64_decodedvalue = key;
    subghz_decenc_ctl.n32_serialnumber = serial;
    subghz_decenc_ctl.n32_rollingcode = counter;
    subghz_decenc_ctl.n32_key2 = tail_raw;
    subghz_decenc_ctl.n8_buttonid = button;
    subghz_decenc_ctl.ndecodedbitlength = FORD_V2_DATA_BITS;
    subghz_decenc_ctl.ndecodeddelay = 0;
    subghz_decenc_ctl.ndecodedprotocol = protocol_index;
    return true;
}

static bool ford_v2_try_cells(const uint8_t *cells, uint16_t cell_count,
                              uint16_t protocol_index, bool invert)
{
    uint8_t raw[FORD_V2_DATA_BYTES] = {0};

    for (uint16_t bit_pos = 1; bit_pos < FORD_V2_DATA_BITS; bit_pos++)
    {
        const uint16_t offset = (bit_pos - 1u) * 2u + 1u;
        if (offset + 1u >= cell_count)
            return false;
        const uint8_t first = cells[offset];
        const uint8_t second = cells[offset + 1u];
        if (first == second)
            return false;

        bool value = first != 0u;
        if (invert)
            value = !value;
        if (value)
            raw[bit_pos >> 3] |= (uint8_t)(1u << (7u - (bit_pos & 7u)));
    }

    return ford_v2_commit(raw, protocol_index);
}

uint8_t subghz_decode_ford_v2(uint16_t protocol_index, uint16_t pulse_count)
{
    uint16_t preamble_count = 0;
    for (uint16_t sync = 0; sync < pulse_count; sync++)
    {
        if (ford_v2_is_short(subghz_decenc_ctl.pulse_times[sync]) &&
            !(preamble_count >= FORD_V2_PREAMBLE_MIN &&
              ford_v2_is_sync(subghz_decenc_ctl.pulse_times[sync]) &&
              ford_v2_sync_is_nearer(subghz_decenc_ctl.pulse_times[sync])))
        {
            if (preamble_count < UINT16_MAX)
                preamble_count++;
            continue;
        }
        if (preamble_count < FORD_V2_PREAMBLE_MIN ||
            (sync & 1u) == 0u ||
            !ford_v2_is_sync(subghz_decenc_ctl.pulse_times[sync]))
        {
            preamble_count = 0;
            continue;
        }

        uint8_t cells[FORD_V2_CELL_CAPACITY];
        uint16_t cell_count = 0;
        for (uint16_t i = sync + 1u; i < pulse_count; i++)
        {
            const uint16_t duration = subghz_decenc_ctl.pulse_times[i];
            uint8_t push;
            if (ford_v2_is_long(duration) &&
                (!ford_v2_is_short(duration) ||
                 get_diff(duration, FORD_V2_TE_LONG) <=
                     get_diff(duration, FORD_V2_TE_SHORT)))
                push = 2;
            else if (ford_v2_is_short(duration))
                push = 1;
            else
                break;

            for (uint8_t cell = 0; cell < push && cell_count < FORD_V2_CELL_CAPACITY; cell++)
                cells[cell_count++] = (uint8_t)((i & 1u) == 0u);
        }

        if (ford_v2_try_cells(cells, cell_count, protocol_index, false) ||
            ford_v2_try_cells(cells, cell_count, protocol_index, true))
            return 0;
        preamble_count = 0;
    }
    return 1;
}
