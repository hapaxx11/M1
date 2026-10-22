/* See COPYING.txt for license details. */

/*
 * M1 Sub-GHz decoder for the ProtoPirate "Chrysler V0" protocol.
 *
 * Decode algorithm derived from RocketGod-git/ProtoPirate
 * protocols/chrysler_v0.c (GPLv3).  The TX encoder lives in
 * Sub_Ghz/subghz_proto_pirate.c; this file implements RX
 * identification only.
 *
 * Chrysler V0 wire format (per payload; two payloads A/B per
 * transmission): 24 preamble pairs of (high 300 us, low 3700 us), a
 * high 300 us, a 15.6 ms low frame gap, then 80 PWM bits MSB first —
 * bit 1 = high 600 us + low 3400 us, bit 0 = high 300 us + low 3700 us
 * (te_short 300 +/- 150; long marks 3400/3700 +/- 400; any low longer
 * than 8000 us terminates the data run).  The 80 bits form a 64-bit
 * first word plus a 16-bit second word ("Key_2" upstream).  Byte 0
 * bits 7..2 carry the bit-reversed 6-bit seed; bytes 1..8 of the
 * payload are a 9-byte plain block XOR-masked by a 16-entry table
 * indexed by the seed low nibble (extra nibble flip for button 1).
 * The structural check is: MSB set   -> key[1]==key[5] and
 * key[6]^key[1]==0x62 (button 2 iff (key2_low ^ key[4])==0x10);
 * MSB clear -> (key[1]^0xC3)==key[5] and key[6]^key[1] in {0x04, 0x08}
 * (0x08 selects button 2).
 *
 * Adaptation notes vs the upstream event-driven decoder:
 *  - Upstream is a persistent instance fed per pulse; its Seek->Data
 *    transition fires on the 15.6 ms frame-gap low.  M1 hard-separates
 *    captures at gaps >= 5000 us, so the frame gap always ENDS the
 *    preamble buffer and the 80 data bits arrive at the START of the
 *    next buffer (the 3400/3700 us long symbols stay inside the buffer
 *    via the < 5000 us reopen path).  A stack-state rerun of the
 *    upstream machine can therefore never collect the data bits.
 *  - The port runs the faithful upstream machine first (handles a
 *    whole preamble+gap+data burst in one buffer); when it does not
 *    commit, the same machine is re-run pre-armed in the data state,
 *    which is exactly the state the upstream instance is in when the
 *    first data pulse arrives after the gap.  Both passes use stack
 *    state only; the commit stays gated by the upstream check, so the
 *    pre-armed pass cannot fire on arbitrary noise.
 *  - The commit fires the moment the 80th bit is collected, as
 *    upstream; a trailing gap (> 8000 us low) also commits a complete
 *    collection, as upstream.
 *
 * M1 Project -- Hapax fork
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "m1_sub_ghz_decenc.h"

#define CHRYSLER_V0_TE_SHORT      300u   /* 0x12C */
#define CHRYSLER_V0_TE_DELTA      150u   /* 0x96  */
#define CHRYSLER_V0_TE_LONG_A     3400u  /* 0xD48 */
#define CHRYSLER_V0_TE_LONG_B     3700u  /* 0xE74 */
#define CHRYSLER_V0_TE_LONG_DELTA 400u   /* 0x190 */
#define CHRYSLER_V0_TE_GAP        8000u  /* 0x1F40 */
#define CHRYSLER_V0_TE_ONE_SHORT  600u   /* 0x258 */
#define CHRYSLER_V0_TE_ONE_MAX    750u   /* 0x2EE */
#define CHRYSLER_V0_PREAMBLE_MIN  0x0Fu  /* upstream requires > 15 pairs */
#define CHRYSLER_V0_BITS          0x50u  /* 80 = 64-bit word + 16-bit Key_2 */

typedef enum {
    ChryslerV0StepReset = 0,
    ChryslerV0StepSeek,
    ChryslerV0StepData,
} chrysler_v0_step_t;

typedef struct {
    chrysler_v0_step_t step;
    uint16_t te_last;
    uint64_t data;
    uint16_t data_2;
    uint8_t bit_count;
    uint16_t packet_bit_count;
} chrysler_v0_state_t;

/*--------------------------------------------------------------------*/
/* Pure frame helpers (exported for host tests)                        */
/*--------------------------------------------------------------------*/

uint8_t m1_chrysler_v0_reverse6(uint32_t value)
{
    uint8_t out = 0;
    uint8_t bits = 6;
    while (bits--)
    {
        out = (uint8_t)((out << 1) | (value & 1u));
        value >>= 1;
    }
    return out;
}

static const uint8_t chrysler_v0_xor_table[16] = {
    0x0F, 0x02, 0x40, 0x0C, 0x30, 0x0E, 0x70, 0x08,
    0x10, 0x0A, 0x50, 0xF4, 0x2F, 0xF6, 0x6F, 0xF0,
};

void m1_chrysler_v0_transform_block(const uint8_t in[9], uint8_t out[9],
                                    uint32_t key, uint8_t button)
{
    uint8_t mask = chrysler_v0_xor_table[key & 0x0Fu];
    if (button == 1u)
        mask ^= (key & 1u) ? 0xF0u : 0x0Fu;
    for (uint8_t i = 0; i < 9u; i++)
        out[i] = (uint8_t)(in[i] ^ mask);
}

/* Faithful port of upstream chrysler_v0_decode_packet.  Returns the
 * upstream check_ok flag.  *serial receives the plain-block serial: for
 * odd seeds (payload B) decoded[0..2,7], for even seeds (payload A) the
 * decoded[0..3] counter word.  *seed_out receives the 6-bit seed. */
bool m1_chrysler_v0_decode_packet(uint64_t data, uint16_t data_2,
                                  uint32_t *serial, uint8_t *button,
                                  uint8_t *seed_out)
{
    uint8_t key[8];
    for (uint8_t i = 0; i < 8u; i++)
        key[i] = (uint8_t)(data >> (56u - i * 8u));

    const uint8_t seed = m1_chrysler_v0_reverse6(key[0] >> 2u);
    const uint8_t b1_xor_b6 = (uint8_t)(key[6] ^ key[1]);
    const bool msb_set = (key[0] & 0x80u) != 0u;

    bool check_ok;
    uint8_t decoded_button;
    if (msb_set)
    {
        const uint8_t key2_low = (uint8_t)(data_2 & 0xFFu);
        check_ok = (key[1] == key[5]) && (b1_xor_b6 == 0x62u);
        decoded_button = ((uint8_t)(key2_low ^ key[4]) == 0x10u) ? 2u : 1u;
    }
    else
    {
        check_ok = false;
        decoded_button = 1u;
        if ((uint8_t)(key[1] ^ 0xC3u) == key[5])
        {
            if (b1_xor_b6 == 0x04u)
            {
                check_ok = true;
            }
            else
            {
                check_ok = (b1_xor_b6 == 0x08u);
                if (b1_xor_b6 == 0x08u)
                    decoded_button = 2u;
            }
        }
        else
        {
            if (b1_xor_b6 == 0x08u)
                decoded_button = 2u;
        }
    }

    uint8_t encoded[9];
    uint8_t decoded[9];
    encoded[0] = key[1];
    encoded[1] = key[2];
    encoded[2] = key[3];
    encoded[3] = key[4];
    encoded[4] = key[5];
    encoded[5] = key[6];
    encoded[6] = key[7];
    encoded[7] = (uint8_t)(data_2 >> 8u);
    encoded[8] = (uint8_t)(data_2 & 0xFFu);
    m1_chrysler_v0_transform_block(encoded, decoded, seed, decoded_button);

    uint32_t serial_value;
    if (seed & 1u)
    {
        /* Payload B: plain-B serial from decoded[0..2] and decoded[7]. */
        serial_value = ((uint32_t)decoded[0] << 24) |
                       ((uint32_t)decoded[1] << 16) |
                       ((uint32_t)decoded[2] << 8) | (uint32_t)decoded[7];
    }
    else
    {
        /* Payload A: 32-bit counter word from decoded[0..3]. */
        serial_value = ((uint32_t)decoded[0] << 24) |
                       ((uint32_t)decoded[1] << 16) |
                       ((uint32_t)decoded[2] << 8) | (uint32_t)decoded[3];
    }

    if (serial)
        *serial = serial_value;
    if (button)
        *button = decoded_button;
    if (seed_out)
        *seed_out = seed;
    return check_ok;
}

/*--------------------------------------------------------------------*/
/* Pulse classification                                                */
/*--------------------------------------------------------------------*/

static bool chrysler_v0_is_short(uint16_t duration)
{
    return get_diff(duration, CHRYSLER_V0_TE_SHORT) <= CHRYSLER_V0_TE_DELTA;
}

static bool chrysler_v0_is_long_mark(uint16_t duration)
{
    return get_diff(duration, CHRYSLER_V0_TE_LONG_A) <= CHRYSLER_V0_TE_LONG_DELTA ||
           get_diff(duration, CHRYSLER_V0_TE_LONG_B) <= CHRYSLER_V0_TE_LONG_DELTA;
}

/*--------------------------------------------------------------------*/
/* Commit                                                              */
/*--------------------------------------------------------------------*/

static bool chrysler_v0_commit(uint64_t data, uint16_t data_2,
                               uint16_t protocol_index)
{
    uint32_t serial = 0;
    uint8_t button = 0;
    uint8_t seed = 0;
    if (!m1_chrysler_v0_decode_packet(data, data_2, &serial, &button, &seed))
        return false;

    subghz_decenc_ctl.n64_decodedvalue  = data;
    /* Second word: the 16-bit "Key_2" (payload bytes 8..9), matching the
     * upstream "Key_2:" field. */
    subghz_decenc_ctl.n32_key2          = data_2;
    subghz_decenc_ctl.n32_serialnumber  = serial;
    subghz_decenc_ctl.n32_rollingcode   = seed;
    subghz_decenc_ctl.n8_buttonid       = button;
    subghz_decenc_ctl.ndecodedbitlength = CHRYSLER_V0_BITS;
    subghz_decenc_ctl.ndecodeddelay     = 0;
    subghz_decenc_ctl.ndecodedprotocol  = protocol_index;
    return true;
}

/*--------------------------------------------------------------------*/
/* Waveform state machine (faithful port of the upstream feed)         */
/*--------------------------------------------------------------------*/

static void chrysler_v0_state_clear(chrysler_v0_state_t *state)
{
    memset(state, 0, sizeof(*state));
    state->step = ChryslerV0StepReset;
}

static bool chrysler_v0_feed(chrysler_v0_state_t *state, bool level,
                             uint16_t duration, uint16_t protocol_index)
{
    switch (state->step)
    {
        case ChryslerV0StepReset:
            if (level && chrysler_v0_is_short(duration))
            {
                state->packet_bit_count = 0;
                state->te_last = duration;
                state->step = ChryslerV0StepSeek;
            }
            break;

        case ChryslerV0StepSeek:
            if (level)
            {
                state->te_last = duration;
                break;
            }
            if (chrysler_v0_is_long_mark(duration))
            {
                if (chrysler_v0_is_short(state->te_last))
                {
                    state->packet_bit_count++;
                }
                else if (state->packet_bit_count > CHRYSLER_V0_PREAMBLE_MIN)
                {
                    state->data_2 = 0;
                    state->step = ChryslerV0StepData;
                    state->data = 1;
                    state->bit_count = 1;
                }
                else
                {
                    state->packet_bit_count = 0;
                    state->step = ChryslerV0StepSeek;
                }
                break;
            }
            if (duration > CHRYSLER_V0_TE_GAP &&
                state->packet_bit_count > CHRYSLER_V0_PREAMBLE_MIN)
            {
                state->data = 0;
                state->data_2 = 0;
                state->bit_count = 0;
                state->step = ChryslerV0StepData;
                break;
            }
            state->step = ChryslerV0StepReset;
            state->packet_bit_count = 0;
            break;

        case ChryslerV0StepData:
        {
            if (level)
            {
                state->te_last = duration;
                break;
            }

            const uint8_t count = state->bit_count;
            if (duration > CHRYSLER_V0_TE_GAP)
            {
                if (count > 0x4Fu &&
                    chrysler_v0_commit(state->data, state->data_2,
                                       protocol_index))
                    return true;
                state->step = ChryslerV0StepReset;
                state->packet_bit_count = 0;
                break;
            }

            uint8_t bit_value = 0;
            if (state->te_last < CHRYSLER_V0_TE_SHORT)
            {
                if (!chrysler_v0_is_short(state->te_last) ||
                    !chrysler_v0_is_long_mark(duration))
                {
                    if (count > 0x4Fu &&
                        chrysler_v0_commit(state->data, state->data_2,
                                           protocol_index))
                        return true;
                    state->step = ChryslerV0StepReset;
                    state->packet_bit_count = 0;
                    break;
                }
                bit_value = 1u;
            }
            else
            {
                if (state->te_last > CHRYSLER_V0_TE_ONE_MAX ||
                    !chrysler_v0_is_long_mark(duration))
                {
                    if (count > 0x4Fu &&
                        chrysler_v0_commit(state->data, state->data_2,
                                           protocol_index))
                        return true;
                    state->step = ChryslerV0StepReset;
                    state->packet_bit_count = 0;
                    break;
                }
                bit_value = chrysler_v0_is_short(state->te_last) ? 1u : 0u;
            }

            const uint8_t bit = bit_value ^ 1u;
            const uint8_t new_count = (uint8_t)(count + 1u);
            if (count <= 0x3Fu)
            {
                state->data = (state->data << 1) | bit;
                state->bit_count = new_count;
                break;
            }

            state->data_2 = (uint16_t)((state->data_2 << 1) | bit);
            state->bit_count = new_count;
            if (new_count != CHRYSLER_V0_BITS)
                break;

            const uint64_t data = state->data;
            const uint16_t data_2 = state->data_2;
            const bool committed = chrysler_v0_commit(data, data_2, protocol_index);
            state->data = 0;
            state->data_2 = 0;
            state->bit_count = 0;
            state->step = ChryslerV0StepReset;
            state->packet_bit_count = 0;
            return committed;
        }

        default:
            chrysler_v0_state_clear(state);
            break;
    }
    return false;
}

static bool chrysler_v0_run(chrysler_v0_step_t initial_step,
                            uint16_t protocol_index, uint16_t pulse_count)
{
    chrysler_v0_state_t state;
    chrysler_v0_state_clear(&state);
    state.step = initial_step;

    for (uint16_t i = 0; i < pulse_count; i++)
    {
        if (chrysler_v0_feed(&state, (i & 1u) == 0u,
                             subghz_decenc_ctl.pulse_times[i], protocol_index))
            return true;
    }
    return false;
}

uint8_t subghz_decode_chrysler_v0(uint16_t protocol_index, uint16_t pulse_count)
{
    /* Pass 1: faithful upstream machine — handles a full
     * preamble+gap+data burst delivered in one buffer. */
    if (chrysler_v0_run(ChryslerV0StepReset, protocol_index, pulse_count))
        return 0;

    /* Pass 2 (M1 split adaptation): the 15.6 ms frame gap hard-separates
     * the capture, so the data bits arrive at the start of the next
     * buffer.  Re-run pre-armed in the data state — the exact state the
     * upstream instance is in after the frame-gap low.  The commit stays
     * gated by the upstream structural check. */
    if (chrysler_v0_run(ChryslerV0StepData, protocol_index, pulse_count))
        return 0;

    return 1;
}
