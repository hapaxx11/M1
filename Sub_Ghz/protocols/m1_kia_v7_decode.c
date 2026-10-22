/* See COPYING.txt for license details. */

/*
 * M1 Sub-GHz decoder for the ProtoPirate "Kia V7" protocol.
 *
 * Decode algorithm derived from RocketGod-git/ProtoPirate
 * protocols/kia_v7.c (GPLv3).  The TX encoder lives in
 * Sub_Ghz/subghz_proto_pirate.c; this file implements RX
 * identification only.
 *
 * Wire format (te 250/500 us, delta 100): a preamble of high/low 250 us
 * pairs (319 pairs on the first frame copy, 15 on tail copies; the
 * decoder accepts anything above 15), one extra high 250 us, then 64
 * Manchester bits, a trailing high 250 us and a low ~2000 us tail gap.
 * The sync pattern carries the top nibble of the collected word, so the
 * decoder pre-loads bits 1,0,1,1 (upstream behaviour).
 *
 * The collected word is the bitwise inverse of the on-air candidate:
 * candidate = ~decode_data.  The candidate is committed verbatim as
 * n64_decodedvalue (the layout the persistence/TX path round-trips).
 * Its high byte must be 0x4C and CRC-8 over candidate bytes 0..6
 * (poly 0x7F, init 0x4C) must equal byte 7 — both are hard gates.
 * Fields (big-endian byte slices of the candidate):
 *   serial  = (b3 << 20) | (b4 << 12) | (b5 << 4) | (b6 >> 4)
 *   counter = (b1 << 8) | b2
 *   button  = b6 & 0xF
 *
 * Adaptation notes vs the upstream event-driven decoder:
 *  - M1 delivers one capture buffer per packet (the pulse handler splits
 *    at >= INTERPACKET_GAP_MIN), so each frame copy arrives as a separate
 *    buffer and the upstream state machine is re-run per buffer with
 *    stack-local state only.  The low 2000 us tail gap belongs to the
 *    splitter and may not appear in the buffer; the commit needs only
 *    the 64th data bit, exactly like upstream.
 *
 * M1 Project -- Hapax fork
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "m1_sub_ghz_decenc.h"
#include "subghz_manchester_decoder.h"
#include "subghz_blocks_math.h"

#define KIA_V7_TE_SHORT 250u
#define KIA_V7_TE_LONG  500u
#define KIA_V7_TE_DELTA 100u

/* Upstream KIA_V7_PREAMBLE_MIN_PAIRS: sync accepted when more than
 * (16 - 1) preamble pairs were counted. */
#define KIA_V7_PREAMBLE_MIN_PAIRS 16u
#define KIA_V7_HEADER             0x4Cu
#define KIA_V7_KEY_BITS           64u

typedef enum {
    KiaV7StepReset = 0,
    KiaV7StepPreamble,
    KiaV7StepSyncLow,
    KiaV7StepData,
} kia_v7_step_t;

typedef struct {
    kia_v7_step_t step;
    uint16_t te_last;
    uint16_t preamble_count;
    ManchesterState manchester_state;
    uint64_t decode_data;
    uint8_t bit_count;
} kia_v7_state_t;

static bool kia_v7_is_short(uint16_t duration)
{
    return get_diff(duration, KIA_V7_TE_SHORT) < KIA_V7_TE_DELTA;
}

static bool kia_v7_is_long(uint16_t duration)
{
    return get_diff(duration, KIA_V7_TE_LONG) < KIA_V7_TE_DELTA;
}

/*--------------------------------------------------------------------*/
/* Pure frame helpers (exported for host tests)                        */
/*--------------------------------------------------------------------*/

/* Candidate gate + field decomposition.  Returns false when the high
 * byte is not 0x4C or the CRC-8 (poly 0x7F, init 0x4C) over bytes 0..6
 * does not match byte 7. */
bool m1_kia_v7_decode_fields(uint64_t candidate, uint32_t *serial,
                             uint8_t *button, uint32_t *counter)
{
    uint8_t bytes[8];
    for (uint8_t i = 0; i < 8u; i++)
        bytes[i] = (uint8_t)(candidate >> (56u - i * 8u));

    if (bytes[0] != KIA_V7_HEADER)
        return false;
    if (subghz_protocol_blocks_crc8(bytes, 7u, 0x7Fu, 0x4Cu) != bytes[7])
        return false;

    *serial = (((uint32_t)bytes[3]) << 20) | (((uint32_t)bytes[4]) << 12) |
              (((uint32_t)bytes[5]) << 4) | (((uint32_t)bytes[6]) >> 4);
    *counter = ((uint32_t)bytes[1] << 8) | (uint32_t)bytes[2];
    *button = bytes[6] & 0x0Fu;
    return true;
}

/*--------------------------------------------------------------------*/
/* Commit                                                              */
/*--------------------------------------------------------------------*/

static bool kia_v7_commit(const kia_v7_state_t *state, uint16_t protocol_index)
{
    const uint64_t candidate = ~state->decode_data;

    uint32_t serial = 0;
    uint32_t counter = 0;
    uint8_t button = 0;
    if (!m1_kia_v7_decode_fields(candidate, &serial, &button, &counter))
        return false;

    subghz_decenc_ctl.n64_decodedvalue = candidate;
    subghz_decenc_ctl.n32_serialnumber = serial;
    subghz_decenc_ctl.n32_rollingcode = counter;
    subghz_decenc_ctl.n8_buttonid = button;
    subghz_decenc_ctl.ndecodedbitlength = KIA_V7_KEY_BITS;
    subghz_decenc_ctl.ndecodeddelay = 0;
    subghz_decenc_ctl.ndecodedprotocol = protocol_index;
    return true;
}

/*--------------------------------------------------------------------*/
/* Waveform state machine                                              */
/*--------------------------------------------------------------------*/

static void kia_v7_state_clear(kia_v7_state_t *state)
{
    state->step = KiaV7StepReset;
    state->te_last = 0;
    state->preamble_count = 0;
    state->decode_data = 0;
    state->bit_count = 0;
    manchester_advance(state->manchester_state, ManchesterEventReset,
                       &state->manchester_state, NULL);
}

static void kia_v7_add_bit(kia_v7_state_t *state, uint8_t bit)
{
    state->decode_data = (state->decode_data << 1) | bit;
    state->bit_count++;
}

static bool kia_v7_feed(kia_v7_state_t *state, bool level, uint16_t duration,
                        uint16_t protocol_index)
{
    switch (state->step)
    {
        case KiaV7StepReset:
            if (level && kia_v7_is_short(duration))
            {
                state->step = KiaV7StepPreamble;
                state->te_last = duration;
                state->preamble_count = 0;
                manchester_advance(state->manchester_state,
                                   ManchesterEventReset,
                                   &state->manchester_state, NULL);
            }
            break;

        case KiaV7StepPreamble:
            if (level)
            {
                if (kia_v7_is_long(duration) && kia_v7_is_short(state->te_last))
                {
                    if (state->preamble_count > (KIA_V7_PREAMBLE_MIN_PAIRS - 1u))
                    {
                        state->decode_data = 0;
                        state->bit_count = 0;
                        state->preamble_count = 0;

                        /* The sync pattern carries the collected word's top
                         * nibble (~0x4C >> 4 = 0xB). */
                        kia_v7_add_bit(state, 1u);
                        kia_v7_add_bit(state, 0u);
                        kia_v7_add_bit(state, 1u);
                        kia_v7_add_bit(state, 1u);

                        state->te_last = duration;
                        state->step = KiaV7StepSyncLow;
                    }
                    else
                    {
                        state->step = KiaV7StepReset;
                    }
                }
                else if (kia_v7_is_short(duration))
                {
                    state->te_last = duration;
                }
                else
                {
                    state->step = KiaV7StepReset;
                }
            }
            else
            {
                if (kia_v7_is_short(duration) && kia_v7_is_short(state->te_last))
                {
                    state->preamble_count++;
                }
                else
                {
                    state->step = KiaV7StepReset;
                }
            }
            break;

        case KiaV7StepSyncLow:
            /* No else branch upstream: anything but the short low that
             * follows the sync long-high simply holds the state. */
            if (!level && kia_v7_is_short(duration) && kia_v7_is_long(state->te_last))
            {
                state->te_last = duration;
                state->step = KiaV7StepData;
            }
            break;

        case KiaV7StepData:
        {
            if (kia_v7_is_short(duration) || kia_v7_is_long(duration))
            {
                ManchesterEvent event;
                if (kia_v7_is_short(duration))
                {
                    event = (ManchesterEvent)((uint8_t)(level ? 1u : 0u) << 1u);
                }
                else
                {
                    event = level ? ManchesterEventLongHigh : ManchesterEventLongLow;
                }

                bool data = false;
                if (manchester_advance(state->manchester_state, event,
                                       &state->manchester_state, &data))
                    kia_v7_add_bit(state, data ? 1u : 0u);
            }
            /* Out-of-tolerance pulses in the data state are ignored, as
             * upstream; the 64-bit check below runs on every pulse. */

            if (state->bit_count == KIA_V7_KEY_BITS)
            {
                const bool committed = kia_v7_commit(state, protocol_index);
                kia_v7_state_clear(state);
                return committed;
            }
            break;
        }

        default:
            state->step = KiaV7StepReset;
            break;
    }
    return false;
}

uint8_t subghz_decode_kia_v7(uint16_t protocol_index, uint16_t pulse_count)
{
    kia_v7_state_t state;
    kia_v7_state_clear(&state);

    for (uint16_t i = 0; i < pulse_count; i++)
    {
        if (kia_v7_feed(&state, (i & 1u) == 0u,
                        subghz_decenc_ctl.pulse_times[i], protocol_index))
            return 0;
    }
    return 1;
}
