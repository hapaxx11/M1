/* See COPYING.txt for license details. */

/*
 * M1 Sub-GHz decoder for the ProtoPirate "Subaru" protocol.
 *
 * Decode algorithm derived from RocketGod-git/ProtoPirate
 * protocols/subaru.c (GPLv3).  The TX encoder lives in
 * Sub_Ghz/subghz_proto_pirate.c; this file implements RX
 * identification only.
 *
 * Subaru wire format (per burst): 75 preamble pairs of (low 1600 us,
 * high 1600 us), a 2800 us low gap, a 2800 us high sync, then 64 PWM
 * data bits MSB first — bit 1 = low 1600 + high 800, bit 0 = low 800 +
 * high 1600 (te 800/1600, te_delta 200) — and a trailing 2800 us low /
 * 2800 us high pair.  Bursts repeat back-to-back with no long gap.
 *
 * The 64-bit word carries: byte 0 low nibble = button, bytes 1..3 =
 * 24-bit serial, and a 16-bit rolling counter scattered across bytes
 * 4..7 (inverted-bit placement with a serial-derived rotation; see
 * m1_subaru_pp_decode_fields).
 *
 * Adaptation notes vs the upstream event-driven decoder:
 *  - M1's pulse handler splits captures at >= 1500 us, but re-opens the
 *    buffer when nothing decoded and the gap is < 5000 us, so te_long
 *    (1600 us) and the 2800 us gap/sync stay inside one buffer and the
 *    full burst arrives as a single capture.  The upstream state machine
 *    is re-run per buffer with stack state only.
 *  - Upstream has no checksum; it commits any 64..80-bit collection
 *    behind a valid preamble/gap/sync and reports the field-decode
 *    validity flag only for display.  That policy is kept: the commit
 *    is structural (preamble + timing), not cryptographic.
 *
 * M1 Project -- Hapax fork
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "m1_sub_ghz_decenc.h"

#define SUBARU_TE_SHORT 800u
#define SUBARU_TE_LONG  1600u
#define SUBARU_TE_DELTA 200u
#define SUBARU_GAP_US   2800u
#define SUBARU_SYNC_US  2800u
#define SUBARU_GAP_TOL  800u

/* Upstream requires header_count >= 0x15 preamble pairs before the gap. */
#define SUBARU_MIN_PREAMBLE 0x15u
#define SUBARU_BITS         64u
#define SUBARU_BITS_MAX     80u

typedef enum {
    SubaruStepReset = 0,
    SubaruStepCheckPreamble,
    SubaruStepFoundGap,
    SubaruStepFoundSync,
    SubaruStepSaveDuration,
} subaru_step_t;

typedef struct {
    subaru_step_t step;
    uint16_t te_last;
    uint64_t data;
    uint8_t bit_count;
    uint16_t header_count;
} subaru_state_t;

/*--------------------------------------------------------------------*/
/* Pure frame helpers (exported for host tests)                        */
/*--------------------------------------------------------------------*/

static void subaru_rotate_left_3bytes(uint8_t *b0, uint8_t *b1, uint8_t *b2,
                                      uint8_t count)
{
    for (uint8_t i = 0; i < count; i++)
    {
        const uint8_t t = *b0;
        *b0 = (uint8_t)((*b0 << 1) | (*b1 >> 7));
        *b1 = (uint8_t)((*b1 << 1) | (*b2 >> 7));
        *b2 = (uint8_t)((*b2 << 1) | (t >> 7));
    }
}

/* Field extraction, faithful port of upstream subaru_decode_fields_exact.
 * Always writes the outputs; the return value is the upstream
 * self-consistency flag (display-only upstream, kept for tests). */
bool m1_subaru_pp_decode_fields(uint64_t key, uint32_t *serial,
                                uint8_t *button, uint16_t *counter)
{
    uint8_t b[8];
    for (uint8_t i = 0; i < 8u; i++)
        b[i] = (uint8_t)(key >> (56u - i * 8u));

    const uint8_t b0 = b[0], b1 = b[1], b2 = b[2], b3 = b[3];
    const uint8_t b4 = b[4], b5 = b[5], b6 = b[6], b7 = b[7];

    uint8_t lo = 0;
    if ((b4 & 0x40u) == 0u) lo |= 0x01u;
    if ((b4 & 0x80u) == 0u) lo |= 0x02u;
    if ((b5 & 0x01u) == 0u) lo |= 0x04u;
    if ((b5 & 0x02u) == 0u) lo |= 0x08u;
    if ((b6 & 0x01u) == 0u) lo |= 0x10u;
    if ((b6 & 0x02u) == 0u) lo |= 0x20u;
    if ((b5 & 0x40u) == 0u) lo |= 0x40u;
    if ((b5 & 0x80u) == 0u) lo |= 0x80u;

    const uint8_t reg_sh1 =
        (uint8_t)(((b7 & 0x0Fu) << 4) | (b5 & 0x0Cu) | ((b6 >> 6) & 0x03u));
    const uint8_t reg_sh2 =
        (uint8_t)(((b6 & 0x3Cu) << 2) | ((b7 >> 4) & 0x0Fu));

    uint8_t ser0 = b3, ser1 = b1, ser2 = b2;
    /* The uint8_t cast of 4 + lo is upstream's (rotation wraps mod 256
     * for lo > 251); the encoders truncate identically. */
    subaru_rotate_left_3bytes(&ser0, &ser1, &ser2, (uint8_t)(4u + lo));

    const uint8_t t1 = (uint8_t)(ser1 ^ reg_sh1);
    const uint8_t t2 = (uint8_t)(ser2 ^ reg_sh2);

    const uint8_t hi =
        (uint8_t)(((t1 & 0x10u) == 0u ? 0x04u : 0u) |
                  ((t1 & 0x20u) == 0u ? 0x08u : 0u) |
                  ((t2 & 0x80u) == 0u ? 0x02u : 0u) |
                  ((t2 & 0x40u) == 0u ? 0x01u : 0u) |
                  ((t1 & 0x01u) == 0u ? 0x40u : 0u) |
                  ((t1 & 0x02u) == 0u ? 0x80u : 0u) |
                  ((t2 & 0x08u) == 0u ? 0x20u : 0u) |
                  ((t2 & 0x04u) == 0u ? 0x10u : 0u));

    const uint8_t local34 = ser0;

    const uint8_t x1 = (uint8_t)(b1 ^ reg_sh1);
    const uint8_t x2 = (uint8_t)(b2 ^ reg_sh2);
    const uint8_t reg_hi_inv = (uint8_t)~hi;
    const uint8_t lo_inv = (uint8_t)~lo;
    const uint8_t expect1 =
        (uint8_t)(((x1 ^ (uint8_t)(reg_hi_inv << 2)) & 0x30u) |
                  ((x1 ^ (uint8_t)(reg_hi_inv >> 6)) & 0x03u) |
                  ((x1 ^ (uint8_t)(lo_inv << 2)) & 0xCCu));
    const uint8_t expect2 =
        (uint8_t)(((x2 ^ (uint8_t)(reg_hi_inv >> 2)) & 0x0Cu) |
                  ((x2 ^ (uint8_t)(reg_hi_inv << 6)) & 0xC0u) |
                  ((x2 ^ (uint8_t)(lo_inv >> 2)) & 0x33u));

    const bool valid =
        (((uint8_t)(b1 ^ expect1) | (uint8_t)(b2 ^ expect2)) == 0u) &&
        ((uint8_t)(b0 ^ local34) == lo);

    if (button)
        *button = b0 & 0x0Fu;
    if (serial)
        *serial = ((uint32_t)b1 << 16) | ((uint32_t)b2 << 8) | (uint32_t)b3;
    if (counter)
        *counter = (uint16_t)((uint16_t)hi << 8 | lo);
    return valid;
}

/*--------------------------------------------------------------------*/
/* Commit                                                              */
/*--------------------------------------------------------------------*/

static bool subaru_commit(uint64_t data, uint8_t bit_count,
                          uint16_t protocol_index)
{
    uint32_t serial = 0;
    uint16_t counter = 0;
    uint8_t button = 0;
    (void)m1_subaru_pp_decode_fields(data, &serial, &button, &counter);

    subghz_decenc_ctl.n64_decodedvalue  = data;
    subghz_decenc_ctl.n32_serialnumber  = serial;
    subghz_decenc_ctl.n32_rollingcode   = counter;
    subghz_decenc_ctl.n8_buttonid       = button;
    subghz_decenc_ctl.ndecodedbitlength = bit_count;
    subghz_decenc_ctl.ndecodeddelay     = 0;
    subghz_decenc_ctl.ndecodedprotocol  = protocol_index;
    return true;
}

/*--------------------------------------------------------------------*/
/* Waveform state machine                                              */
/*--------------------------------------------------------------------*/

static void subaru_state_clear(subaru_state_t *state)
{
    memset(state, 0, sizeof(*state));
    state->step = SubaruStepReset;
}

static bool subaru_feed(subaru_state_t *state, bool level, uint16_t duration,
                        uint16_t protocol_index)
{
    switch (state->step)
    {
        case SubaruStepReset:
            if (level && get_diff(duration, SUBARU_TE_LONG) < SUBARU_TE_DELTA)
            {
                state->data = 0;
                state->bit_count = 0;
                state->header_count = 0;
                state->te_last = duration;
                state->step = SubaruStepCheckPreamble;
            }
            break;

        case SubaruStepCheckPreamble:
            if (level)
                break;
            if (get_diff(duration, SUBARU_TE_LONG) < SUBARU_TE_DELTA &&
                get_diff(state->te_last, SUBARU_TE_LONG) < SUBARU_TE_DELTA)
            {
                state->header_count++;
                break;
            }
            if (state->header_count >= SUBARU_MIN_PREAMBLE &&
                get_diff(duration, SUBARU_GAP_US) < SUBARU_GAP_TOL)
            {
                state->te_last = duration;
                state->step = SubaruStepFoundGap;
            }
            else
            {
                state->step = SubaruStepReset;
            }
            break;

        case SubaruStepFoundGap:
            if (level && get_diff(duration, SUBARU_SYNC_US) < SUBARU_GAP_TOL &&
                get_diff(state->te_last, SUBARU_GAP_US) < SUBARU_GAP_TOL)
            {
                state->step = SubaruStepFoundSync;
            }
            else
            {
                state->step = SubaruStepReset;
            }
            break;

        case SubaruStepFoundSync:
            if (!level && (get_diff(duration, SUBARU_TE_LONG) < SUBARU_TE_DELTA ||
                           get_diff(duration, SUBARU_TE_SHORT) < SUBARU_TE_DELTA))
            {
                state->te_last = duration;
                state->step = SubaruStepSaveDuration;
            }
            else
            {
                state->step = SubaruStepReset;
            }
            break;

        case SubaruStepSaveDuration:
        {
            subaru_step_t next_step = SubaruStepReset;
            bool bit = false;
            bool valid = false;

            if (level)
            {
                if (get_diff(duration, SUBARU_TE_LONG) < SUBARU_TE_DELTA)
                {
                    if (get_diff(state->te_last, SUBARU_TE_SHORT) < SUBARU_TE_DELTA)
                    {
                        bit = false;
                        valid = true;
                    }
                }
                else if (get_diff(duration, SUBARU_TE_SHORT) < SUBARU_TE_DELTA)
                {
                    if (get_diff(state->te_last, SUBARU_TE_LONG) < SUBARU_TE_DELTA)
                    {
                        bit = true;
                        valid = true;
                    }
                }
                if (valid)
                {
                    state->data = (state->data << 1) | (bit ? 1u : 0u);
                    state->bit_count++;
                    next_step = SubaruStepFoundSync;
                }
            }

            state->step = next_step;

            if (state->bit_count >= SUBARU_BITS &&
                state->bit_count <= SUBARU_BITS_MAX)
            {
                const uint8_t bit_count = state->bit_count;
                const uint64_t data = state->data;
                subaru_state_clear(state);
                return subaru_commit(data, bit_count, protocol_index);
            }
            break;
        }

        default:
            subaru_state_clear(state);
            break;
    }
    return false;
}

uint8_t subghz_decode_subaru_pp(uint16_t protocol_index, uint16_t pulse_count)
{
    subaru_state_t state;
    subaru_state_clear(&state);

    for (uint16_t i = 0; i < pulse_count; i++)
    {
        if (subaru_feed(&state, (i & 1u) == 0u,
                        subghz_decenc_ctl.pulse_times[i], protocol_index))
            return 0;
    }
    return 1;
}
