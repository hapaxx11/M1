/* See COPYING.txt for license details. */

/*
 * M1 Sub-GHz decoder for the ProtoPirate "Renault V0" protocol.
 *
 * Decode algorithm derived from RocketGod-git/ProtoPirate
 * protocols/renault_v0.c (GPLv3).  The TX encoder lives in
 * Sub_Ghz/subghz_proto_pirate.c; this file implements RX
 * identification only.
 *
 * Renault V0 wire format (per burst; 3 bursts per transmission with a
 * 25 ms inter-burst gap): 1000 us sync high, 16 Manchester-coded
 * preamble one-bits, then 82 Manchester-coded data bits (64-bit key
 * word + 18-bit "Key2" second word), an optional 125 us trailing high,
 * and a 250 us final low.  Manchester symbols are te_short = 125 us and
 * te_long = 250 us with te_delta = 69 us; the bit state machine is the
 * upstream 4-state table {0x01, 0x91, 0x9B, 0xFB} shifted by the
 * classified event code (0/2/4/6).
 *
 * Adaptation notes vs the upstream event-driven decoder:
 *  - M1 delivers one capture buffer per burst (the pulse handler splits
 *    at >= INTERPACKET_GAP_MIN and the 25 ms inter-burst gap is a hard
 *    split), so the upstream per-instance state machine is re-run per
 *    buffer with stack state only.
 *  - Upstream evaluates the collected bits only when its bit buffer
 *    overflows or an unclassifiable pulse arrives.  M1 additionally
 *    evaluates at packet end: the final burst's trailing 250 us low is
 *    classifiable and the inter-burst gap is consumed by the packet
 *    split, so a complete collection can otherwise go unevaluated.
 *  - Upstream skips ALL leading one-bits as preamble, so a frame whose
 *    key-word MSB (serial bit 23) is set has that bit absorbed into the
 *    preamble run and is rejected by the bit-count gate; the quirk is
 *    kept for fidelity.
 *
 * M1 Project -- Hapax fork
 */

#include <stdbool.h>
#include <stdint.h>
#include "m1_sub_ghz_decenc.h"

#define RENAULT_V0_TE_SHORT     125u
#define RENAULT_V0_TE_LONG      250u
#define RENAULT_V0_TE_DELTA     69u
#define RENAULT_V0_SYNC_MIN_US  800u

#define RENAULT_V0_MIN_BITS     82u  /* 64-bit key + 18-bit Key2 */
#define RENAULT_V0_KEY2_BITS    18u
#define RENAULT_V0_BIT_LIMIT    109u /* upstream 0x6D */
#define RENAULT_V0_BITS_MAX     112u /* upstream decoded-bits buffer 0x70 */
#define RENAULT_V0_MIN_PREAMBLE 9u   /* upstream requires > 9 leading ones */

typedef enum {
    RenaultV0StepReset = 0,
    RenaultV0StepData,
} renault_v0_step_t;

typedef struct {
    renault_v0_step_t step;
    uint8_t manchester_state;
    uint8_t decoded_bits[RENAULT_V0_BITS_MAX];
    uint8_t decoded_bit_count;
} renault_v0_state_t;

/* Upstream renault_v0_decoder_state_table, indexed by state & 3; the
 * classified event code (0/2/4/6) is the shift selecting the next state. */
static const uint8_t renault_v0_state_table[4] = {0x01u, 0x91u, 0x9Bu, 0xFBu};

/*--------------------------------------------------------------------*/
/* Pulse classification (faithful port of upstream classify_event)     */
/*--------------------------------------------------------------------*/

static bool renault_v0_classify_event(uint16_t duration, bool level,
                                      uint8_t *event_code)
{
    if (duration <= RENAULT_V0_TE_SHORT - 1u)
    {
        if ((uint16_t)(RENAULT_V0_TE_SHORT - duration) > RENAULT_V0_TE_DELTA)
            return false;
        *event_code = (uint8_t)((level ? 0u : 1u) << 1);
        return true;
    }

    if (duration <= RENAULT_V0_TE_LONG - 1u)
    {
        const uint16_t short_delta = (uint16_t)(duration - RENAULT_V0_TE_SHORT);
        const uint16_t long_delta  = (uint16_t)(RENAULT_V0_TE_LONG - duration);
        if (short_delta <= RENAULT_V0_TE_DELTA)
        {
            /* Ambiguous window (both te within delta) resolves to LONG. */
            *event_code = (long_delta > RENAULT_V0_TE_DELTA)
                              ? (uint8_t)((level ? 0u : 1u) << 1)
                              : (uint8_t)(level ? 4u : 6u);
            return true;
        }
        if (long_delta <= RENAULT_V0_TE_DELTA)
        {
            *event_code = (uint8_t)(level ? 4u : 6u);
            return true;
        }
        return false;
    }

    if ((uint16_t)(duration - RENAULT_V0_TE_LONG) > RENAULT_V0_TE_DELTA)
        return false;
    *event_code = (uint8_t)(level ? 4u : 6u);
    return true;
}

/*--------------------------------------------------------------------*/
/* Pure frame helpers (exported for host tests)                        */
/*--------------------------------------------------------------------*/

/* Bytewise XOR fold over the 64-bit key word (big-endian) plus the two
 * Key2 slices (key2>>10, key2>>2); Key2 bits 1..0 carry the fold's top
 * 2 bits and are compared separately by the validity gate. */
uint8_t m1_renault_v0_checksum(uint64_t data, uint32_t key2)
{
    uint8_t checksum = 0;
    for (uint8_t i = 0; i < 8u; i++)
        checksum ^= (uint8_t)(data >> ((7u - i) * 8u));
    checksum ^= (uint8_t)((key2 >> 10u) & 0xFFu);
    checksum ^= (uint8_t)((key2 >> 2u) & 0xFFu);
    return checksum;
}

bool m1_renault_v0_button_valid(uint8_t button)
{
    return button == 0x05u || button == 0x06u || button == 0x0Au;
}

/* Upstream "type 13" validity gate: C1 (fold low 6 bits == 0x13),
 * C2 (Key2 low 2 bits == fold top 2 bits), and a known button code. */
bool m1_renault_v0_frame_valid(uint64_t data, uint32_t key2)
{
    const uint8_t checksum = m1_renault_v0_checksum(data, key2);
    if ((checksum & 0x3Fu) != 0x13u)
        return false;
    if ((key2 & 0x03u) != (uint32_t)(checksum >> 6u))
        return false;
    return m1_renault_v0_button_valid((uint8_t)(data >> 32u));
}

/* Upstream decode_candidate: skip the leading-one preamble run, take
 * the next 64 bits as the key word and the 18 after that as Key2, then
 * apply the validity gate. */
bool m1_renault_v0_parse_frame(const uint8_t *bits, uint8_t bit_count,
                               uint64_t *out_data, uint32_t *out_key2)
{
    if (bit_count < RENAULT_V0_MIN_BITS)
        return false;

    uint8_t preamble = 0;
    while (preamble < bit_count && bits[preamble] == 1u)
        preamble++;
    if (preamble <= RENAULT_V0_MIN_PREAMBLE)
        return false;
    if ((uint8_t)(bit_count - preamble) < RENAULT_V0_MIN_BITS)
        return false;

    uint64_t data = 0;
    for (uint8_t i = 0; i < 64u; i++)
        data = (data << 1) | (uint64_t)(bits[preamble + i] & 1u);

    uint32_t key2 = 0;
    for (uint8_t i = 0; i < RENAULT_V0_KEY2_BITS; i++)
        key2 = (key2 << 1) | (uint32_t)(bits[preamble + 64u + i] & 1u);

    if (!m1_renault_v0_frame_valid(data, key2))
        return false;

    *out_data = data;
    *out_key2 = key2;
    return true;
}

/*--------------------------------------------------------------------*/
/* Commit                                                              */
/*--------------------------------------------------------------------*/

static bool renault_v0_decode_candidate(renault_v0_state_t *state,
                                        uint16_t protocol_index)
{
    uint64_t data = 0;
    uint32_t key2 = 0;
    if (!m1_renault_v0_parse_frame(state->decoded_bits,
                                   state->decoded_bit_count,
                                   &data, &key2))
        return false;

    subghz_decenc_ctl.n64_decodedvalue = data;
    subghz_decenc_ctl.n32_serialnumber = (uint32_t)(data >> 40u);
    subghz_decenc_ctl.n32_rollingcode  = (uint32_t)((data >> 24u) & 0xFFu);
    subghz_decenc_ctl.n8_buttonid      = (uint8_t)(data >> 32u);
    /* Second word: 18-bit Key2, matching the upstream "Key2:" field. */
    subghz_decenc_ctl.n32_key2         = (uint64_t)key2;
    subghz_decenc_ctl.ndecodedbitlength = RENAULT_V0_MIN_BITS;
    subghz_decenc_ctl.ndecodeddelay    = 0;
    subghz_decenc_ctl.ndecodedprotocol = protocol_index;
    return true;
}

/*--------------------------------------------------------------------*/
/* Waveform state machine                                              */
/*--------------------------------------------------------------------*/

static void renault_v0_state_clear(renault_v0_state_t *state)
{
    state->step = RenaultV0StepReset;
    state->manchester_state = 1u;
    state->decoded_bit_count = 0;
}

static void renault_v0_arm_data(renault_v0_state_t *state)
{
    state->step = RenaultV0StepData;
    state->decoded_bit_count = 0;
    state->manchester_state = 1u;
}

static bool renault_v0_feed(renault_v0_state_t *state, bool level,
                            uint16_t duration, uint16_t protocol_index)
{
    if (state->step == RenaultV0StepReset)
    {
        if (level && duration >= RENAULT_V0_SYNC_MIN_US)
            renault_v0_arm_data(state);
        return false;
    }

    if (state->decoded_bit_count > RENAULT_V0_BIT_LIMIT)
    {
        if (renault_v0_decode_candidate(state, protocol_index))
            return true;
        renault_v0_state_clear(state);
        return false;
    }

    uint8_t event_code = 0;
    if (!renault_v0_classify_event(duration, level, &event_code))
    {
        if (renault_v0_decode_candidate(state, protocol_index))
            return true;
        /* An over-long high is the next burst's sync: re-arm on it. */
        if (level && duration >= RENAULT_V0_SYNC_MIN_US)
            renault_v0_arm_data(state);
        else
            state->step = RenaultV0StepReset;
        return false;
    }

    const uint8_t cur = state->manchester_state & 0x03u;
    const uint8_t next = (renault_v0_state_table[cur] >> event_code) & 0x03u;
    if (next == cur)
        return false; /* ignored event, no state change */

    state->manchester_state = next;
    if (next == 1u || next == 2u)
    {
        state->decoded_bits[state->decoded_bit_count] =
            (uint8_t)(next == 1u ? 1u : 0u);
        state->decoded_bit_count++;
    }
    return false;
}

uint8_t subghz_decode_renault_v0(uint16_t protocol_index, uint16_t pulse_count)
{
    renault_v0_state_t state;
    renault_v0_state_clear(&state);

    for (uint16_t i = 0; i < pulse_count; i++)
    {
        if (renault_v0_feed(&state, (i & 1u) == 0u,
                            subghz_decenc_ctl.pulse_times[i], protocol_index))
            return 0;
    }

    /* Packet-end commit: a burst can end on a classifiable trailing low
     * (or with the inter-burst gap consumed by the packet split), leaving
     * a complete collection that no further pulse would evaluate. */
    if (state.step == RenaultV0StepData &&
        renault_v0_decode_candidate(&state, protocol_index))
        return 0;
    return 1;
}
