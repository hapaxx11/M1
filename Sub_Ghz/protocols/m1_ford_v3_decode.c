/* See COPYING.txt for license details. */

/*
 * m1_ford_v3_decode.c
 *
 * M1 Sub-GHz decoder for the ProtoPirate "Ford V3" automotive keyfob protocol.
 *
 * Reference implementation: RocketGod-git/ProtoPirate protocols/ford_v3.c
 * (te_short=240µs, te_long=480µs, 104 Manchester wire bits = 13 bytes, OOK/AM).
 * The protocol is entirely cipher-free; validation is purely structural.  There
 * are two field layouts sharing one wire frame: a "US" variant (plaintext
 * counter, full-byte button, flag high-bit set) and an "EU" variant (bit-inverted
 * counter, bit0 button).  The variant is chosen by whether the stricter US
 * validation passes, mirroring the reference's ford_v3_variant_from_saved_or_raw.
 *
 * Unlike the name-only placeholder automotive entries in this directory, this
 * decoder actually parses the on-air waveform so M1 can identify Ford V3 keyfobs
 * from a capture.
 *
 * Wire frame (13 bytes, MSB-first Manchester):
 *   raw[0]      = 0xFF  (marker)
 *   raw[1..4]   = serial / UID (32-bit, big-endian)
 *   raw[5]      = flag (US: bit7 must be set)
 *   raw[6]      = button (US: 0x01=Lock / 0x02=Unlock; EU: bit0 → Unlock/Lock)
 *   raw[7..8]   = counter (US: plain BE; EU: bitwise-inverted BE)
 *   raw[9..12]  = hop / remaining code
 *
 * The frame-parsing logic (m1_ford_v3_parse_frame) is pure and host-testable.
 *
 * M1 Project -- Hapax fork
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "m1_sub_ghz_decenc.h"

/* Timing (µs) — matches ProtoPirate reference. */
#define FORD_V3_TE_SHORT  240u
#define FORD_V3_TE_LONG   480u
#define FORD_V3_TE_DELTA  90u
#define FORD_V3_PREAMBLE_PULSES 30u

/* Frame geometry. */
#define FORD_V3_WIRE_BITS  104u
#define FORD_V3_WIRE_BYTES 13u
#define FORD_V3_WIRE_CELLS (FORD_V3_WIRE_BITS * 2u)   /* 208 Manchester cells */

/* Field constants. */
#define FORD_V3_MARKER      0xFFu
#define FORD_V3_BTN_LOCK    0x01u
#define FORD_V3_BTN_UNLOCK  0x02u
#define FORD_V3_VARIANT_EU  0u
#define FORD_V3_VARIANT_US  1u

/*============================================================================*/
/* Pure frame parsing (host-testable)                                          */
/*============================================================================*/

static bool ford_v3_us_valid(const uint8_t raw[FORD_V3_WIRE_BYTES])
{
    if (raw[0] != FORD_V3_MARKER)
        return false;
    if ((raw[6] != FORD_V3_BTN_LOCK) && (raw[6] != FORD_V3_BTN_UNLOCK))
        return false;
    if ((raw[5] & 0x80u) == 0u)
        return false;
    return true;
}

/**
 * Validate and parse a 13-byte Ford V3 frame.
 *
 * Returns true if @p raw is a structurally-valid Ford V3 frame (0xFF marker and
 * a sane serial), filling any non-NULL output pointers with the extracted
 * fields.  @p variant reports FORD_V3_VARIANT_US or FORD_V3_VARIANT_EU.
 *
 * Pure function — no global side effects; unit-tested on the host.
 */
bool m1_ford_v3_parse_frame(const uint8_t raw[FORD_V3_WIRE_BYTES],
                            uint8_t *variant, uint32_t *serial,
                            uint8_t *button, uint16_t *counter)
{
    if (raw == NULL)
        return false;
    if (raw[0] != FORD_V3_MARKER)
        return false;

    const uint32_t id = ((uint32_t)raw[1] << 24) | ((uint32_t)raw[2] << 16) |
                        ((uint32_t)raw[3] << 8) | raw[4];
    if (id == 0u || id == UINT32_MAX)
        return false;

    const bool is_us = ford_v3_us_valid(raw);

    if (variant) *variant = is_us ? FORD_V3_VARIANT_US : FORD_V3_VARIANT_EU;
    if (serial)  *serial  = id;
    if (is_us)
    {
        if (button)  *button  = raw[6];
        if (counter) *counter = (uint16_t)(((uint16_t)raw[7] << 8) | raw[8]);
    }
    else
    {
        if (button)  *button  = (raw[6] & 0x01u) ? FORD_V3_BTN_UNLOCK
                                                 : FORD_V3_BTN_LOCK;
        if (counter) *counter = (uint16_t)(((uint16_t)(uint8_t)~raw[7] << 8) |
                                           (uint8_t)~raw[8]);
    }
    return true;
}

/*============================================================================*/
/* Manchester cell helpers                                                     */
/*============================================================================*/

static inline bool ford_v3_is_short(uint16_t d)
{
    return get_diff(d, FORD_V3_TE_SHORT) < FORD_V3_TE_DELTA;
}

static inline bool ford_v3_is_long(uint16_t d)
{
    return get_diff(d, FORD_V3_TE_LONG) < FORD_V3_TE_DELTA;
}

static bool ford_v3_try_window(const uint8_t *cells, uint16_t cell_count,
                               uint16_t protocol_index, bool invert)
{
    if (cell_count < FORD_V3_WIRE_CELLS)
        return false;

    const uint8_t *w = &cells[cell_count - FORD_V3_WIRE_CELLS];

    uint8_t raw[FORD_V3_WIRE_BYTES] = {0};
    for (uint8_t b = 0; b < FORD_V3_WIRE_BITS; b++)
    {
        const uint8_t first  = w[b * 2u];
        const uint8_t second = w[b * 2u + 1u];
        if (first == second)
            return false;

        bool bit = (first != 0u);
        if (invert)
            bit = !bit;
        if (bit)
            raw[b >> 3] |= (uint8_t)(1u << (7u - (b & 7u)));
    }

    uint32_t serial = 0;
    uint16_t counter = 0;
    uint8_t  button = 0, variant = 0;
    if (!m1_ford_v3_parse_frame(raw, &variant, &serial, &button, &counter))
        return false;

    subghz_decenc_ctl.n64_decodedvalue  = ((uint64_t)serial << 16) | counter;
    subghz_decenc_ctl.n32_serialnumber  = serial;
    subghz_decenc_ctl.n32_rollingcode   = counter;
    subghz_decenc_ctl.n8_buttonid       = button;
    subghz_decenc_ctl.ndecodedbitlength = (uint16_t)FORD_V3_WIRE_BITS;
    subghz_decenc_ctl.ndecodeddelay     = 0;
    subghz_decenc_ctl.ndecodedprotocol  = protocol_index;
    return true;
}

/*============================================================================*/
/* M1 registry decode entry                                                     */
/*============================================================================*/

uint8_t subghz_decode_ford_v3(uint16_t p, uint16_t pulsecount)
{
    uint8_t  cells[FORD_V3_WIRE_CELLS];
    uint16_t cell_count = 0;
    uint8_t preamble_count = 0;
    bool frame_started = false;

    for (uint16_t i = 0; i < pulsecount; i++)
    {
        const uint16_t d = subghz_decenc_ctl.pulse_times[i];
        const uint8_t  level = (uint8_t)((i & 1u) == 0u);

        uint8_t push = 0;
        if (!frame_started)
        {
            if (ford_v3_is_short(d))
            {
                if (preamble_count < FORD_V3_PREAMBLE_PULSES)
                    preamble_count++;
                continue;
            }

            if (preamble_count >= FORD_V3_PREAMBLE_PULSES &&
                ford_v3_is_long(d))
            {
                frame_started = true;
                cell_count = 0;
                preamble_count = 0;
                push = 2;
            }
            else
            {
                preamble_count = 0;
                continue;
            }
        }
        else if (ford_v3_is_short(d))
        {
            push = 1;
        }
        else if (ford_v3_is_long(d))
        {
            push = 2;
        }
        else
        {
            frame_started = false;
            preamble_count = 0;
            cell_count = 0;
            continue;
        }

        for (uint8_t c = 0; c < push; c++)
        {
            if (cell_count < FORD_V3_WIRE_CELLS)
            {
                cells[cell_count++] = level;
            }
            else
            {
                memmove(cells, &cells[1], FORD_V3_WIRE_CELLS - 1u);
                cells[FORD_V3_WIRE_CELLS - 1u] = level;
            }

            if (cell_count == FORD_V3_WIRE_CELLS)
            {
                if (ford_v3_try_window(cells, cell_count, p, false) ||
                    ford_v3_try_window(cells, cell_count, p, true))
                {
                    return 0;
                }
            }
        }
    }

    return 1; /* no valid Ford V3 frame found */
}
