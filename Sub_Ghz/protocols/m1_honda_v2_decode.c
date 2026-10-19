/* See COPYING.txt for license details. */

/*
 * M1 Sub-GHz decoder for the ProtoPirate "Honda V2" protocol.
 *
 * Decode algorithm derived from RocketGod-git/ProtoPirate
 * protocols/honda_v2.c (GPLv3). The TX encoder remains in
 * Sub_Ghz/subghz_proto_pirate.c; this file implements RX identification only.
 *
 * M1 Project -- Hapax fork
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "m1_sub_ghz_decenc.h"

#define HONDA_V2_BIT_COUNT          81u
#define HONDA_V2_MIN_PREAMBLE_PAIRS 64u
#define HONDA_V2_TE_SHORT           250u
#define HONDA_V2_TE_LONG            500u
#define HONDA_V2_TE_DELTA           100u
#define HONDA_V2_SYNC_US            750u
#define HONDA_V2_SYNC_DELTA_US      120u
#define HONDA_V2_SIG_UNLOCK         0xA285E3u
#define HONDA_V2_SIG_LOCK           0xC20363u

typedef enum {
    HondaV2StepReset = 0,
    HondaV2StepPreambleLow,
    HondaV2StepPreambleHigh,
    HondaV2StepSyncLow,
    HondaV2StepData,
} honda_v2_step_t;

typedef struct {
    honda_v2_step_t step;
    uint16_t preamble_count;
    uint8_t raw[10];
    uint8_t bit_count;
    bool extra_bit;
    bool previous_bit;
    bool boundary_pad_skipped;
    bool pending_short;
} honda_v2_state_t;

static bool honda_v2_duration_is(uint32_t duration, uint32_t target,
                                 uint32_t delta)
{
    return duration >= target ? duration - target < delta
                              : target - duration < delta;
}

static bool honda_v2_is_short(uint32_t duration)
{
    return honda_v2_duration_is(duration, HONDA_V2_TE_SHORT, HONDA_V2_TE_DELTA);
}

static bool honda_v2_is_long(uint32_t duration)
{
    return honda_v2_duration_is(duration, HONDA_V2_TE_LONG, HONDA_V2_TE_DELTA);
}

static bool honda_v2_is_sync(uint32_t duration)
{
    return honda_v2_duration_is(duration, HONDA_V2_SYNC_US,
                                HONDA_V2_SYNC_DELTA_US);
}

static uint8_t honda_v2_calculate_check(uint32_t count)
{
    const uint8_t c0 = ((count >> 1) ^ (count >> 2) ^ (count >> 3) ^
                        (count >> 4) ^ (count >> 6)) & 1u;
    const uint8_t c1 = ((count >> 0) ^ (count >> 2) ^ (count >> 3) ^
                        (count >> 4) ^ (count >> 5) ^ (count >> 6) ^ 1u) & 1u;
    const uint8_t c2 = ((count >> 1) ^ (count >> 3) ^ (count >> 4) ^
                        (count >> 5) ^ (count >> 6)) & 1u;
    return (uint8_t)(c0 | (c1 << 1) | (c2 << 2));
}

static uint16_t honda_v2_calculate_tail(uint32_t count)
{
    const uint8_t msb = ((count >> 0) ^ (count >> 2) ^
                         (count >> 4) ^ (count >> 5)) & 1u;
    return msb ? 0xFFFFu : 0x7FFFu;
}

static uint8_t honda_v2_button_from_signature(uint32_t signature)
{
    if (signature == HONDA_V2_SIG_UNLOCK)
        return 0x04u;
    if (signature == HONDA_V2_SIG_LOCK)
        return 0x02u;
    return 0;
}

bool m1_honda_v2_parse_frame(const uint8_t raw[10], bool extra_bit,
                             uint64_t *key, uint16_t *tail,
                             uint32_t *signature, uint32_t *serial,
                             uint32_t *count, uint8_t *button, uint8_t *check)
{
    if (!raw)
        return false;

    const uint32_t cnt = ((uint32_t)raw[6] << 1) | (raw[7] >> 7);
    const uint16_t tail_value = ((uint16_t)raw[8] << 8) | raw[9];
    if ((raw[7] & 0x78u) != 0u ||
        (raw[7] & 0x07u) != honda_v2_calculate_check(cnt) ||
        tail_value != honda_v2_calculate_tail(cnt) || !extra_bit)
        return false;

    uint64_t data = 0;
    for (size_t i = 0; i < 8; i++)
        data = (data << 8) | raw[i];
    uint32_t sig = ((uint32_t)raw[0] << 16) |
                   ((uint32_t)raw[1] << 8) | raw[2];
    const uint32_t sn = ((uint32_t)raw[3] << 16) |
                        ((uint32_t)raw[4] << 8) | raw[5];

    /* ProtoPirate's lock waveform carries the normalized 0x820363 signature;
     * restore its canonical key/button form after validating the frame. */
    if (sig == 0x820363u)
    {
        data |= 1ULL << 62;
        sig = HONDA_V2_SIG_LOCK;
    }

    if (key) *key = data;
    if (tail) *tail = tail_value;
    if (signature) *signature = sig;
    if (serial) *serial = sn;
    if (count) *count = cnt;
    if (button) *button = honda_v2_button_from_signature(sig);
    if (check) *check = raw[7] & 0x07u;
    return true;
}

static bool honda_v2_add_bit(honda_v2_state_t *state, bool bit)
{
    if (state->bit_count < 80u)
    {
        if (bit)
            state->raw[state->bit_count / 8u] |=
                (uint8_t)(1u << (7u - (state->bit_count % 8u)));
    }
    else if (state->bit_count == 80u)
    {
        state->extra_bit = bit;
    }
    else
    {
        return false;
    }

    state->bit_count++;
    return true;
}

static bool honda_v2_process_transition(honda_v2_state_t *state, bool level,
                                        uint32_t duration)
{
    if (!state->boundary_pad_skipped)
    {
        if (level && honda_v2_is_short(duration))
        {
            state->boundary_pad_skipped = true;
            return true;
        }
        state->boundary_pad_skipped = true;
    }

    if (state->pending_short)
    {
        if (!state->previous_bit && !level && honda_v2_is_short(duration))
        {
            state->pending_short = false;
            return honda_v2_add_bit(state, false);
        }
        if (state->previous_bit && level && honda_v2_is_short(duration))
        {
            state->pending_short = false;
            return honda_v2_add_bit(state, true);
        }
        return false;
    }

    if (!state->previous_bit)
    {
        if (level && honda_v2_is_long(duration))
        {
            state->previous_bit = true;
            return honda_v2_add_bit(state, true);
        }
        if (level && honda_v2_is_short(duration))
        {
            state->pending_short = true;
            return true;
        }
        return false;
    }

    if (!level && honda_v2_is_long(duration))
    {
        state->previous_bit = false;
        return honda_v2_add_bit(state, false);
    }
    if (!level && honda_v2_is_short(duration))
    {
        state->pending_short = true;
        return true;
    }
    return false;
}

static bool honda_v2_finish(honda_v2_state_t *state, uint16_t protocol_index)
{
    uint64_t key = 0;
    uint16_t tail = 0;
    uint32_t signature = 0;
    uint32_t serial = 0;
    uint32_t count = 0;
    uint8_t button = 0;
    uint8_t check = 0;
    if (!m1_honda_v2_parse_frame(state->raw, state->extra_bit, &key, &tail,
                                 &signature, &serial, &count, &button, &check))
        return false;

    (void)signature;
    subghz_decenc_ctl.n64_decodedvalue = key;
    subghz_decenc_ctl.n32_serialnumber = serial;
    subghz_decenc_ctl.n32_rollingcode = count;
    subghz_decenc_ctl.n32_key2 = ((uint32_t)tail << 8) | check;
    subghz_decenc_ctl.n8_buttonid = button;
    subghz_decenc_ctl.ndecodedbitlength = HONDA_V2_BIT_COUNT;
    subghz_decenc_ctl.ndecodeddelay = 0;
    subghz_decenc_ctl.ndecodedprotocol = protocol_index;
    return true;
}

static bool honda_v2_feed(honda_v2_state_t *state, bool level,
                          uint32_t duration, uint16_t protocol_index)
{
    switch (state->step)
    {
        case HondaV2StepReset:
            if (level && honda_v2_is_short(duration))
            {
                state->preamble_count = 0;
                state->step = HondaV2StepPreambleLow;
            }
            break;

        case HondaV2StepPreambleLow:
            if (!level && honda_v2_is_short(duration))
            {
                state->preamble_count++;
                state->step = HondaV2StepPreambleHigh;
            }
            else
            {
                state->step = HondaV2StepReset;
            }
            break;

        case HondaV2StepPreambleHigh:
            if (level && honda_v2_is_short(duration))
            {
                state->step = HondaV2StepPreambleLow;
            }
            else if (level && honda_v2_is_sync(duration) &&
                     state->preamble_count >= HONDA_V2_MIN_PREAMBLE_PAIRS)
            {
                state->step = HondaV2StepSyncLow;
            }
            else
            {
                state->step = HondaV2StepReset;
            }
            break;

        case HondaV2StepSyncLow:
            if (!level && honda_v2_is_sync(duration))
            {
                memset(state->raw, 0, sizeof(state->raw));
                state->bit_count = 0;
                state->extra_bit = false;
                state->previous_bit = true;
                state->boundary_pad_skipped = false;
                state->pending_short = false;
                honda_v2_add_bit(state, true);
                state->step = HondaV2StepData;
            }
            else
            {
                state->step = HondaV2StepReset;
            }
            break;

        case HondaV2StepData:
            if (!honda_v2_process_transition(state, level, duration))
            {
                state->step = HondaV2StepReset;
                break;
            }
            if (state->bit_count == HONDA_V2_BIT_COUNT)
            {
                const bool decoded = honda_v2_finish(state, protocol_index);
                state->step = HondaV2StepReset;
                return decoded;
            }
            break;
    }
    return false;
}

uint8_t subghz_decode_honda_v2(uint16_t protocol_index, uint16_t pulse_count)
{
    honda_v2_state_t state;
    memset(&state, 0, sizeof(state));

    for (uint16_t i = 0; i < pulse_count; i++)
    {
        if (honda_v2_feed(&state, (i & 1u) == 0u,
                          subghz_decenc_ctl.pulse_times[i], protocol_index))
            return 0;
    }
    return 1;
}
