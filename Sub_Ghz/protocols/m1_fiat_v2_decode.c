/* See COPYING.txt for license details. */

/*
 * m1_fiat_v2_decode.c
 *
 * M1 Sub-GHz decoder for the ProtoPirate "Fiat V2" automotive keyfob
 * protocol (FCA / Fiat-Chrysler-Automobiles rolling-code remotes — Fiat,
 * Jeep, Dodge, RAM, Alfa Romeo).
 *
 * Reference implementation: RocketGod-git/ProtoPirate protocols/fiat_v2.c
 * (te_short=210µs, te_long=420µs, te_delta=100µs, 112 wire bits = 14 bytes,
 * Manchester/biphase AM).  Only the *decode* path is ported here — the frame
 * is read and its fields are extracted; no rolling-code cipher is required to
 * identify a captured signal (the cipher is only needed to brute-force keys).
 *
 * Unlike the other ProtoPirate automotive entries in this directory (which are
 * name-only placeholders that never decode), this decoder actually parses the
 * on-air waveform so M1 can identify Fiat V2 keyfobs from a capture.
 *
 * Wire format (MSB-first, 14 bytes):
 *   raw[0]      = 0x00  (marker 0)
 *   raw[1]      = 0x01  (marker 1)
 *   raw[2..5]   = UID (serial, big-endian)
 *   raw[6]      = type nibble (0xDn => FCA layout)
 *   raw[7]      = button (sel = raw[7] >> 6: 1=Trunk, 2=Lock, 3=Unlock)
 *   raw[8..]    = counter / hop (layout depends on FCA vs non-FCA)
 *
 * The frame-parsing logic (m1_fiat_v2_parse_frame) is pure and host-testable.
 *
 * M1 Project -- Hapax fork
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "m1_sub_ghz_decenc.h"

/* Timing (µs) — matches ProtoPirate reference. */
#define FIAT_V2_TE_SHORT   210u
#define FIAT_V2_TE_LONG    420u
#define FIAT_V2_TE_DELTA   100u

/* Frame geometry. */
#define FIAT_V2_WIRE_BITS  112u
#define FIAT_V2_WIRE_BYTES 14u
#define FIAT_V2_WIRE_CELLS (FIAT_V2_WIRE_BITS * 2u)   /* 224 Manchester cells */

/* Field constants (ProtoPirate fiat_v2.c). */
#define FIAT_V2_MARKER0       0x00u
#define FIAT_V2_MARKER1       0x01u
#define FIAT_V2_BTN_SHIFT     6u
#define FIAT_V2_BUTTON_TRUNK  0x1u
#define FIAT_V2_BUTTON_LOCK   0x2u
#define FIAT_V2_BUTTON_UNLOCK 0x3u
#define FIAT_V2_FCA_TYPE_NIB  0xD0u

/*============================================================================*/
/* Pure frame parsing (host-testable)                                          */
/*============================================================================*/

static bool fiat_v2_button_valid(uint8_t button)
{
    const uint8_t sel = (uint8_t)(button >> FIAT_V2_BTN_SHIFT);
    return sel == FIAT_V2_BUTTON_LOCK || sel == FIAT_V2_BUTTON_UNLOCK ||
           sel == FIAT_V2_BUTTON_TRUNK;
}

static uint32_t fiat_v2_uid(const uint8_t raw[FIAT_V2_WIRE_BYTES])
{
    return ((uint32_t)raw[2] << 24) | ((uint32_t)raw[3] << 16) |
           ((uint32_t)raw[4] << 8) | raw[5];
}

static bool fiat_v2_is_fca(const uint8_t raw[FIAT_V2_WIRE_BYTES])
{
    return (raw[6] & 0xF0u) == FIAT_V2_FCA_TYPE_NIB;
}

static uint32_t fiat_v2_hop(const uint8_t raw[FIAT_V2_WIRE_BYTES])
{
    if (fiat_v2_is_fca(raw))
    {
        return ((uint32_t)raw[10] << 24) | ((uint32_t)raw[11] << 16) |
               ((uint32_t)raw[12] << 8) | raw[13];
    }
    return ((uint32_t)raw[9] << 24) | ((uint32_t)raw[10] << 16) |
           ((uint32_t)raw[11] << 8) | raw[12];
}

static uint32_t fiat_v2_counter(const uint8_t raw[FIAT_V2_WIRE_BYTES])
{
    if (fiat_v2_is_fca(raw))
    {
        const uint32_t raw_cnt = ((uint32_t)raw[8] << 6) | (uint32_t)(raw[9] >> 2);
        return (~raw_cnt) & 0x3FFFu;
    }
    const uint32_t raw_cnt = ((uint32_t)(raw[7] & 0x3Fu) << 5) |
                             (uint32_t)(raw[8] >> 3);
    return (~raw_cnt) & 0x7FFu;
}

/**
 * Validate and parse a 14-byte Fiat V2 frame.
 *
 * Returns true if @p raw is a structurally-valid Fiat V2 frame, filling any
 * non-NULL output pointers with the extracted fields.
 *
 * Pure function — no global side effects; unit-tested on the host.
 */
bool m1_fiat_v2_parse_frame(const uint8_t raw[FIAT_V2_WIRE_BYTES],
                            uint32_t *uid, uint8_t *button,
                            uint32_t *hop, uint32_t *counter)
{
    if (raw == NULL)
        return false;

    if (raw[0] != FIAT_V2_MARKER0 || raw[1] != FIAT_V2_MARKER1)
        return false;
    if (!fiat_v2_button_valid(raw[7]))
        return false;

    const uint32_t id = fiat_v2_uid(raw);
    if (id == 0u || id == UINT32_MAX)
        return false;

    if (uid)     *uid     = id;
    if (button)  *button  = raw[7];
    if (hop)     *hop     = fiat_v2_hop(raw);
    if (counter) *counter = fiat_v2_counter(raw);
    return true;
}

/*============================================================================*/
/* Manchester cell helpers                                                     */
/*============================================================================*/

static inline bool fiat_v2_is_short(uint16_t d)
{
    return get_diff(d, FIAT_V2_TE_SHORT) < FIAT_V2_TE_DELTA;
}

static inline bool fiat_v2_is_long(uint16_t d)
{
    return get_diff(d, FIAT_V2_TE_LONG) < FIAT_V2_TE_DELTA;
}

/*
 * Attempt to decode the trailing FIAT_V2_WIRE_CELLS window of @p cells into a
 * valid frame, trying both Manchester polarities.  Returns true and stores the
 * result in subghz_decenc_ctl on success.
 */
static bool fiat_v2_try_window(const uint8_t *cells, uint16_t cell_count,
                               uint16_t protocol_index, bool invert)
{
    if (cell_count < FIAT_V2_WIRE_CELLS)
        return false;

    const uint8_t *w = &cells[cell_count - FIAT_V2_WIRE_CELLS];

    uint8_t raw[FIAT_V2_WIRE_BYTES] = {0};
    for (uint8_t b = 0; b < FIAT_V2_WIRE_BITS; b++)
    {
        const uint8_t first  = w[b * 2u];
        const uint8_t second = w[b * 2u + 1u];
        if (first == second)
            return false; /* not a valid Manchester cell pair */

        bool bit = (first != 0u);
        if (invert)
            bit = !bit;
        if (bit)
            raw[b >> 3] |= (uint8_t)(1u << (7u - (b & 7u)));
    }

    uint32_t uid = 0, hop = 0, counter = 0;
    uint8_t  button = 0;
    if (!m1_fiat_v2_parse_frame(raw, &uid, &button, &hop, &counter))
        return false;

    subghz_decenc_ctl.n64_decodedvalue  = ((uint64_t)uid << 32) | hop;
    subghz_decenc_ctl.n32_serialnumber  = uid;
    subghz_decenc_ctl.n32_rollingcode   = counter;
    subghz_decenc_ctl.n8_buttonid       = button;
    subghz_decenc_ctl.ndecodedbitlength = (uint16_t)FIAT_V2_WIRE_BITS;
    subghz_decenc_ctl.ndecodeddelay     = 0;
    subghz_decenc_ctl.ndecodedprotocol  = protocol_index;
    return true;
}

/*============================================================================*/
/* M1 registry decode entry                                                     */
/*============================================================================*/

uint8_t subghz_decode_fiat_v2(uint16_t p, uint16_t pulsecount)
{
    /*
     * Reconstruct the Manchester cell stream from the captured pulse array.
     * subghz_decenc_ctl.pulse_times[] alternates HIGH (even index) / LOW (odd
     * index) durations.  Each short pulse contributes one cell of that pulse's
     * level; each long pulse contributes two.  A sliding window of the most
     * recent 224 cells is tried after every push, in both polarities, so the
     * decoder self-aligns regardless of the leading signal phase.
     */
    uint8_t  cells[FIAT_V2_WIRE_CELLS];
    uint16_t cell_count = 0;

    for (uint16_t i = 0; i < pulsecount; i++)
    {
        const uint16_t d = subghz_decenc_ctl.pulse_times[i];
        const uint8_t  level = (uint8_t)((i & 1u) == 0u); /* even=HIGH, odd=LOW */

        uint8_t push = 0;
        if (fiat_v2_is_short(d))
            push = 1;
        else if (fiat_v2_is_long(d))
            push = 2;
        else
        {
            cell_count = 0; /* out-of-range pulse: resync */
            continue;
        }

        for (uint8_t c = 0; c < push; c++)
        {
            if (cell_count < FIAT_V2_WIRE_CELLS)
            {
                cells[cell_count++] = level;
            }
            else
            {
                memmove(cells, &cells[1], FIAT_V2_WIRE_CELLS - 1u);
                cells[FIAT_V2_WIRE_CELLS - 1u] = level;
            }

            if (cell_count == FIAT_V2_WIRE_CELLS)
            {
                if (fiat_v2_try_window(cells, cell_count, p, false) ||
                    fiat_v2_try_window(cells, cell_count, p, true))
                {
                    return 0; /* success */
                }
            }
        }
    }

    return 1; /* no valid Fiat V2 frame found */
}
