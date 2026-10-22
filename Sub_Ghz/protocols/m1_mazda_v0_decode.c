/* See COPYING.txt for license details. */

/*
 * M1 Sub-GHz decoder for the ProtoPirate "Mazda V0" protocol.
 *
 * Decode algorithm derived from RocketGod-git/ProtoPirate
 * protocols/mazda_v0.c (GPLv3).  The TX encoder lives in
 * Sub_Ghz/subghz_proto_pirate.c; this file implements RX
 * identification only.
 *
 * Mazda V0 wire format (per transmission; the 52 ms gap is a hard
 * split under the M1 capture model): a 12-byte 0xFF Manchester preamble
 * block, a 52 ms low gap, then the frame body — two more 0xFF preamble
 * bytes, the 0xD7 sync byte, eight Manchester-coded data bytes carrying
 * the bitwise-inverse of the 64-bit key word, and a 0x5A tail byte.
 * Manchester cells are te_short = 250 us / te_long = 500 us with an
 * upstream tolerance of te_delta + 20 = 120 us.  The 64-bit key word is
 * XOR-obfuscated (mask derived from the interleaved counter bytes,
 * selected by the checksum parity); the low byte is the plaintext
 * checksum: bytewise sum of the 4 serial bytes, the 2 low counter bytes,
 * and the packed button/counter-high byte.
 *
 * Adaptation notes vs the upstream event-driven decoder:
 *  - M1 delivers one capture buffer per packet (the 52 ms gaps
 *    hard-separate captures), so the upstream state machine is re-run
 *    per buffer with stack state only.  The frame body arrives as its
 *    own buffer; the standalone preamble block decodes to nothing.
 *  - The commit fires the moment the 64th data bit is collected, as
 *    upstream; the tail byte and trailing gap are never needed.
 *
 * M1 Project -- Hapax fork
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "m1_sub_ghz_decenc.h"
#include "subghz_blocks_math.h"
#include "subghz_manchester_decoder.h"

#define MAZDA_V0_TE_SHORT 250u
#define MAZDA_V0_TE_LONG  500u
/* Upstream tolerance is te_delta (100) + 20. */
#define MAZDA_V0_TE_TOL   120u

#define MAZDA_V0_SYNC_BYTE     0xD7u
#define MAZDA_V0_PREAMBLE_ONES 16u
#define MAZDA_V0_BITS          64u

typedef enum {
    MazdaV0StepReset = 0,
    MazdaV0StepPreamble,
    MazdaV0StepData,
} mazda_v0_step_t;

typedef struct {
    mazda_v0_step_t step;
    ManchesterState manchester_state;
    uint64_t data;
    uint8_t bit_count;
    uint16_t preamble_count;
    uint8_t preamble_pattern;
} mazda_v0_state_t;

/*--------------------------------------------------------------------*/
/* Pure frame helpers (exported for host tests)                        */
/*--------------------------------------------------------------------*/

/* Plaintext checksum: bytewise sum of serial bytes, low counter bytes
 * and the packed button/counter-high byte. */
uint8_t m1_mazda_v0_checksum(uint32_t serial, uint8_t button, uint32_t counter)
{
    counter &= 0xFFFFFu;
    return (uint8_t)(((serial >> 24) & 0xFFu) + ((serial >> 16) & 0xFFu) +
                     ((serial >> 8) & 0xFFu) + (serial & 0xFFu) +
                     ((counter >> 8) & 0xFFu) + (counter & 0xFFu) +
                     ((((counter >> 16) & 0x0Fu) | ((button & 0x0Fu) << 4)) & 0xFFu));
}

/* Deobfuscate the wire key word and split it into fields.  The XOR mask
 * is one of the two interleaved counter bytes; the checksum parity
 * selects which. */
void m1_mazda_v0_parse_key(uint64_t key, uint32_t *serial, uint8_t *button,
                           uint32_t *counter)
{
    uint8_t data[8];
    for (uint8_t i = 0; i < 8u; i++)
        data[i] = (uint8_t)(key >> (56u - i * 8u));

    const bool parity = subghz_protocol_blocks_parity8(data[7]) != 0u;
    const uint8_t limit = parity ? 6u : 5u;
    const uint8_t mask = data[limit];

    for (uint8_t i = 0; i < limit; i++)
        data[i] ^= mask;
    if (!parity)
        data[6] ^= mask;

    const uint8_t counter_lo = (data[5] & 0x55u) | (data[6] & 0xAAu);
    const uint8_t counter_mid = (data[6] & 0x55u) | (data[5] & 0xAAu);

    if (serial)
        *serial = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |
                  ((uint32_t)data[2] << 8) | (uint32_t)data[3];
    if (button)
        *button = (data[4] >> 4) & 0x0Fu;
    if (counter)
        *counter = (((uint32_t)data[4] & 0x0Fu) << 16) |
                   ((uint32_t)counter_mid << 8) | (uint32_t)counter_lo;
}

/*--------------------------------------------------------------------*/
/* Pulse classification (faithful port of upstream mazda_v0_get_event) */
/*--------------------------------------------------------------------*/

/* Upstream maps HIGH pulses to the "Low" Manchester events and vice
 * versa; the air waveform is the inverted key, so the decoded stream is
 * the key word before the final bitwise inversion. */
static bool mazda_v0_get_event(uint16_t duration, bool level,
                               ManchesterEvent *event)
{
    if (get_diff(duration, MAZDA_V0_TE_SHORT) < MAZDA_V0_TE_TOL)
    {
        *event = level ? ManchesterEventShortLow : ManchesterEventShortHigh;
        return true;
    }
    if (get_diff(duration, MAZDA_V0_TE_LONG) < MAZDA_V0_TE_TOL)
    {
        *event = level ? ManchesterEventLongLow : ManchesterEventLongHigh;
        return true;
    }
    return false;
}

/*--------------------------------------------------------------------*/
/* Commit                                                              */
/*--------------------------------------------------------------------*/

static bool mazda_v0_commit(uint64_t wire_data, uint16_t protocol_index)
{
    const uint64_t key = ~wire_data;

    uint32_t serial = 0;
    uint32_t counter = 0;
    uint8_t button = 0;
    m1_mazda_v0_parse_key(key, &serial, &button, &counter);

    if (m1_mazda_v0_checksum(serial, button, counter) != (uint8_t)key)
        return false;

    subghz_decenc_ctl.n64_decodedvalue  = key;
    subghz_decenc_ctl.n32_serialnumber  = serial;
    subghz_decenc_ctl.n32_rollingcode   = counter;
    subghz_decenc_ctl.n8_buttonid       = button;
    subghz_decenc_ctl.ndecodedbitlength = MAZDA_V0_BITS;
    subghz_decenc_ctl.ndecodeddelay     = 0;
    subghz_decenc_ctl.ndecodedprotocol  = protocol_index;
    return true;
}

/*--------------------------------------------------------------------*/
/* Waveform state machine                                              */
/*--------------------------------------------------------------------*/

static void mazda_v0_state_clear(mazda_v0_state_t *state)
{
    memset(state, 0, sizeof(*state));
    state->step = MazdaV0StepReset;
    state->manchester_state = ManchesterStateStart1;
}

static bool mazda_v0_feed(mazda_v0_state_t *state, bool level,
                          uint16_t duration, uint16_t protocol_index)
{
    ManchesterEvent event = ManchesterEventReset;
    bool data = false;

    switch (state->step)
    {
        case MazdaV0StepReset:
            if (level && get_diff(duration, MAZDA_V0_TE_SHORT) < MAZDA_V0_TE_TOL)
            {
                state->data = 0;
                state->bit_count = 0;
                state->step = MazdaV0StepPreamble;
                state->manchester_state = ManchesterStateMid1;
                state->preamble_count = 0;
                state->preamble_pattern = 0;
            }
            break;

        case MazdaV0StepPreamble:
            if (!mazda_v0_get_event(duration, level, &event))
            {
                state->step = MazdaV0StepReset;
                break;
            }
            if (manchester_advance(state->manchester_state, event,
                                   &state->manchester_state, &data))
            {
                state->preamble_pattern =
                    (uint8_t)((state->preamble_pattern << 1) | (data ? 1u : 0u));
                if (data)
                {
                    state->preamble_count++;
                }
                else if (state->preamble_count <= MAZDA_V0_PREAMBLE_ONES - 1u)
                {
                    state->preamble_count = 0;
                    state->preamble_pattern = 0;
                    break;
                }
                if (state->preamble_pattern == MAZDA_V0_SYNC_BYTE &&
                    state->preamble_count > MAZDA_V0_PREAMBLE_ONES - 1u)
                {
                    state->data = 0;
                    state->bit_count = 0;
                    state->step = MazdaV0StepData;
                }
            }
            break;

        case MazdaV0StepData:
            if (!mazda_v0_get_event(duration, level, &event))
            {
                state->step = MazdaV0StepReset;
                break;
            }
            if (manchester_advance(state->manchester_state, event,
                                   &state->manchester_state, &data))
            {
                state->data = (state->data << 1) | (data ? 1u : 0u);
                state->bit_count++;
                if (state->bit_count == MAZDA_V0_BITS)
                {
                    const bool committed =
                        mazda_v0_commit(state->data, protocol_index);
                    mazda_v0_state_clear(state);
                    return committed;
                }
            }
            break;

        default:
            mazda_v0_state_clear(state);
            break;
    }
    return false;
}

uint8_t subghz_decode_mazda_v0(uint16_t protocol_index, uint16_t pulse_count)
{
    mazda_v0_state_t state;
    mazda_v0_state_clear(&state);

    for (uint16_t i = 0; i < pulse_count; i++)
    {
        if (mazda_v0_feed(&state, (i & 1u) == 0u,
                          subghz_decenc_ctl.pulse_times[i], protocol_index))
            return 0;
    }
    return 1;
}
