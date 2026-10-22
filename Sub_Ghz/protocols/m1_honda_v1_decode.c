/* See COPYING.txt for license details. */

/*
 * M1 Sub-GHz decoder for the ProtoPirate "Honda V1" protocol.
 *
 * Decode algorithm derived from RocketGod-git/ProtoPirate
 * protocols/honda_v1.c (GPLv3). The TX encoder remains in
 * Sub_Ghz/subghz_proto_pirate.c; this file implements RX identification only.
 *
 * M1 Project -- Hapax fork
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "m1_sub_ghz_decenc.h"

#define HONDA_V1_BIT_COUNT       68u
#define HONDA_V1_MAX_BIT_COUNT   76u
#define HONDA_V1_BUFFER_BYTES    12u
#define HONDA_V1_TE_SHORT        1000u
#define HONDA_V1_TE_LONG         2000u
#define HONDA_V1_TE_DELTA        400u
#define HONDA_V1_TE_SHORT_MIN    600u
#define HONDA_V1_TE_END          3500u

typedef enum {
    HondaV1StepReset = 0,
    HondaV1StepPreamble,
    HondaV1StepData,
} honda_v1_step_t;

typedef struct {
    honda_v1_step_t step;
    uint8_t preamble_count;
    bool preamble_has_long;
    bool data_pending;
    bool last_level;
    uint8_t bits[HONDA_V1_BUFFER_BYTES];
    uint8_t bit_count;
    uint32_t pending;
    bool pending_valid;
} honda_v1_state_t;

static bool honda_v1_button_valid(uint8_t button)
{
    return button <= 10u && ((0x701u >> button) & 1u) != 0u;
}

static bool honda_v1_duration_is(uint32_t duration, uint32_t target)
{
    return duration >= target ? duration - target <= HONDA_V1_TE_DELTA
                              : target - duration <= HONDA_V1_TE_DELTA;
}

bool m1_honda_v1_parse_frame(const uint8_t *bits, uint8_t bit_count,
                             uint64_t *key, uint32_t *serial,
                             uint16_t *counter, uint8_t *button, uint8_t *crc)
{
    if (!bits || bit_count < HONDA_V1_BIT_COUNT ||
        bit_count > HONDA_V1_MAX_BIT_COUNT)
        return false;

    uint8_t aligned[HONDA_V1_BUFFER_BYTES];
    memcpy(aligned, bits, sizeof(aligned));
    uint8_t shift_count = bit_count - HONDA_V1_BIT_COUNT;
    if (shift_count < 1u)
        shift_count = 1u;

    for (uint8_t shift = 0; shift < shift_count; shift++)
    {
        for (size_t i = 0; i < sizeof(aligned) - 1u; i++)
            aligned[i] = (uint8_t)((aligned[i] << 1) | (aligned[i + 1u] >> 7));
        aligned[sizeof(aligned) - 1u] <<= 1;
    }

    const uint8_t btn = aligned[4] >> 4;
    if (!honda_v1_button_valid(btn))
        return false;

    uint64_t data = 0;
    for (size_t i = 0; i < 8; i++)
        data = (data << 8) | aligned[i];

    if (key) *key = data;
    if (serial) *serial = (uint32_t)((data >> 36) & 0x0FFFFFFFu);
    if (counter) *counter = (uint16_t)(data & 0xFFFFu);
    if (button) *button = btn;
    if (crc) *crc = aligned[8] >> 4;
    return true;
}

static void honda_v1_state_reset(honda_v1_state_t *state)
{
    state->step = HondaV1StepReset;
    state->preamble_count = 0;
    state->preamble_has_long = false;
    state->data_pending = false;
    state->last_level = false;
    state->bit_count = 0;
    memset(state->bits, 0, sizeof(state->bits));
}

static void honda_v1_add_bit(honda_v1_state_t *state, bool bit)
{
    if (state->bit_count > 0x4Bu)
        return;
    if (bit)
        state->bits[state->bit_count >> 3] |=
            (uint8_t)(1u << (((uint8_t)~state->bit_count) & 7u));
    state->bit_count++;
}

static bool honda_v1_commit(honda_v1_state_t *state, uint16_t protocol_index)
{
    uint64_t key = 0;
    uint32_t serial = 0;
    uint16_t counter = 0;
    uint8_t button = 0;
    uint8_t crc = 0;
    if (!m1_honda_v1_parse_frame(state->bits, state->bit_count, &key,
                                 &serial, &counter, &button, &crc))
        return false;

    subghz_decenc_ctl.n64_decodedvalue = key;
    subghz_decenc_ctl.n32_serialnumber = serial;
    subghz_decenc_ctl.n32_rollingcode = counter;
    subghz_decenc_ctl.n32_key2 = crc;
    subghz_decenc_ctl.n8_buttonid = button;
    subghz_decenc_ctl.ndecodedbitlength = HONDA_V1_BIT_COUNT;
    subghz_decenc_ctl.ndecodeddelay = 0;
    subghz_decenc_ctl.ndecodedprotocol = protocol_index;
    return true;
}

static bool honda_v1_symbol(honda_v1_state_t *state, bool level,
                            uint32_t duration, uint16_t protocol_index)
{
    const bool short_pulse = honda_v1_duration_is(duration, HONDA_V1_TE_SHORT);
    const bool long_pulse = honda_v1_duration_is(duration, HONDA_V1_TE_LONG);

    if (!short_pulse && !long_pulse)
    {
        bool decoded = false;
        if (!level && duration > HONDA_V1_TE_END && state->step == HondaV1StepData)
            decoded = honda_v1_commit(state, protocol_index);
        honda_v1_state_reset(state);
        return decoded;
    }

    if (state->step == HondaV1StepReset)
    {
        if (level)
        {
            state->step = HondaV1StepPreamble;
            state->preamble_count = 1;
            state->last_level = level;
        }
        return false;
    }

    if (state->step == HondaV1StepPreamble)
    {
        if (long_pulse)
        {
            if (state->preamble_count < 0xFFu)
                state->preamble_count++;
            state->preamble_has_long = true;
            state->last_level = level;
            return false;
        }

        if (state->preamble_has_long && state->preamble_count > 5u)
        {
            state->step = HondaV1StepData;
            state->bit_count = 0;
            memset(state->bits, 0, sizeof(state->bits));
            state->data_pending = true;
            state->last_level = level;
            return false;
        }

        if (state->preamble_count < 0xFFu)
            state->preamble_count++;
        state->last_level = level;
        return false;
    }

    if (short_pulse)
    {
        if (state->data_pending)
        {
            honda_v1_add_bit(state, level);
            state->data_pending = false;
            state->last_level = level;
            return false;
        }
        state->data_pending = true;
        state->last_level = level;
    }
    else
    {
        if (state->data_pending)
            honda_v1_add_bit(state, level);
        else
            honda_v1_add_bit(state, state->last_level);
        state->last_level = level;
    }
    return false;
}

static bool honda_v1_feed(honda_v1_state_t *state, bool level,
                          uint32_t duration, uint16_t protocol_index)
{
    if (duration < HONDA_V1_TE_DELTA)
    {
        state->pending += duration;
        state->pending_valid = true;
        return false;
    }

    if (state->pending_valid)
    {
        const uint32_t pending = state->pending;
        if (level)
        {
            state->pending = pending + duration;
            return false;
        }
        if (pending >= HONDA_V1_TE_SHORT_MIN &&
            honda_v1_symbol(state, true, pending, protocol_index))
            return true;
        state->pending = 0;
        state->pending_valid = false;
    }

    if (level)
    {
        state->pending = duration;
        state->pending_valid = true;
        return false;
    }

    return honda_v1_symbol(state, false, duration, protocol_index);
}

uint8_t subghz_decode_honda_v1(uint16_t protocol_index, uint16_t pulse_count)
{
    honda_v1_state_t state;
    memset(&state, 0, sizeof(state));

    for (uint16_t i = 0; i < pulse_count; i++)
    {
        if (honda_v1_feed(&state, (i & 1u) == 0u,
                          subghz_decenc_ctl.pulse_times[i], protocol_index))
            return 0;
    }

    return state.step == HondaV1StepData &&
                   honda_v1_commit(&state, protocol_index)
               ? 0
               : 1;
}
