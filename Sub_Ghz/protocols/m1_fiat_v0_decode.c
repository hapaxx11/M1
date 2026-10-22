/* See COPYING.txt for license details. */

/*
 * M1 Sub-GHz decoder for the ProtoPirate "Fiat V0" automotive keyfob
 * protocol.
 *
 * Decode algorithm derived from RocketGod-git/ProtoPirate protocols/fiat_v0.c
 * (GPLv3): differential Manchester, te 200/400 us (delta 100), 150-pair
 * short/short preamble, 800 us sync gap, 64-bit key (hop << 32 | fix)
 * followed by a 6-bit end byte — 71 bits committed upstream.  The TX encoder
 * lives in Sub_Ghz/subghz_proto_pirate.c; this file implements RX
 * identification only.
 *
 * Adaptation notes vs the upstream event-driven decoder:
 *  - M1 delivers one capture buffer per burst: the ~1.6 ms trailer low of a
 *    burst merges on air with the 25 ms inter-burst gap and hard-separates
 *    the capture, so the upstream state machine is re-run per buffer and
 *    never sees the following burst's preamble.  Upstream's 71st bit (and,
 *    on air, the 70th) comes from that preamble: the merged trailer absorbs
 *    the final low Manchester cell, so a standalone burst yields 70 decoded
 *    bits when the wire end byte is odd and 69 when it is even (the absorbed
 *    trailing bit is always a 0).  As a documented relaxation, a complete
 *    collection is committed when the non-Manchester trailer low arrives or
 *    the buffer ends, and the end byte is reconstructed exactly as the
 *    upstream codec reports it on air (verified bit-exact against the
 *    upstream encoder+decoder on the host):
 *      70 bits collected (wire6 odd):  endbyte = (wire6 << 1) | 1
 *      69 bits collected (wire6 even): endbyte = ((data_low & 0x1F) << 2) | 3
 *      (values masked to 6 bits, as upstream's data_low & 0x3F does).
 *  - Upstream drops the first data bit when it is a 1 (a te_long high into
 *    the just-reset Manchester machine produces no output, shifting the whole
 *    frame).  As a documented adaptation, a te_long high as the very first
 *    data pulse — which the encoder only emits for a leading 1 — seeds that
 *    bit, so frames with bit 63 set decode to the same fields the encoder
 *    started from instead of upstream's shifted values.
 *  - Upstream's unreachable duplicate gap check at the end of the preamble
 *    step (a low matching te_short +/- delta can never also match the 800 us
 *    gap +/- delta) is omitted.
 *  - Fiat V0 carries no checksum; like upstream, commits are structural.
 *
 * M1 Project -- Hapax fork
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "m1_sub_ghz_decenc.h"
#include "subghz_manchester_decoder.h"

#define FIAT_V0_TE_SHORT 200u
#define FIAT_V0_TE_LONG  400u
#define FIAT_V0_TE_DELTA 100u
#define FIAT_V0_GAP_US   800u

/* Upstream counts BOTH preamble edges (high and low shorts), so the
 * FIAT_V0_PREAMBLE_PAIRS=150 gate trips after ~75 short/short pairs. */
#define FIAT_V0_MIN_PREAMBLE_EDGES 150u

#define FIAT_V0_KEY_BITS     64u
#define FIAT_V0_ENDBYTE_BITS 6u
/* 64 key bits + 6 end-byte bits on the wire. */
#define FIAT_V0_FRAME_BITS   (FIAT_V0_KEY_BITS + FIAT_V0_ENDBYTE_BITS)
/* 0x47: upstream commit point (frame bits + the duplicated trailing bit). */
#define FIAT_V0_COMMIT_BITS  71u

typedef enum {
    FiatV0StepReset = 0,
    FiatV0StepPreamble,
    FiatV0StepData,
} fiat_v0_step_t;

typedef struct {
    fiat_v0_step_t step;
    ManchesterState manchester_state;
    uint16_t preamble_count;
    uint32_t data_low;
    uint32_t data_high;
    uint8_t bit_count;
    uint32_t hop;
    uint32_t fix;
} fiat_v0_state_t;

static bool fiat_v0_is_short(uint16_t duration)
{
    return get_diff(duration, FIAT_V0_TE_SHORT) < FIAT_V0_TE_DELTA;
}

static bool fiat_v0_is_long(uint16_t duration)
{
    return get_diff(duration, FIAT_V0_TE_LONG) < FIAT_V0_TE_DELTA;
}

static bool fiat_v0_is_gap(uint16_t duration)
{
    return get_diff(duration, FIAT_V0_GAP_US) < FIAT_V0_TE_DELTA;
}

/*--------------------------------------------------------------------*/
/* Pure helpers (exported for host tests)                              */
/*--------------------------------------------------------------------*/

/* Split a committed Fiat V0 key word the way upstream does:
 * data = (hop << 32) | fix; serial = fix, counter = hop. */
void m1_fiat_v0_parse_fields(uint64_t data, uint32_t *hop, uint32_t *fix)
{
    if (hop)
        *hop = (uint32_t)(data >> 32);
    if (fix)
        *fix = (uint32_t)(data & 0xFFFFFFFFu);
}

/* Upstream fiat_v0_display_suffix(): button name from the low nibble of the
 * decoded end byte. */
const char *m1_fiat_v0_button_name(uint8_t endbyte)
{
    const uint8_t low_nibble = endbyte & 0x0Fu;
    if (low_nibble >= 0x04u && low_nibble <= 0x07u)
        return "Lock";
    if (low_nibble >= 0x08u && low_nibble <= 0x0Bu)
        return "Unlock";
    return "??";
}

/*--------------------------------------------------------------------*/
/* Commit                                                              */
/*--------------------------------------------------------------------*/

static bool fiat_v0_commit(const fiat_v0_state_t *state, uint8_t endbyte,
                           uint16_t protocol_index)
{
    subghz_decenc_ctl.n64_decodedvalue =
        ((uint64_t)state->hop << 32) | state->fix;
    subghz_decenc_ctl.n32_serialnumber = state->fix;
    subghz_decenc_ctl.n32_rollingcode = state->hop;
    subghz_decenc_ctl.n8_buttonid = endbyte;
    subghz_decenc_ctl.ndecodedbitlength = FIAT_V0_COMMIT_BITS;
    subghz_decenc_ctl.ndecodeddelay = 0;
    subghz_decenc_ctl.ndecodedprotocol = protocol_index;
    return true;
}

/* M1 relaxation: on air the merged trailer absorbs the final low Manchester
 * cell, so a standalone burst yields 70 decoded bits (wire end byte odd) or
 * 69 (wire end byte even — the absorbed trailing bit is always a 0, and
 * upstream's following-preamble artifact bits come out as 1s).  These
 * reconstructions reproduce bit-exactly what the upstream codec reports for
 * the same waveform (verified against the upstream encoder+decoder on the
 * host). */
static uint8_t fiat_v0_endbyte_from_70(uint8_t wire6)
{
    return (uint8_t)(((wire6 << 1) | 1u) & 0x3Fu);
}

static uint8_t fiat_v0_endbyte_from_69(uint8_t wire5)
{
    return (uint8_t)(((wire5 << 2) | 0x03u) & 0x3Fu);
}

/*--------------------------------------------------------------------*/
/* Waveform state machine                                              */
/*--------------------------------------------------------------------*/

static void fiat_v0_data_reset(fiat_v0_state_t *state)
{
    state->preamble_count = 0;
    state->data_low = 0;
    state->data_high = 0;
    state->bit_count = 0;
    manchester_advance(state->manchester_state, ManchesterEventReset,
                       &state->manchester_state, NULL);
}

static bool fiat_v0_feed(fiat_v0_state_t *state, bool level, uint16_t duration,
                         uint16_t protocol_index)
{
    switch (state->step)
    {
        case FiatV0StepReset:
            if (!level)
                break;
            if (fiat_v0_is_short(duration))
            {
                state->step = FiatV0StepPreamble;
                fiat_v0_data_reset(state);
            }
            break;

        case FiatV0StepPreamble:
            if (level)
            {
                if (fiat_v0_is_short(duration))
                    state->preamble_count++;
                else
                    state->step = FiatV0StepReset;
                break;
            }
            if (fiat_v0_is_short(duration))
            {
                state->preamble_count++;
            }
            else
            {
                if (state->preamble_count >= FIAT_V0_MIN_PREAMBLE_EDGES &&
                    fiat_v0_is_gap(duration))
                {
                    state->step = FiatV0StepData;
                    fiat_v0_data_reset(state);
                }
                else
                {
                    state->step = FiatV0StepReset;
                }
            }
            break;

        case FiatV0StepData:
        {
            ManchesterEvent event = ManchesterEventReset;
            if (fiat_v0_is_short(duration))
                event = level ? ManchesterEventShortLow : ManchesterEventShortHigh;
            else if (fiat_v0_is_long(duration))
                event = level ? ManchesterEventLongLow : ManchesterEventLongHigh;

            if (event != ManchesterEventReset)
            {
                bool data_bit = false;
                if (manchester_advance(state->manchester_state, event,
                                       &state->manchester_state, &data_bit))
                {
                    const uint32_t new_bit = data_bit ? 1u : 0u;
                    const uint32_t carry = (state->data_low >> 31) & 1u;
                    state->data_low = (state->data_low << 1) | new_bit;
                    state->data_high = (state->data_high << 1) | carry;
                    state->bit_count++;

                    if (state->bit_count == FIAT_V0_KEY_BITS)
                    {
                        state->fix = state->data_low;
                        state->hop = state->data_high;
                        state->data_low = 0;
                        state->data_high = 0;
                    }
                    if (state->bit_count == FIAT_V0_COMMIT_BITS)
                    {
                        return fiat_v0_commit(state,
                                              (uint8_t)(state->data_low & 0x3Fu),
                                              protocol_index);
                    }
                }
                else if (state->bit_count == 0u &&
                         event == ManchesterEventLongLow)
                {
                    /* First data pulse is a te_long high: the encoder only
                     * emits that for a leading 1, which the just-reset
                     * Manchester machine cannot output.  Seed the bit. */
                    state->data_low = 1;
                    state->data_high = 0;
                    state->bit_count = 1;
                }
            }
            else
            {
                if (state->bit_count == FIAT_V0_COMMIT_BITS)
                {
                    return fiat_v0_commit(state,
                                          (uint8_t)(state->data_low & 0x3Fu),
                                          protocol_index);
                }
                if (state->bit_count == FIAT_V0_FRAME_BITS)
                {
                    /* M1 packet relaxation: the merged trailer low ends the
                     * burst before upstream's 71st bit (odd wire end byte). */
                    return fiat_v0_commit(
                        state,
                        fiat_v0_endbyte_from_70((uint8_t)(state->data_low & 0x3Fu)),
                        protocol_index);
                }
                if (state->bit_count == FIAT_V0_FRAME_BITS - 1u)
                {
                    /* Even wire end byte: the absorbed trailing 0 cell leaves
                     * only five end-byte bits in the collection. */
                    return fiat_v0_commit(
                        state,
                        fiat_v0_endbyte_from_69((uint8_t)(state->data_low & 0x1Fu)),
                        protocol_index);
                }
                if (state->bit_count < FIAT_V0_KEY_BITS)
                    state->step = FiatV0StepReset;
            }
            break;
        }

        default:
            state->step = FiatV0StepReset;
            break;
    }
    return false;
}

uint8_t subghz_decode_fiat_v0(uint16_t protocol_index, uint16_t pulse_count)
{
    fiat_v0_state_t state;
    memset(&state, 0, sizeof(state));
    state.step = FiatV0StepReset;
    state.manchester_state = ManchesterStateMid1;

    for (uint16_t i = 0; i < pulse_count; i++)
    {
        if (fiat_v0_feed(&state, (i & 1u) == 0u,
                         subghz_decenc_ctl.pulse_times[i], protocol_index))
            return 0;
    }

    /* Packet-end relaxation: the trailer low may be missing from the buffer
     * (capture cut at the packet limit); commit a complete collection. */
    if (state.step == FiatV0StepData)
    {
        if (state.bit_count == FIAT_V0_COMMIT_BITS)
            return fiat_v0_commit(&state, (uint8_t)(state.data_low & 0x3Fu),
                                  protocol_index) ? 0 : 1;
        if (state.bit_count == FIAT_V0_FRAME_BITS)
            return fiat_v0_commit(
                       &state,
                       fiat_v0_endbyte_from_70((uint8_t)(state.data_low & 0x3Fu)),
                       protocol_index)
                       ? 0
                       : 1;
        if (state.bit_count == FIAT_V0_FRAME_BITS - 1u)
            return fiat_v0_commit(
                       &state,
                       fiat_v0_endbyte_from_69((uint8_t)(state.data_low & 0x1Fu)),
                       protocol_index)
                       ? 0
                       : 1;
    }
    return 1;
}
