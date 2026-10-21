/* See COPYING.txt for license details. */

/*
 * M1 Sub-GHz decoder for the ProtoPirate "Ford V0" protocol.
 *
 * Decode algorithm derived from RocketGod-git/ProtoPirate
 * protocols/ford_v0.c (GPLv3). The TX encoder remains in
 * Sub_Ghz/subghz_proto_pirate.c; this file implements RX identification only.
 *
 * M1 Project -- Hapax fork
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "m1_sub_ghz_decenc.h"
#include "subghz_manchester_decoder.h"

#define FORD_V0_TE_SHORT 250u
#define FORD_V0_TE_LONG  500u
#define FORD_V0_TE_DELTA 100u
#define FORD_V0_GAP_US   3500u

typedef enum {
    FordV0StepReset = 0,
    FordV0StepPreamble,
    FordV0StepPreambleCheck,
    FordV0StepGap,
    FordV0StepData,
} ford_v0_step_t;

typedef struct {
    ford_v0_step_t step;
    ManchesterState manchester_state;
    uint64_t data_low;
    uint64_t data_high;
    uint8_t bit_count;
    uint16_t header_count;
    uint64_t key1;
} ford_v0_state_t;

static const uint8_t ford_v0_crc_matrix[64] = {
    0xDA, 0xB5, 0x55, 0x6A, 0xAA, 0xAA, 0xAA, 0xD5,
    0xB6, 0x6C, 0xCC, 0xD9, 0x99, 0x99, 0x99, 0xB3,
    0x71, 0xE3, 0xC3, 0xC7, 0x87, 0x87, 0x87, 0x8F,
    0x0F, 0xE0, 0x3F, 0xC0, 0x7F, 0x80, 0x7F, 0x80,
    0x00, 0x1F, 0xFF, 0xC0, 0x00, 0x7F, 0xFF, 0x80,
    0x00, 0x00, 0x00, 0x3F, 0xFF, 0xFF, 0xFF, 0x80,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7F,
    0x23, 0x12, 0x94, 0x84, 0x35, 0xF4, 0x55, 0x84,
};

static uint8_t ford_v0_parity8(uint8_t value)
{
    uint8_t parity = 0;
    while (value)
    {
        parity ^= value & 1u;
        value >>= 1;
    }
    return parity;
}

static uint8_t ford_v0_calculate_crc(const uint8_t *buf)
{
    uint8_t crc = 0;
    for (uint8_t row = 0; row < 8; row++)
    {
        uint8_t xor_sum = 0;
        for (uint8_t col = 0; col < 8; col++)
            xor_sum ^= ford_v0_crc_matrix[row * 8u + col] & buf[col + 1u];
        if (ford_v0_parity8(xor_sum))
            crc |= (uint8_t)(1u << row);
    }
    return crc;
}

bool m1_ford_v0_verify_crc(uint64_t key1, uint16_t key2)
{
    uint8_t buf[16] = {0};
    for (uint8_t i = 0; i < 8; i++)
        buf[i] = (uint8_t)(key1 >> (56u - i * 8u));
    buf[8] = (uint8_t)(key2 >> 8);

    const uint8_t calculated = ford_v0_calculate_crc(buf);
    const uint8_t received = (uint8_t)(key2 & 0xFFu) ^ 0x80u;
    return (calculated & 0x7Fu) == (received & 0x7Fu);
}

void m1_ford_v0_decode_fields(uint64_t key1, uint16_t key2,
                              uint32_t *serial, uint8_t *button,
                              uint32_t *count)
{
    uint8_t buf[13] = {0};
    for (uint8_t i = 0; i < 8; i++)
        buf[i] = (uint8_t)(key1 >> (56u - i * 8u));
    buf[8] = (uint8_t)(key2 >> 8);
    buf[9] = (uint8_t)(key2 & 0xFFu);

    const uint8_t tmp = buf[8];
    const uint8_t parity = ford_v0_parity8(tmp);
    buf[11] = tmp != 0u ? parity : 0u;

    uint8_t xor_byte;
    uint8_t limit;
    if (buf[11])
    {
        xor_byte = buf[7];
        limit = 7;
    }
    else
    {
        xor_byte = buf[6];
        limit = 6;
    }
    for (uint8_t i = 1; i < limit; i++)
        buf[i] ^= xor_byte;
    if (!buf[11])
        buf[7] ^= xor_byte;

    const uint8_t orig_b7 = buf[7];
    buf[7] = (orig_b7 & 0xAAu) | (buf[6] & 0x55u);
    const uint8_t mixed = (buf[6] & 0xAAu) | (orig_b7 & 0x55u);
    buf[12] = mixed;
    buf[6] = mixed;

    const uint32_t serial_le = (uint32_t)buf[1] | ((uint32_t)buf[2] << 8) |
                               ((uint32_t)buf[3] << 16) |
                               ((uint32_t)buf[4] << 24);
    if (serial)
        *serial = ((serial_le & 0xFFu) << 24) |
                  (((serial_le >> 8) & 0xFFu) << 16) |
                  (((serial_le >> 16) & 0xFFu) << 8) |
                  ((serial_le >> 24) & 0xFFu);
    if (button)
        *button = (buf[5] >> 3) & 0x0Fu;
    if (count)
        *count = ((uint32_t)(buf[5] & 0x07u) << 16) |
                 ((uint32_t)buf[6] << 8) | buf[7];
}

static ManchesterEvent ford_v0_event(uint16_t duration, bool level)
{
    if (get_diff(duration, FORD_V0_TE_SHORT) < FORD_V0_TE_DELTA)
        return level ? ManchesterEventShortHigh : ManchesterEventShortLow;
    if (get_diff(duration, FORD_V0_TE_LONG) < FORD_V0_TE_DELTA)
        return level ? ManchesterEventLongHigh : ManchesterEventLongLow;
    return ManchesterEventReset;
}

static void ford_v0_add_bit(ford_v0_state_t *state, bool bit)
{
    const uint32_t low = (uint32_t)state->data_low;
    state->data_low = (state->data_low << 1) | (bit ? 1u : 0u);
    state->data_high = (state->data_high << 1) | ((low >> 31) & 1u);
    state->bit_count++;
}

static bool ford_v0_process_data(ford_v0_state_t *state, uint16_t protocol_index)
{
    if (state->bit_count == 64u)
    {
        const uint64_t combined = (state->data_high << 32) | state->data_low;
        state->key1 = combined;
        state->data_low = 0;
        state->data_high = 0;
        return false;
    }
    if (state->bit_count != 80u)
        return false;

    const uint16_t key2 = (uint16_t)(state->data_low & 0xFFFFu);
    uint32_t serial = 0;
    uint32_t count = 0;
    uint8_t button = 0;
    m1_ford_v0_decode_fields(state->key1, key2, &serial, &button, &count);

    subghz_decenc_ctl.n64_decodedvalue = state->key1;
    subghz_decenc_ctl.n32_serialnumber = serial;
    subghz_decenc_ctl.n32_rollingcode = count;
    subghz_decenc_ctl.n32_key2 = key2;
    subghz_decenc_ctl.n8_buttonid = button;
    subghz_decenc_ctl.ndecodedbitlength = 64;
    subghz_decenc_ctl.ndecodeddelay = 0;
    subghz_decenc_ctl.ndecodedprotocol = protocol_index;
    return true;
}

static void ford_v0_reset(ford_v0_state_t *state)
{
    state->step = FordV0StepReset;
    state->data_low = 0;
    state->data_high = 0;
    state->bit_count = 0;
    state->header_count = 0;
    state->key1 = 0;
    state->manchester_state = ManchesterStateMid1;
}

static bool ford_v0_feed(ford_v0_state_t *state, bool level,
                         uint16_t duration, uint16_t protocol_index)
{
    switch (state->step)
    {
        case FordV0StepReset:
            if (level && get_diff(duration, FORD_V0_TE_SHORT) < FORD_V0_TE_DELTA)
            {
                state->data_low = 0;
                state->data_high = 0;
                state->step = FordV0StepPreamble;
                state->header_count = 0;
                state->bit_count = 0;
                manchester_advance(state->manchester_state, ManchesterEventReset,
                                   &state->manchester_state, NULL);
            }
            break;

        case FordV0StepPreamble:
            if (!level)
            {
                state->step = get_diff(duration, FORD_V0_TE_LONG) < FORD_V0_TE_DELTA
                                  ? FordV0StepPreambleCheck
                                  : FordV0StepReset;
            }
            break;

        case FordV0StepPreambleCheck:
            if (level)
            {
                if (get_diff(duration, FORD_V0_TE_LONG) < FORD_V0_TE_DELTA)
                {
                    state->header_count++;
                    state->step = FordV0StepPreamble;
                }
                else if (get_diff(duration, FORD_V0_TE_SHORT) < FORD_V0_TE_DELTA)
                {
                    state->step = FordV0StepGap;
                }
                else
                {
                    state->step = FordV0StepReset;
                }
            }
            break;

        case FordV0StepGap:
            if (!level && get_diff(duration, FORD_V0_GAP_US) < 250u)
            {
                state->data_low = 1;
                state->data_high = 0;
                state->bit_count = 1;
                state->step = FordV0StepData;
            }
            else if (!level && duration > FORD_V0_GAP_US + 250u)
            {
                state->step = FordV0StepReset;
            }
            break;

        case FordV0StepData:
        {
            const ManchesterEvent event = ford_v0_event(duration, level);
            if (event == ManchesterEventReset)
            {
                state->step = FordV0StepReset;
                break;
            }
            bool bit = false;
            if (manchester_advance(state->manchester_state, event,
                                   &state->manchester_state, &bit))
            {
                ford_v0_add_bit(state, bit);
                if (ford_v0_process_data(state, protocol_index))
                    return true;
            }
            break;
        }
    }
    return false;
}

uint8_t subghz_decode_ford_v0(uint16_t protocol_index, uint16_t pulse_count)
{
    ford_v0_state_t state;
    memset(&state, 0, sizeof(state));
    ford_v0_reset(&state);

    for (uint16_t i = 0; i < pulse_count; i++)
    {
        if (ford_v0_feed(&state, (i & 1u) == 0u,
                         subghz_decenc_ctl.pulse_times[i], protocol_index))
            return 0;
    }
    return 1;
}
