/* See COPYING.txt for license details. */

/*
 * m1_renault_v1_decode.c
 *
 * M1 Sub-GHz decoder for the ProtoPirate "Renault V1" automotive keyfob
 * protocol (HITAG2-based rolling-code remotes).
 *
 * Reference implementation: RocketGod-git/ProtoPirate protocols/renault_v1.c
 * (te_short=125µs, te_long=250µs, 104 Manchester wire bits = 16-bit header +
 * 88-bit payload, OOK/AM).  Only the *decode/identify* path is ported here — it
 * is fully cipher-free: the HITAG2 cipher in the reference is used only by the
 * encoder and by key brute-force, never on the receive path.  A frame is
 * accepted on a 16-bit header match plus an XOR8 checksum; serial, button,
 * rolling counter and the (still-encrypted) hop field are then sliced out.
 *
 * Unlike the name-only placeholder automotive entries in this directory, this
 * decoder actually parses the on-air waveform so M1 can identify Renault V1
 * keyfobs from a capture.
 *
 * Wire frame (post-Manchester, bits received inverted on air — the dual-polarity
 * decode below recovers the logical frame):
 *   full[0..1]   = 16-bit header, must equal 0x0001
 *   full[2..12]  = 11-byte payload (raw[0..10]):
 *     raw[0..3]  = serial / UID (32-bit, big-endian)
 *     raw[4]     = button (high nibble) + counter high bits (low nibble)
 *     raw[5..9]  = counter low bits + 32-bit hop (encrypted) + 2-bit tail
 *     raw[10]    = XOR8 checksum of raw[0..9]
 *
 * The frame-parsing logic (m1_renault_v1_parse_frame) is pure and host-testable.
 *
 * M1 Project -- Hapax fork
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "m1_sub_ghz_decenc.h"

/* Timing (µs) — matches ProtoPirate reference (te_short=125, te_long=250). */
#define RENAULT_V1_TE_SHORT  125u
#define RENAULT_V1_TE_LONG   250u
#define RENAULT_V1_TE_DELTA  60u

/* Frame geometry. */
#define RENAULT_V1_WIRE_BITS  104u
#define RENAULT_V1_WIRE_BYTES 13u
#define RENAULT_V1_WIRE_CELLS (RENAULT_V1_WIRE_BITS * 2u)  /* 208 Manchester cells */
#define RENAULT_V1_PAYLOAD_BYTES 11u

/* Field constants. */
#define RENAULT_V1_HEADER     0x0001u

/*============================================================================*/
/* Pure frame parsing (host-testable)                                          */
/*============================================================================*/

static uint8_t renault_v1_xor8(const uint8_t raw[RENAULT_V1_PAYLOAD_BYTES])
{
    uint8_t v = 0;
    for (uint8_t i = 0; i < 10u; i++)
        v ^= raw[i];
    return v;
}

/**
 * Validate and parse a 13-byte Renault V1 wire frame (16-bit header followed by
 * an 11-byte payload).
 *
 * Returns true if @p full is a structurally-valid Renault V1 frame (header
 * 0x0001 and a matching XOR8 checksum), filling any non-NULL output pointers
 * with the extracted fields.
 *
 * Pure function — no global side effects; unit-tested on the host.
 */
bool m1_renault_v1_parse_frame(const uint8_t full[RENAULT_V1_WIRE_BYTES],
                               uint32_t *serial, uint8_t *button,
                               uint16_t *counter, uint32_t *hop)
{
    if (full == NULL)
        return false;

    const uint16_t header = (uint16_t)(((uint16_t)full[0] << 8) | full[1]);
    if (header != RENAULT_V1_HEADER)
        return false;

    const uint8_t *raw = &full[2];
    if (renault_v1_xor8(raw) != raw[10])
        return false;

    const uint32_t id = ((uint32_t)raw[0] << 24) | ((uint32_t)raw[1] << 16) |
                        ((uint32_t)raw[2] << 8) | raw[3];
    if (id == 0u || id == UINT32_MAX)
        return false;

    if (serial)  *serial  = id;
    if (button)  *button  = (uint8_t)((raw[4] >> 4) & 0x0Fu);
    if (counter) *counter = (uint16_t)(((uint16_t)(raw[4] & 0x0Fu) << 6) |
                                       (uint16_t)(raw[5] >> 2));
    if (hop)
    {
        *hop = ((uint32_t)(raw[5] & 0x03u) << 30) | ((uint32_t)raw[6] << 22) |
               ((uint32_t)raw[7] << 14) | ((uint32_t)raw[8] << 6) |
               (uint32_t)(raw[9] >> 2);
    }
    return true;
}

/*============================================================================*/
/* Manchester cell helpers                                                     */
/*============================================================================*/

static inline bool renault_v1_is_short(uint16_t d)
{
    return get_diff(d, RENAULT_V1_TE_SHORT) < RENAULT_V1_TE_DELTA;
}

static inline bool renault_v1_is_long(uint16_t d)
{
    return get_diff(d, RENAULT_V1_TE_LONG) < RENAULT_V1_TE_DELTA;
}

static bool renault_v1_try_window(const uint8_t *cells, uint16_t cell_count,
                                  uint16_t protocol_index, bool invert)
{
    if (cell_count < RENAULT_V1_WIRE_CELLS)
        return false;

    const uint8_t *w = &cells[cell_count - RENAULT_V1_WIRE_CELLS];

    uint8_t full[RENAULT_V1_WIRE_BYTES] = {0};
    for (uint8_t b = 0; b < RENAULT_V1_WIRE_BITS; b++)
    {
        const uint8_t first  = w[b * 2u];
        const uint8_t second = w[b * 2u + 1u];
        if (first == second)
            return false; /* not a valid Manchester cell pair */

        bool bit = (first != 0u);
        if (invert)
            bit = !bit;
        if (bit)
            full[b >> 3] |= (uint8_t)(1u << (7u - (b & 7u)));
    }

    uint32_t serial = 0, hop = 0;
    uint16_t counter = 0;
    uint8_t  button = 0;
    if (!m1_renault_v1_parse_frame(full, &serial, &button, &counter, &hop))
        return false;

    subghz_decenc_ctl.n64_decodedvalue  = ((uint64_t)serial << 32) | hop;
    subghz_decenc_ctl.n32_serialnumber  = serial;
    subghz_decenc_ctl.n32_rollingcode   = counter;
    subghz_decenc_ctl.n8_buttonid       = button;
    subghz_decenc_ctl.ndecodedbitlength = (uint16_t)RENAULT_V1_WIRE_BITS;
    subghz_decenc_ctl.ndecodeddelay     = 0;
    subghz_decenc_ctl.ndecodedprotocol  = protocol_index;
    return true;
}

/*============================================================================*/
/* M1 registry decode entry                                                     */
/*============================================================================*/

uint8_t subghz_decode_renault_v1(uint16_t p, uint16_t pulsecount)
{
    uint8_t  cells[RENAULT_V1_WIRE_CELLS];
    uint16_t cell_count = 0;

    for (uint16_t i = 0; i < pulsecount; i++)
    {
        const uint16_t d = subghz_decenc_ctl.pulse_times[i];
        const uint8_t  level = (uint8_t)((i & 1u) == 0u); /* even=HIGH, odd=LOW */

        uint8_t push = 0;
        if (renault_v1_is_short(d))
            push = 1;
        else if (renault_v1_is_long(d))
            push = 2;
        else
        {
            cell_count = 0; /* out-of-range pulse: resync */
            continue;
        }

        for (uint8_t c = 0; c < push; c++)
        {
            if (cell_count < RENAULT_V1_WIRE_CELLS)
            {
                cells[cell_count++] = level;
            }
            else
            {
                memmove(cells, &cells[1], RENAULT_V1_WIRE_CELLS - 1u);
                cells[RENAULT_V1_WIRE_CELLS - 1u] = level;
            }

            if (cell_count == RENAULT_V1_WIRE_CELLS)
            {
                if (renault_v1_try_window(cells, cell_count, p, false) ||
                    renault_v1_try_window(cells, cell_count, p, true))
                {
                    return 0; /* success */
                }
            }
        }
    }

    return 1; /* no valid Renault V1 frame found */
}
