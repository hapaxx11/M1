/* See COPYING.txt for license details. */

/*
 * M1 Sub-GHz decoder for the ProtoPirate "Ford V1" protocol.
 *
 * Decode algorithm derived from RocketGod-git/ProtoPirate
 * protocols/ford_v1.c (GPLv3). The TX encoder remains in
 * Sub_Ghz/subghz_proto_pirate.c; this file implements RX identification only.
 *
 * M1 Project -- Hapax fork
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "m1_sub_ghz_decenc.h"
#include "subghz_manchester_decoder.h"

#define FORD_V1_TE_SHORT        65u
#define FORD_V1_TE_LONG         130u
#define FORD_V1_DELTA_LONG      40u
#define FORD_V1_DELTA_DATASYNC  39u
#define FORD_V1_PREAMBLE_MIN    50u
#define FORD_V1_DATA_BITS       136u
#define FORD_V1_DATA_BYTES      17u

typedef enum {
    FordV1StepReset = 0,
    FordV1StepPreamble,
    FordV1StepSync,
    FordV1StepData,
} ford_v1_step_t;

typedef struct {
    ford_v1_step_t step;
    ManchesterState manchester_state;
    uint32_t decode_data;
    uint8_t decode_count_bit;
    uint8_t raw[FORD_V1_DATA_BYTES + 1u];
    uint8_t byte_count;
    uint16_t preamble_count;
    uint8_t sync_event_idx;
    uint8_t sync_event_count;
    uint8_t sync_events[8];
} ford_v1_state_t;

static uint16_t ford_v1_crc16(const uint8_t *data, size_t len)
{
    uint16_t remainder = 0;
    for (size_t byte = 0; byte < len; byte++)
    {
        remainder ^= (uint16_t)data[byte] << 8;
        for (uint8_t bit = 0; bit < 8; bit++)
        {
            remainder = (remainder & 0x8000u)
                            ? (uint16_t)((remainder << 1) ^ 0x1021u)
                            : (uint16_t)(remainder << 1);
        }
    }
    return remainder;
}

static void ford_v1_shift_right_one(uint8_t raw[FORD_V1_DATA_BYTES])
{
    uint8_t carry = 0;
    for (uint8_t i = 0; i < FORD_V1_DATA_BYTES; i++)
    {
        const uint8_t next = raw[i] & 1u;
        raw[i] = (uint8_t)((raw[i] >> 1) | (carry << 7));
        carry = next;
    }
}

static void ford_v1_decode_with_flag(uint8_t *raw, uint8_t flag)
{
    if (flag)
    {
        const uint8_t xor_byte = raw[7];
        for (uint8_t i = 1; i < 7; i++)
            raw[i] ^= xor_byte;
    }
    else
    {
        const uint8_t xor_byte = raw[6];
        for (uint8_t i = 1; i < 6; i++)
            raw[i] ^= xor_byte;
        raw[7] ^= xor_byte;
    }

    const uint8_t b6 = raw[6];
    const uint8_t b7 = raw[7];
    raw[6] = (b6 & 0xAAu) | (b7 & 0x55u);
    raw[7] = (b7 & 0xAAu) | (b6 & 0x55u);
}

static void ford_v1_encode_inverse_block(uint8_t block[9])
{
    uint8_t sum = 0;
    for (uint8_t i = 1; i <= 7; i++)
        sum = (uint8_t)(sum + block[i]);

    const uint8_t p6 = block[6];
    const uint8_t p7 = block[7];
    const uint8_t post6 = (p6 & 0xAAu) | (p7 & 0x55u);
    const uint8_t post7 = (p7 & 0xAAu) | (p6 & 0x55u);
    const uint8_t xorv = post6 ^ post7;

    uint8_t xor_byte;
    if ((__builtin_popcount((unsigned int)sum) & 1u) != 0u)
    {
        block[6] = xorv;
        block[7] = post7;
        xor_byte = post7;
    }
    else
    {
        block[6] = post6;
        block[7] = xorv;
        xor_byte = post6;
    }
    for (uint8_t i = 1; i <= 5; i++)
        block[i] ^= xor_byte;
}

static void ford_v1_encode_air(const uint8_t plain[9], uint8_t air[9])
{
    memcpy(air, plain, 9);
    ford_v1_encode_inverse_block(air);
}

static bool ford_v1_plain_from_air(const uint8_t air[9], uint8_t plain[9])
{
    for (uint8_t flag = 0; flag < 2; flag++)
    {
        uint8_t candidate[9];
        memcpy(candidate, air, 9);
        ford_v1_decode_with_flag(candidate, flag);
        uint8_t reencoded[9];
        ford_v1_encode_air(candidate, reencoded);
        if (memcmp(reencoded, air, 9) == 0)
        {
            memcpy(plain, candidate, 9);
            return true;
        }
    }
    return false;
}

static void ford_v1_fields_from_plain(const uint8_t plain[9], uint32_t *serial,
                                      uint8_t *button, uint32_t *count)
{
    *serial = ((uint32_t)plain[1] << 24) | ((uint32_t)plain[2] << 16) |
              ((uint32_t)plain[3] << 8) | plain[0];
    *button = (plain[5] >> 4) & 0x0Fu;
    *count = ((uint32_t)(plain[5] & 0x0Fu) << 16) |
             ((uint32_t)plain[6] << 8) | plain[7];
}

bool m1_ford_v1_parse_frame(const uint8_t raw[FORD_V1_DATA_BYTES],
                            uint64_t *key1, uint64_t *key2,
                            uint32_t *serial, uint8_t *button,
                            uint32_t *count, uint16_t *crc,
                            bool *fields_valid)
{
    if (!raw)
        return false;

    uint8_t canonical[FORD_V1_DATA_BYTES];
    memcpy(canonical, raw, sizeof(canonical));
    uint16_t calculated = ford_v1_crc16(&canonical[3], 12);
    uint16_t received = ((uint16_t)canonical[15] << 8) | canonical[16];
    if (received != calculated)
    {
        for (size_t i = 0; i < sizeof(canonical); i++)
            canonical[i] = (uint8_t)~canonical[i];
        calculated = ford_v1_crc16(&canonical[3], 12);
        received = ((uint16_t)canonical[15] << 8) | canonical[16];
    }
    if (received != calculated)
        return false;

    uint64_t primary = 0;
    uint64_t secondary = 0;
    for (uint8_t i = 0; i < 7; i++)
        primary = (primary << 8) | canonical[i];
    for (uint8_t i = 0; i < 8; i++)
        secondary = (secondary << 8) | canonical[7u + i];
    if (key1) *key1 = primary;
    if (key2) *key2 = secondary;
    if (crc) *crc = received;

    const uint8_t *air = &canonical[6];
    uint8_t decoded_b0[9];
    uint8_t decoded_b1[9];
    memcpy(decoded_b0, air, 9);
    memcpy(decoded_b1, air, 9);
    ford_v1_decode_with_flag(decoded_b0, 0);
    ford_v1_decode_with_flag(decoded_b1, 1);

    const uint8_t *plain = NULL;
    uint8_t roundtrip[9];
    bool strict = false;
    if ((decoded_b0[3] == canonical[5] && decoded_b0[4] == canonical[6]) ||
        (decoded_b1[3] == canonical[5] && decoded_b1[4] == canonical[6]))
    {
        plain = (decoded_b0[3] == canonical[5] && decoded_b0[4] == canonical[6])
                    ? decoded_b0
                    : decoded_b1;
        strict = true;
    }
    else if (ford_v1_plain_from_air(air, roundtrip))
    {
        plain = roundtrip;
    }

    if (strict)
    {
        ford_v1_fields_from_plain(plain, serial, button, count);
    }
    else
    {
        if (serial)
            *serial = ((uint32_t)canonical[3] << 24) |
                      ((uint32_t)canonical[4] << 16) |
                      ((uint32_t)canonical[5] << 8) | canonical[6];
        if (button) *button = 0;
        if (count) *count = 0;
    }
    if (fields_valid) *fields_valid = strict;
    return true;
}

static void ford_v1_reset(ford_v1_state_t *state)
{
    state->step = FordV1StepReset;
    state->decode_data = 0;
    state->decode_count_bit = 0;
    state->byte_count = 0;
    memset(state->raw, 0, sizeof(state->raw));
    state->preamble_count = 0;
    state->sync_event_idx = 0;
    state->sync_event_count = 0;
    memset(state->sync_events, 0, sizeof(state->sync_events));
    manchester_advance(state->manchester_state, ManchesterEventReset,
                       &state->manchester_state, NULL);
}

static bool ford_v1_commit(ford_v1_state_t *state, uint16_t protocol_index)
{
    uint64_t key1 = 0;
    uint64_t key2 = 0;
    uint32_t serial = 0;
    uint32_t count = 0;
    uint16_t crc = 0;
    uint8_t button = 0;
    bool fields_valid = false;
    uint8_t shifted[FORD_V1_DATA_BYTES];
    memcpy(shifted, state->raw, sizeof(shifted));
    ford_v1_shift_right_one(shifted);
    if (!m1_ford_v1_parse_frame(shifted, &key1, &key2, &serial,
                                &button, &count, &crc, &fields_valid) &&
        !m1_ford_v1_parse_frame(state->raw, &key1, &key2, &serial,
                                &button, &count, &crc, &fields_valid))
        return false;

    (void)fields_valid;
    subghz_decenc_ctl.n64_decodedvalue = key1;
    subghz_decenc_ctl.n32_serialnumber = serial;
    subghz_decenc_ctl.n32_rollingcode = count;
    subghz_decenc_ctl.n32_key2 = key2;
    subghz_decenc_ctl.n8_buttonid = button;
    subghz_decenc_ctl.ndecodedbitlength = FORD_V1_DATA_BITS;
    subghz_decenc_ctl.ndecodeddelay = 0;
    subghz_decenc_ctl.ndecodedprotocol = protocol_index;
    return true;
}

static bool ford_v1_try_last_byte(ford_v1_state_t *state, uint16_t protocol_index)
{
    if (state->byte_count == 16u && state->decode_count_bit <= FORD_V1_DATA_BITS)
    {
        const uint8_t shift = FORD_V1_DATA_BITS - state->decode_count_bit;
        if (shift <= 7u)
        {
            const uint8_t variants = (uint8_t)(1u << shift);
            uint8_t saved[16];
            memcpy(saved, state->raw, sizeof(saved));
            const uint32_t partial = state->decode_data;
            for (uint8_t variant = 0; variant < variants; variant++)
            {
                memcpy(state->raw, saved, sizeof(saved));
                state->raw[16] = (uint8_t)(((uint8_t)partial << shift) | variant);
                if (ford_v1_commit(state, protocol_index))
                {
                    ford_v1_reset(state);
                    return true;
                }
            }
        }
    }
    ford_v1_reset(state);
    return false;
}

static bool ford_v1_feed(ford_v1_state_t *state, bool level,
                         uint16_t duration, uint16_t protocol_index)
{
    switch (state->step)
    {
        case FordV1StepReset:
            if (!level && get_diff(duration, FORD_V1_TE_LONG) < FORD_V1_DELTA_LONG)
            {
                state->step = FordV1StepPreamble;
                state->preamble_count = 1;
            }
            break;

        case FordV1StepPreamble:
            if (get_diff(duration, FORD_V1_TE_LONG) < FORD_V1_DELTA_LONG)
            {
                state->preamble_count++;
            }
            else if (get_diff(duration, FORD_V1_TE_SHORT) < FORD_V1_DELTA_DATASYNC)
            {
                if (state->preamble_count >= FORD_V1_PREAMBLE_MIN)
                {
                    state->sync_event_idx = 0;
                    state->sync_event_count = 1;
                    state->sync_events[0] = level ? ManchesterEventShortHigh
                                                  : ManchesterEventShortLow;
                    state->step = FordV1StepSync;
                }
                else
                {
                    state->step = FordV1StepReset;
                }
            }
            else if (state->preamble_count < FORD_V1_PREAMBLE_MIN)
            {
                state->step = FordV1StepReset;
            }
            break;

        case FordV1StepSync:
        {
            ManchesterEvent event;
            bool is_short = false;
            if (get_diff(duration, FORD_V1_TE_SHORT) < FORD_V1_DELTA_DATASYNC)
            {
                event = level ? ManchesterEventShortHigh : ManchesterEventShortLow;
                is_short = true;
            }
            else if (get_diff(duration, FORD_V1_TE_LONG) < FORD_V1_DELTA_DATASYNC)
            {
                event = level ? ManchesterEventLongHigh : ManchesterEventLongLow;
            }
            else
            {
                state->step = FordV1StepPreamble;
                break;
            }

            state->sync_event_idx++;
            if (is_short)
                state->sync_event_count++;
            if (state->sync_event_idx < 8u)
                state->sync_events[state->sync_event_idx] = event;

            if (state->sync_event_count > 2u)
            {
                state->decode_data = 0;
                state->decode_count_bit = 0;
                state->byte_count = 0;
                memset(state->raw, 0, sizeof(state->raw));
                manchester_advance(state->manchester_state, ManchesterEventReset,
                                   &state->manchester_state, NULL);
                if (state->sync_events[0] == ManchesterEventShortLow)
                    state->manchester_state = ManchesterStateMid0;
                state->step = FordV1StepData;

                for (uint8_t i = 0; i <= state->sync_event_idx && i < 8u; i++)
                {
                    bool bit = false;
                    if (manchester_advance(state->manchester_state,
                                           (ManchesterEvent)state->sync_events[i],
                                           &state->manchester_state, &bit))
                    {
                        bit = !bit;
                        state->decode_data = (state->decode_data << 1) | (bit ? 1u : 0u);
                        state->decode_count_bit++;
                        if ((state->decode_count_bit & 7u) == 0u)
                        {
                            if (state->byte_count < FORD_V1_DATA_BYTES)
                                state->raw[state->byte_count++] =
                                    (uint8_t)state->decode_data;
                            state->decode_data = 0;
                        }
                    }
                }
            }
            else if (state->sync_event_idx >= 7u)
            {
                state->step = FordV1StepPreamble;
            }
            break;
        }

        case FordV1StepData:
        {
            ManchesterEvent event;
            if (get_diff(duration, FORD_V1_TE_SHORT) < FORD_V1_DELTA_DATASYNC)
                event = level ? ManchesterEventShortHigh : ManchesterEventShortLow;
            else if (get_diff(duration, FORD_V1_TE_LONG) < FORD_V1_DELTA_DATASYNC)
                event = level ? ManchesterEventLongHigh : ManchesterEventLongLow;
            else
                return ford_v1_try_last_byte(state, protocol_index);

            bool bit = false;
            if (manchester_advance(state->manchester_state, event,
                                   &state->manchester_state, &bit))
            {
                bit = !bit;
                state->decode_data = (state->decode_data << 1) | (bit ? 1u : 0u);
                state->decode_count_bit++;
                if ((state->decode_count_bit & 7u) == 0u)
                {
                    if (state->byte_count < FORD_V1_DATA_BYTES)
                        state->raw[state->byte_count++] = (uint8_t)state->decode_data;
                    state->decode_data = 0;
                    if (state->byte_count > 16u)
                    {
                        const bool decoded = ford_v1_commit(state, protocol_index);
                        ford_v1_reset(state);
                        return decoded;
                    }
                }
            }
            break;
        }
    }
    return false;
}

uint8_t subghz_decode_ford_v1(uint16_t protocol_index, uint16_t pulse_count)
{
    ford_v1_state_t state;
    memset(&state, 0, sizeof(state));
    ford_v1_reset(&state);

    for (uint16_t i = 0; i < pulse_count; i++)
    {
        if (ford_v1_feed(&state, (i & 1u) == 0u,
                         subghz_decenc_ctl.pulse_times[i], protocol_index))
            return 0;
    }
    return 1;
}
