/* See COPYING.txt for license details. */

/*
 * M1 Sub-GHz decoder for the ProtoPirate "Kia V5" protocol.
 *
 * Decode algorithm derived from RocketGod-git/ProtoPirate
 * protocols/kia_v5.c (GPLv3).  The TX encoder lives in
 * Sub_Ghz/subghz_proto_pirate.c; this file implements RX
 * identification only.
 *
 * Wire format (te 400/800 us, delta 150): ~200 high/low 400 us preamble
 * pairs, sync low 400 + high 800 + low 400 + high 400, then 67 Manchester
 * bits (64-bit key + 3-bit CRC; bit 1 = low->high, bit 0 = high->low) and
 * a low/high 400 us trailer.  The trailing 3 CRC bits are collected but
 * NOT validated on RX, mirroring upstream.
 *
 * Field recovery: the 64-bit wire key is byte-swapped and each byte
 * bit-reversed into the "yek" word; serial = (yek >> 32) & 0x0FFFFFFF,
 * button = (yek >> 60) & 0xF, and the low 32 bits of yek are the
 * mixer-encrypted rolling counter.
 *
 * Mixer key gating: counter recovery needs the KIA_KEY4 mixer key
 * (ProtoPirate keystore type 13), injected at build time as
 * m1_kia_v5_key_builtin.  When m1_kia_v5_key_builtin_available is false
 * the frame still commits — serial and button are recoverable without the
 * key — but the counter is reported as 0.
 *
 * Adaptation notes vs the upstream event-driven decoder:
 *  - M1 delivers one capture buffer per packet (the pulse handler splits
 *    at >= INTERPACKET_GAP_MIN), so each burst arrives as a separate
 *    buffer and the upstream state machine is re-run per buffer with
 *    stack-local state only.
 *  - Upstream commits when an out-of-tolerance pulse arrives in the data
 *    state.  M1 additionally commits at packet end when a complete
 *    (>= 64-bit) collection is in hand, because the inter-burst gap is
 *    consumed by the packet splitter and may not appear in the buffer.
 *
 * M1 Project -- Hapax fork
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "m1_sub_ghz_decenc.h"
#include "subghz_manchester_decoder.h"
#include "subghz_protopirate_keys_builtin.h"

#define KIA_V5_TE_SHORT 400u
#define KIA_V5_TE_LONG  800u
#define KIA_V5_TE_DELTA 150u

/* Upstream accepts the sync once more than 40 preamble low edges passed. */
#define KIA_V5_MIN_PREAMBLE_COUNT 40u
/* 64 key bits + 3 CRC bits; bits beyond 67 are dropped (upstream guard). */
#define KIA_V5_KEY_BITS   64u
#define KIA_V5_TOTAL_BITS 67u

typedef enum {
    KiaV5StepReset = 0,
    KiaV5StepCheckPreamble,
    KiaV5StepData,
} kia_v5_step_t;

typedef struct {
    kia_v5_step_t step;
    uint16_t te_last;
    uint16_t header_count;
    ManchesterState manchester_state;
    uint64_t decoded_data;
    uint64_t saved_key;
    uint8_t bit_count;
} kia_v5_state_t;

static bool kia_v5_is_short(uint16_t duration)
{
    return get_diff(duration, KIA_V5_TE_SHORT) < KIA_V5_TE_DELTA;
}

static bool kia_v5_is_long(uint16_t duration)
{
    return get_diff(duration, KIA_V5_TE_LONG) < KIA_V5_TE_DELTA;
}

/*--------------------------------------------------------------------*/
/* Pure frame helpers (exported for host tests)                        */
/*--------------------------------------------------------------------*/

static uint8_t kia_v5_rev8(uint8_t value)
{
    value = (uint8_t)(((value & 0xF0u) >> 4) | ((value & 0x0Fu) << 4));
    value = (uint8_t)(((value & 0xCCu) >> 2) | ((value & 0x33u) << 2));
    value = (uint8_t)(((value & 0xAAu) >> 1) | ((value & 0x55u) << 1));
    return value;
}

/* Upstream mixer_decode(), with the KIA_KEY4 mixer key passed in as the
 * 8 big-endian keystore bytes (keystore[i] = key >> ((7 - i) * 8)). */
uint16_t m1_kia_v5_mixer_decode(uint32_t encrypted, uint64_t mfkey)
{
    uint8_t keystore[8];
    for (uint8_t i = 0; i < 8u; i++)
        keystore[i] = (uint8_t)((mfkey >> ((7u - i) * 8u)) & 0xFFu);

    uint8_t s0 = (uint8_t)(encrypted & 0xFFu);
    uint8_t s1 = (uint8_t)((encrypted >> 8) & 0xFFu);
    uint8_t s2 = (uint8_t)((encrypted >> 16) & 0xFFu);
    uint8_t s3 = (uint8_t)((encrypted >> 24) & 0xFFu);

    int round_index = 1;
    for (uint8_t i = 0; i < 18u; i++)
    {
        uint8_t r = keystore[round_index];
        int steps = 8;
        while (steps > 0)
        {
            uint8_t base;
            if ((s3 & 0x40u) == 0u)
                base = (s3 & 0x02u) == 0u ? 0x74u : 0x2Eu;
            else
                base = (s3 & 0x02u) == 0u ? 0x3Au : 0x5Cu;

            if (s2 & 0x08u)
                base = (uint8_t)(((base >> 4) & 0x0Fu) | ((base & 0x0Fu) << 4));
            if (s1 & 0x01u)
                base = (uint8_t)((base & 0x3Fu) << 2);
            if (s0 & 0x01u)
                base = (uint8_t)(base << 1);

            const uint8_t temp = (uint8_t)(s3 ^ s1);
            s3 = (uint8_t)((s3 & 0x7Fu) << 1);
            if (s2 & 0x80u) s3 |= 0x01u;
            s2 = (uint8_t)((s2 & 0x7Fu) << 1);
            if (s1 & 0x80u) s2 |= 0x01u;
            s1 = (uint8_t)((s1 & 0x7Fu) << 1);
            if (s0 & 0x80u) s1 |= 0x01u;
            s0 = (uint8_t)((s0 & 0x7Fu) << 1);

            const uint8_t chk = (uint8_t)(base ^ (r ^ temp));
            if (chk & 0x80u) s0 |= 0x01u;
            r = (uint8_t)((r & 0x7Fu) << 1);
            steps--;
        }
        round_index = (round_index - 1) & 0x7;
    }
    return (uint16_t)(s0 + ((uint16_t)s1 << 8));
}

/* Field decomposition of a committed 64-bit wire key.  When the mixer key
 * is not available the counter is reported as 0 (serial and button do not
 * depend on it). */
void m1_kia_v5_decode_fields(uint64_t data, uint64_t mfkey,
                             bool mfkey_available, uint32_t *serial,
                             uint8_t *button, uint32_t *counter)
{
    uint64_t yek = 0;
    for (uint8_t i = 0; i < 8u; i++)
    {
        const uint8_t byte = (uint8_t)((data >> (i * 8u)) & 0xFFu);
        yek |= ((uint64_t)kia_v5_rev8(byte) << ((7u - i) * 8u));
    }

    *serial = (uint32_t)((yek >> 32) & 0x0FFFFFFFu);
    *button = (uint8_t)((yek >> 60) & 0x0Fu);
    *counter = mfkey_available
                   ? (uint32_t)m1_kia_v5_mixer_decode((uint32_t)(yek & 0xFFFFFFFFu),
                                                      mfkey)
                   : 0u;
}

/*--------------------------------------------------------------------*/
/* Commit                                                              */
/*--------------------------------------------------------------------*/

static bool kia_v5_commit(const kia_v5_state_t *state, uint16_t protocol_index)
{
    if (state->bit_count < KIA_V5_KEY_BITS)
        return false;

    uint32_t serial = 0;
    uint32_t counter = 0;
    uint8_t button = 0;
    m1_kia_v5_decode_fields(state->saved_key, m1_kia_v5_key_builtin,
                            m1_kia_v5_key_builtin_available,
                            &serial, &button, &counter);

    subghz_decenc_ctl.n64_decodedvalue = state->saved_key;
    subghz_decenc_ctl.n32_serialnumber = serial;
    subghz_decenc_ctl.n32_rollingcode = counter;
    subghz_decenc_ctl.n8_buttonid = button;
    subghz_decenc_ctl.ndecodedbitlength =
        state->bit_count > KIA_V5_TOTAL_BITS ? KIA_V5_TOTAL_BITS
                                             : state->bit_count;
    subghz_decenc_ctl.ndecodeddelay = 0;
    subghz_decenc_ctl.ndecodedprotocol = protocol_index;
    return true;
}

/*--------------------------------------------------------------------*/
/* Waveform state machine                                              */
/*--------------------------------------------------------------------*/

static void kia_v5_state_clear(kia_v5_state_t *state)
{
    state->step = KiaV5StepReset;
    state->te_last = 0;
    state->header_count = 0;
    state->decoded_data = 0;
    state->saved_key = 0;
    state->bit_count = 0;
    manchester_advance(state->manchester_state, ManchesterEventReset,
                       &state->manchester_state, NULL);
}

static void kia_v5_add_bit(kia_v5_state_t *state, bool bit)
{
    state->decoded_data = (state->decoded_data << 1) | (bit ? 1u : 0u);
    state->bit_count++;
}

static bool kia_v5_feed(kia_v5_state_t *state, bool level, uint16_t duration,
                        uint16_t protocol_index)
{
    switch (state->step)
    {
        case KiaV5StepReset:
            if (level && kia_v5_is_short(duration))
            {
                state->step = KiaV5StepCheckPreamble;
                state->te_last = duration;
                state->header_count = 1;
                state->bit_count = 0;
                state->decoded_data = 0;
                manchester_advance(state->manchester_state,
                                   ManchesterEventReset,
                                   &state->manchester_state, NULL);
            }
            break;

        case KiaV5StepCheckPreamble:
            if (level)
            {
                if (kia_v5_is_long(duration))
                {
                    if (state->header_count > KIA_V5_MIN_PREAMBLE_COUNT)
                    {
                        state->step = KiaV5StepData;
                        state->bit_count = 0;
                        state->decoded_data = 0;
                        state->saved_key = 0;
                        state->header_count = 0;
                    }
                    else
                    {
                        state->te_last = duration;
                    }
                }
                else if (kia_v5_is_short(duration))
                {
                    state->te_last = duration;
                }
                else
                {
                    state->step = KiaV5StepReset;
                }
            }
            else
            {
                if (kia_v5_is_short(duration) && kia_v5_is_short(state->te_last))
                {
                    state->header_count++;
                }
                else if (kia_v5_is_long(duration) && kia_v5_is_short(state->te_last))
                {
                    state->header_count++;
                }
                else if (kia_v5_is_long(state->te_last))
                {
                    state->header_count++;
                }
                else
                {
                    state->step = KiaV5StepReset;
                }
                state->te_last = duration;
            }
            break;

        case KiaV5StepData:
        {
            ManchesterEvent event;
            if (kia_v5_is_short(duration))
            {
                event = level ? ManchesterEventShortHigh : ManchesterEventShortLow;
            }
            else if (kia_v5_is_long(duration))
            {
                event = level ? ManchesterEventLongHigh : ManchesterEventLongLow;
            }
            else
            {
                /* Upstream commit trigger: an out-of-tolerance pulse (the
                 * trailer/inter-burst gap) with a complete collection. */
                if (kia_v5_commit(state, protocol_index))
                    return true;
                state->step = KiaV5StepReset;
                break;
            }

            bool data_bit;
            if (state->bit_count <= (KIA_V5_TOTAL_BITS - 1u) &&
                manchester_advance(state->manchester_state, event,
                                   &state->manchester_state, &data_bit))
            {
                kia_v5_add_bit(state, data_bit);
                if (state->bit_count == KIA_V5_KEY_BITS)
                {
                    state->saved_key = state->decoded_data;
                    state->decoded_data = 0;
                }
            }
            state->te_last = duration;
            break;
        }

        default:
            state->step = KiaV5StepReset;
            break;
    }
    return false;
}

uint8_t subghz_decode_kia_v5(uint16_t protocol_index, uint16_t pulse_count)
{
    kia_v5_state_t state;
    kia_v5_state_clear(&state);

    for (uint16_t i = 0; i < pulse_count; i++)
    {
        if (kia_v5_feed(&state, (i & 1u) == 0u,
                        subghz_decenc_ctl.pulse_times[i], protocol_index))
            return 0;
    }

    /* Packet-end commit: M1's packet splitter consumes the inter-burst
     * gap, so a complete collection is committed when the buffer runs
     * out without an out-of-tolerance pulse. */
    if (kia_v5_commit(&state, protocol_index))
        return 0;
    return 1;
}
