/* See COPYING.txt for license details. */

/*
 * M1 Sub-GHz decoder for the ProtoPirate "Kia V0" protocol family.
 *
 * One decoder handles the four wire sub-types upstream groups under
 * "Kia V0": Kia (type 1), Suzuki (type 2), Honda V0 (type 3) and
 * Mitsubishi (type 4).  Decode algorithm derived from
 * RocketGod-git/ProtoPirate protocols/kia_v0.c (GPLv3).  The TX encoder
 * lives in Sub_Ghz/subghz_proto_pirate.c; this file implements RX
 * identification only.
 *
 * Adaptation notes vs the upstream event-driven decoder:
 *  - M1 delivers one capture buffer per packet (the pulse handler splits
 *    at >= INTERPACKET_GAP_MIN), so each frame copy arrives as a separate
 *    buffer and the upstream state machine is re-run per buffer.
 *  - Upstream commits Suzuki on the low+high 2000 us double gap, which
 *    straddles the M1 packet split (the merged low ends one buffer, the
 *    2000 us high starts the next).  As a documented relaxation, a
 *    64-bit Suzuki-shaped collection is also committed when the packet
 *    ends (or a merged trailing low arrives) in the data state; the
 *    shifted-CRC check still gates every commit.
 *  - Mitsubishi is told apart from Kia only by the tail copy's preamble
 *    length (72..88 pairs) matching the previous copy's key, so a small
 *    file-static cache carries the last Kia-shaped key across buffers.
 *    m1_kia_v0_decode_reset() clears it (used by host tests).
 *
 * M1 Project -- Hapax fork
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "m1_sub_ghz_decenc.h"
#include "subghz_blocks_math.h"

#define KIA_V0_TE_SHORT 250u
#define KIA_V0_TE_LONG  500u
#define KIA_V0_TE_DELTA 100u

#define KIA_V0_TYPE_KIA    1u
#define KIA_V0_TYPE_SUZUKI 2u
#define KIA_V0_TYPE_HONDA  3u
#define KIA_V0_TYPE_MITSU  4u

#define KIA_V0_BITS_KIA    61u
#define KIA_V0_BITS_SUZUKI 64u
#define KIA_V0_BITS_HONDA  72u

/* Commit gap for Kia/Honda copies: high 700..1700 us (upstream 1000 +/- span). */
#define KIA_V0_KIA_GAP_BASE 700u
#define KIA_V0_KIA_GAP_SPAN 1000u
/* Suzuki separator: low 2000 + high 2000 (strict +/- 500 us). */
#define KIA_V0_SUZUKI_GAP      2000u
#define KIA_V0_SUZUKI_GAP_SPAN 500u

/* Upstream requires more than 14 short preamble pairs before the sync. */
#define KIA_V0_MIN_PREAMBLE_PAIRS 14u
/* Mitsubishi tail copy carries an 80-pair preamble (window 72..88). */
#define KIA_V0_MITSU_PREAMBLE_MIN 72u
#define KIA_V0_MITSU_PREAMBLE_MAX 88u

typedef enum {
    KiaV0StepReset = 0,
    KiaV0StepPreamble,
    KiaV0StepSaveDuration,
    KiaV0StepCheckDuration,
} kia_v0_step_t;

typedef struct {
    kia_v0_step_t step;
    uint16_t te_last;
    uint64_t data;
    uint8_t bit_count;
    uint16_t preamble_pairs;
} kia_v0_state_t;

/* Last Kia-shaped key from the previous buffer (Mitsubishi detection). */
static uint64_t s_last_kia_data;
static bool s_have_last_kia;

void m1_kia_v0_decode_reset(void)
{
    s_last_kia_data = 0;
    s_have_last_kia = false;
}

static bool kia_v0_is_short(uint16_t duration)
{
    return get_diff(duration, KIA_V0_TE_SHORT) < KIA_V0_TE_DELTA;
}

static bool kia_v0_is_long(uint16_t duration)
{
    return get_diff(duration, KIA_V0_TE_LONG) < KIA_V0_TE_DELTA;
}

static bool kia_v0_is_kia_gap(uint16_t duration)
{
    return duration >= KIA_V0_KIA_GAP_BASE &&
           (uint32_t)(duration - KIA_V0_KIA_GAP_BASE) <= KIA_V0_KIA_GAP_SPAN;
}

static bool kia_v0_is_suzuki_gap(uint16_t duration)
{
    return duration >= KIA_V0_SUZUKI_GAP &&
           (uint32_t)(duration - KIA_V0_SUZUKI_GAP) <= KIA_V0_SUZUKI_GAP_SPAN;
}

/*--------------------------------------------------------------------*/
/* Pure frame helpers (exported for host tests)                        */
/*--------------------------------------------------------------------*/

static uint8_t kia_v0_rev8(uint8_t value)
{
    value = (uint8_t)(((value & 0xF0u) >> 4) | ((value & 0x0Fu) << 4));
    value = (uint8_t)(((value & 0xCCu) >> 2) | ((value & 0x33u) << 2));
    value = (uint8_t)(((value & 0xAAu) >> 1) | ((value & 0x55u) << 1));
    return value;
}

/* CRC-8 (poly 0x7F, init 0x00) over bits 8..55 of the 61-bit Kia word. */
uint8_t m1_kia_v0_wire_crc(uint64_t data)
{
    uint8_t buf[6];
    for (uint8_t i = 0; i < 6u; i++)
        buf[i] = (uint8_t)(data >> (48u - i * 8u));
    return subghz_protocol_blocks_crc8(buf, sizeof(buf), 0x7Fu, 0x00u);
}

bool m1_kia_v0_kia_crc_valid(uint64_t data)
{
    return m1_kia_v0_wire_crc(data) == (uint8_t)(data & 0xFFu);
}

static bool kia_v0_suzuki_shifted_crc_valid(uint64_t shifted)
{
    const uint32_t lo = (uint32_t)(shifted & 0xFFFFFFFFu);
    const uint32_t hi = (uint32_t)(shifted >> 32);
    const uint32_t r3 = ((lo >> 16) | (hi << 16)) & ~0xF0000000u;
    const uint32_t r2 = (lo >> 12) & 0x0Fu;
    uint32_t r1 = (hi >> 12) & 0xFFFFu;
    const uint8_t r4 = (uint8_t)((lo >> 4) & 0xFFu);

    r1 = ((r1 & 0xFFu) << 8) | ((r1 >> 8) & 0xFFu);

    const uint8_t buf[6] = {
        (uint8_t)(r1 & 0xFFu),
        (uint8_t)((r1 >> 8) & 0xFFu),
        (uint8_t)((r3 >> 20) & 0xFFu),
        (uint8_t)((r3 >> 12) & 0xFFu),
        (uint8_t)((r3 >> 4) & 0xFFu),
        (uint8_t)(((r3 << 4) | r2) & 0xFFu),
    };
    return subghz_protocol_blocks_crc8(buf, sizeof(buf), 0x7Fu, 0x00u) == r4;
}

/* The Suzuki sync can swallow one wire bit; upstream tries the raw word
 * and the word shifted left by one. */
bool m1_kia_v0_suzuki_resolve(uint64_t data, uint64_t *shifted)
{
    if (kia_v0_suzuki_shifted_crc_valid(data))
    {
        *shifted = data;
        return true;
    }
    const uint64_t from_wire = data << 1u;
    if (kia_v0_suzuki_shifted_crc_valid(from_wire))
    {
        *shifted = from_wire;
        return true;
    }
    return false;
}

uint64_t m1_kia_v0_honda_transform(uint64_t data)
{
    uint64_t key = 0;
    for (uint8_t i = 0; i < 8u; i++)
        key = (key << 8) | kia_v0_rev8((uint8_t)(data >> (56u - i * 8u)));
    return key;
}

bool m1_kia_v0_honda_key_valid(uint64_t key)
{
    return ((key >> 60) == 0x0Fu) && (((key >> 56) & 0x0Fu) == 0u) &&
           (((key >> 8) & 0x0Fu) == 0x0Au);
}

/*--------------------------------------------------------------------*/
/* Commit                                                              */
/*--------------------------------------------------------------------*/

static bool kia_v0_commit(uint64_t data, uint8_t type, uint16_t bit_count,
                          uint16_t protocol_index)
{
    uint32_t serial = 0;
    uint32_t counter = 0;
    uint8_t button = 0;

    if (type == KIA_V0_TYPE_HONDA)
    {
        serial = ((uint32_t)((data >> 32) & 0xFFu) << 16) |
                 ((uint32_t)((data >> 24) & 0xFFu) << 8) |
                 (uint32_t)((data >> 16) & 0xFFu);
        counter = ((uint32_t)kia_v0_rev8((uint8_t)(data >> 48)) << 8) |
                  (uint32_t)kia_v0_rev8((uint8_t)(data >> 40));
        button = (uint8_t)(((data >> 8) & 0xFFu) >> 5);
    }
    else if (type == KIA_V0_TYPE_SUZUKI)
    {
        serial = (uint32_t)((data >> 16) & 0x0FFFFFFFu);
        button = (uint8_t)((data >> 12) & 0x0Fu);
        counter = (uint32_t)((data >> 44) & 0xFFFFu);
    }
    else /* KIA_V0_TYPE_KIA / KIA_V0_TYPE_MITSU */
    {
        serial = (uint32_t)((data >> 12) & 0x0FFFFFFFu);
        button = (uint8_t)((data >> 8) & 0x0Fu);
        counter = (uint32_t)((data >> 40) & 0xFFFFu);
    }

    subghz_decenc_ctl.n64_decodedvalue = data;
    subghz_decenc_ctl.n32_serialnumber = serial;
    subghz_decenc_ctl.n32_rollingcode = counter;
    /* Sub-type rides the capture->type channel (key2 bits 16..23); the save
     * path persists it as the upstream-compatible "Type:" field. */
    subghz_decenc_ctl.n32_key2 = (uint64_t)type << 16;
    subghz_decenc_ctl.n8_buttonid = button;
    subghz_decenc_ctl.ndecodedbitlength = bit_count;
    subghz_decenc_ctl.ndecodeddelay = 0;
    subghz_decenc_ctl.ndecodedprotocol = protocol_index;
    return true;
}

/*--------------------------------------------------------------------*/
/* Frame finishers                                                     */
/*--------------------------------------------------------------------*/

static bool kia_v0_finish_kia_or_honda(kia_v0_state_t *state,
                                       uint16_t protocol_index)
{
    if (state->bit_count != KIA_V0_BITS_KIA)
        return false;

    const uint64_t data = state->data;
    if (m1_kia_v0_kia_crc_valid(data))
    {
        uint8_t type = KIA_V0_TYPE_KIA;
        if (s_have_last_kia && s_last_kia_data == data &&
            state->preamble_pairs >= KIA_V0_MITSU_PREAMBLE_MIN &&
            state->preamble_pairs <= KIA_V0_MITSU_PREAMBLE_MAX)
        {
            type = KIA_V0_TYPE_MITSU;
        }
        s_last_kia_data = data;
        s_have_last_kia = true;
        return kia_v0_commit(data, type, KIA_V0_BITS_KIA, protocol_index);
    }

    const uint64_t key =
        m1_kia_v0_honda_transform(data & 0x0FFFFFFFFFFFFFFFuLL);
    if (m1_kia_v0_honda_key_valid(key))
        return kia_v0_commit(key, KIA_V0_TYPE_HONDA, KIA_V0_BITS_HONDA,
                             protocol_index);
    return false;
}

/* Eager Honda check run after every data bit, as upstream does: Honda V0
 * frames commit as soon as all 61 wire bits are in, without waiting for a
 * trailing gap.  A Kia-valid CRC short-circuits (Kia commits at the gap). */
static bool kia_v0_try_honda(kia_v0_state_t *state, uint16_t protocol_index)
{
    if (state->bit_count != KIA_V0_BITS_KIA)
        return false;
    if (m1_kia_v0_kia_crc_valid(state->data))
        return false;
    const uint64_t key =
        m1_kia_v0_honda_transform(state->data & 0x0FFFFFFFFFFFFFFFuLL);
    if (!m1_kia_v0_honda_key_valid(key))
        return false;
    return kia_v0_commit(key, KIA_V0_TYPE_HONDA, KIA_V0_BITS_HONDA,
                         protocol_index);
}

static bool kia_v0_finish_suzuki(kia_v0_state_t *state, uint16_t protocol_index)
{
    if (state->bit_count != KIA_V0_BITS_SUZUKI)
        return false;
    uint64_t shifted = 0;
    if (!m1_kia_v0_suzuki_resolve(state->data, &shifted))
        return false;
    return kia_v0_commit(shifted, KIA_V0_TYPE_SUZUKI, KIA_V0_BITS_SUZUKI,
                         protocol_index);
}

/*--------------------------------------------------------------------*/
/* Waveform state machine                                              */
/*--------------------------------------------------------------------*/

static void kia_v0_state_clear(kia_v0_state_t *state)
{
    state->step = KiaV0StepReset;
    state->te_last = 0;
    state->data = 0;
    state->bit_count = 0;
    state->preamble_pairs = 0;
}

static void kia_v0_add_bit(kia_v0_state_t *state, uint8_t bit)
{
    state->data = (state->data << 1) | bit;
    state->bit_count++;
}

static bool kia_v0_feed(kia_v0_state_t *state, bool level, uint16_t duration,
                        uint16_t protocol_index)
{
    switch (state->step)
    {
        case KiaV0StepReset:
            if (level && kia_v0_is_short(duration))
            {
                state->step = KiaV0StepPreamble;
                state->te_last = duration;
                state->preamble_pairs = 0;
            }
            break;

        case KiaV0StepPreamble:
            if (level)
            {
                if (kia_v0_is_short(duration) || kia_v0_is_long(duration))
                    state->te_last = duration;
                else
                    kia_v0_state_clear(state);
            }
            else if (kia_v0_is_short(duration) && kia_v0_is_short(state->te_last))
            {
                state->preamble_pairs++;
            }
            else if (kia_v0_is_long(duration) && kia_v0_is_long(state->te_last))
            {
                if (state->preamble_pairs > KIA_V0_MIN_PREAMBLE_PAIRS)
                {
                    state->data = 0;
                    state->bit_count = 0;
                    kia_v0_add_bit(state, 1u);
                    kia_v0_add_bit(state, 1u);
                    state->step = KiaV0StepSaveDuration;
                }
                else
                {
                    kia_v0_state_clear(state);
                }
            }
            else
            {
                kia_v0_state_clear(state);
            }
            break;

        case KiaV0StepSaveDuration:
            if (!level)
            {
                /* A merged trailing low (e.g. Suzuki's last data low absorbed
                 * by the 2000 us gap when M1's packet split separates the
                 * double gap) lands here; try the CRC-gated Suzuki commit
                 * before discarding the collection. */
                if (kia_v0_finish_suzuki(state, protocol_index))
                    return true;
                kia_v0_state_clear(state);
                break;
            }
            if (kia_v0_is_kia_gap(duration))
            {
                if (kia_v0_finish_kia_or_honda(state, protocol_index))
                    return true;
                kia_v0_state_clear(state);
                break;
            }
            if (kia_v0_is_suzuki_gap(duration) &&
                kia_v0_is_suzuki_gap(state->te_last))
            {
                if (kia_v0_finish_suzuki(state, protocol_index))
                    return true;
                kia_v0_state_clear(state);
                break;
            }
            if (kia_v0_is_short(duration) || kia_v0_is_long(duration))
            {
                state->te_last = duration;
                state->step = KiaV0StepCheckDuration;
            }
            else
            {
                kia_v0_state_clear(state);
            }
            break;

        case KiaV0StepCheckDuration:
            if (level)
            {
                kia_v0_state_clear(state);
                break;
            }
            if (kia_v0_is_short(state->te_last) && kia_v0_is_short(duration))
            {
                kia_v0_add_bit(state, 0u);
                if (kia_v0_try_honda(state, protocol_index))
                    return true;
                state->step = KiaV0StepSaveDuration;
                break;
            }
            if (kia_v0_is_long(state->te_last) && kia_v0_is_long(duration))
            {
                kia_v0_add_bit(state, 1u);
                if (kia_v0_try_honda(state, protocol_index))
                    return true;
                state->step = KiaV0StepSaveDuration;
                break;
            }
            if (kia_v0_is_suzuki_gap(duration))
            {
                state->te_last = duration;
                state->step = KiaV0StepSaveDuration;
                break;
            }
            kia_v0_state_clear(state);
            break;

        default:
            kia_v0_state_clear(state);
            break;
    }
    return false;
}

uint8_t subghz_decode_kia_v0(uint16_t protocol_index, uint16_t pulse_count)
{
    kia_v0_state_t state;
    kia_v0_state_clear(&state);

    for (uint16_t i = 0; i < pulse_count; i++)
    {
        if (kia_v0_feed(&state, (i & 1u) == 0u,
                        subghz_decenc_ctl.pulse_times[i], protocol_index))
            return 0;
    }

    /* Packet-end Suzuki commit (M1 split relaxation): the upstream double
     * gap is divided across two capture buffers, so a complete CRC-valid
     * 64-bit collection is committed when the buffer runs out. */
    if (kia_v0_finish_suzuki(&state, protocol_index))
        return 0;
    return 1;
}
