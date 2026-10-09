/* See COPYING.txt for license details. */

/*
 * m1_psa_decode.c
 *
 * M1 Sub-GHz decoder for the ProtoPirate "PSA" automotive keyfob protocol
 * (Peugeot / Citroën / Opel / Vauxhall).
 *
 * Reference implementation: RocketGod-git/ProtoPirate protocols/psa.c +
 * psa_crypto.c (te_short=250µs, te_long=500µs, 80 Manchester wire bits = 10
 * bytes, OOK/AM+FM).  Only the cipher-free "Direct XOR" decode path (reference
 * type 0x23) is ported here — it recovers the button, serial and rolling counter
 * through a pure XOR network guarded by a nibble checksum.  The reference's other
 * path (type 0x36) requires TEA and is deferred there to a brute-force plugin, so
 * no cipher is used on this decode/identify path.
 *
 * Unlike the name-only placeholder automotive entries in this directory, this
 * decoder actually parses the on-air waveform so M1 can identify PSA keyfobs from
 * a capture.
 *
 * Wire frame (10 bytes, MSB-first Manchester): buffer[0..7] = Key1 (big-endian),
 * buffer[8..9] = Key2/validation (big-endian).  Acceptance marker: low nibble of
 * buffer[1] must equal 0xA.
 *
 * The frame-parsing logic (m1_psa_parse_frame) is pure and host-testable.
 *
 * M1 Project -- Hapax fork
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "m1_sub_ghz_decenc.h"

/* Timing (µs) — matches ProtoPirate reference. */
#define PSA_TE_SHORT  250u
#define PSA_TE_LONG   500u
#define PSA_TE_DELTA  100u

/* Frame geometry. */
#define PSA_WIRE_BITS  80u
#define PSA_WIRE_BYTES 10u
#define PSA_WIRE_CELLS (PSA_WIRE_BITS * 2u)   /* 160 Manchester cells */
#define PSA_MARKER_NIBBLE 0x0Au
#define PSA_PREAMBLE_MIN_PULSES 143u

/*============================================================================*/
/* Pure frame parsing (host-testable)                                          */
/*============================================================================*/

/* Gate mirroring psa_direct_xor_allowed_by_key2(): decides whether the Direct
 * XOR (cipher-free) path may be attempted for a given Key2 high byte. */
static bool psa_direct_xor_allowed(uint8_t key2_high_byte)
{
    const uint8_t lo = (uint8_t)(key2_high_byte & 0x0Fu);
    if (lo < 3u)
        return true;
    if (lo < 7u && (key2_high_byte & 0x0Cu) != 0u)
        return true;
    return false;
}

/* Nibble checksum over bytes 2..7 (psa_calculate_checksum): result stored in the
 * high nibble of the returned byte. */
static uint8_t psa_nibble_checksum(const uint8_t *b)
{
    uint32_t checksum = 0;
    for (uint8_t i = 2; i < 8u; i++)
        checksum += (uint32_t)(b[i] & 0x0Fu) + (uint32_t)((b[i] >> 4) & 0x0Fu);
    return (uint8_t)((checksum * 0x10u) & 0xFFu);
}

/**
 * Validate and parse a 10-byte PSA wire frame via the cipher-free Direct-XOR
 * path.
 *
 * Returns true if @p frame is a structurally-valid PSA frame (marker nibble
 * 0xA, Direct-XOR gate satisfied and the nibble checksum validates), filling any
 * non-NULL output pointers with the extracted fields.
 *
 * Pure function — no global side effects; unit-tested on the host.
 */
bool m1_psa_parse_frame(const uint8_t frame[PSA_WIRE_BYTES],
                        uint32_t *serial, uint8_t *button, uint16_t *counter)
{
    if (frame == NULL)
        return false;

    /* Acceptance marker: low nibble of buffer[1] must be 0xA. */
    if ((frame[1] & 0x0Fu) != PSA_MARKER_NIBBLE)
        return false;
    if (!psa_direct_xor_allowed(frame[8]))
        return false;

    /* Work on a local copy with scratch room (reference uses a 48-byte buffer). */
    uint8_t b[16] = {0};
    memcpy(b, frame, PSA_WIRE_BYTES);

    /* Direct-XOR validation: high nibble of checksum must equal that of b[8]. */
    const uint8_t checksum = psa_nibble_checksum(b);
    if (((checksum ^ b[8]) & 0xF0u) != 0u)
        return false;

    /* Second-stage XOR un-mixing (psa_copy_reverse + psa_second_stage_xor). */
    uint8_t t[8];
    t[0] = b[5]; t[1] = b[4]; t[2] = b[3]; t[3] = b[2];
    t[4] = b[9]; t[5] = b[8]; t[6] = b[7]; t[7] = b[6];
    b[2] = (uint8_t)(t[0] ^ t[6]);
    b[3] = (uint8_t)(t[2] ^ t[0]);
    b[4] = (uint8_t)(t[6] ^ t[3]);
    b[5] = (uint8_t)(t[7] ^ t[1]);
    b[6] = (uint8_t)(t[3] ^ t[1]);
    b[7] = (uint8_t)(t[6] ^ t[4] ^ t[5]);

    /* Field extraction (psa_extract_fields_mode23). */
    const uint32_t id = ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 8) | b[4];
    if (id == 0u)
        return false;

    if (serial)  *serial  = id;
    if (button)  *button  = (uint8_t)(frame[8] & 0x0Fu);
    if (counter) *counter = (uint16_t)(((uint16_t)b[5] << 8) | b[6]);
    return true;
}

/*============================================================================*/
/* Manchester cell helpers                                                     */
/*============================================================================*/

static inline bool psa_is_short(uint16_t d)
{
    return get_diff(d, PSA_TE_SHORT) < PSA_TE_DELTA;
}

static inline bool psa_is_long(uint16_t d)
{
    return get_diff(d, PSA_TE_LONG) < PSA_TE_DELTA;
}

static inline bool psa_is_end(uint16_t d)
{
    return get_diff(d, 1000u) <= 199u;
}

static bool psa_try_window(const uint8_t *cells, uint16_t cell_count,
                           uint16_t protocol_index, bool invert)
{
    if (cell_count < PSA_WIRE_CELLS)
        return false;

    const uint8_t *w = &cells[cell_count - PSA_WIRE_CELLS];

    uint8_t frame[PSA_WIRE_BYTES] = {0};
    for (uint8_t bit_i = 0; bit_i < PSA_WIRE_BITS; bit_i++)
    {
        const uint8_t first  = w[bit_i * 2u];
        const uint8_t second = w[bit_i * 2u + 1u];
        if (first == second)
            return false;

        bool bit = (first != 0u);
        if (invert)
            bit = !bit;
        if (bit)
            frame[bit_i >> 3] |= (uint8_t)(1u << (7u - (bit_i & 7u)));
    }

    uint32_t serial = 0;
    uint16_t counter = 0;
    uint8_t  button = 0;
    if (!m1_psa_parse_frame(frame, &serial, &button, &counter))
        return false;

    uint64_t key1 = 0;
    for (uint8_t i = 0; i < 8u; i++)
        key1 = (key1 << 8) | frame[i];

    subghz_decenc_ctl.n64_decodedvalue  = key1;
    subghz_decenc_ctl.n32_serialnumber  = serial;
    subghz_decenc_ctl.n32_rollingcode   = counter;
    subghz_decenc_ctl.n8_buttonid       = button;
    subghz_decenc_ctl.ndecodedbitlength = (uint16_t)PSA_WIRE_BITS;
    subghz_decenc_ctl.ndecodeddelay     = 0;
    subghz_decenc_ctl.ndecodedprotocol  = protocol_index;
    return true;
}

/*============================================================================*/
/* M1 registry decode entry                                                     */
/*============================================================================*/

uint8_t subghz_decode_psa(uint16_t p, uint16_t pulsecount)
{
    uint8_t  cells[PSA_WIRE_CELLS];
    uint16_t cell_count = 0;
    uint16_t preamble_count = 0;
    bool frame_started = false;

    for (uint16_t i = 0; i < pulsecount; i++)
    {
        const uint16_t d = subghz_decenc_ctl.pulse_times[i];
        const uint8_t  level = (uint8_t)((i & 1u) == 0u);

        uint8_t push = 0;
        if (!frame_started)
        {
            if (psa_is_short(d))
            {
                if (preamble_count < PSA_PREAMBLE_MIN_PULSES)
                    preamble_count++;
                continue;
            }

            if (preamble_count >= PSA_PREAMBLE_MIN_PULSES && psa_is_long(d))
            {
                frame_started = true;
                cell_count = 0;
                preamble_count = 0;
                continue;
            }

            preamble_count = 0;
            continue;
        }

        if (psa_is_short(d))
        {
            push = 1;
        }
        else if (psa_is_long(d))
        {
            push = 2;
        }
        else
        {
            if (psa_is_end(d) && cell_count >= PSA_WIRE_CELLS &&
                (psa_try_window(cells, cell_count, p, false) ||
                 psa_try_window(cells, cell_count, p, true)))
            {
                return 0;
            }

            frame_started = false;
            preamble_count = 0;
            cell_count = 0;
            continue;
        }

        for (uint8_t c = 0; c < push; c++)
        {
            if (cell_count < PSA_WIRE_CELLS)
            {
                cells[cell_count++] = level;
            }
            else
            {
                memmove(cells, &cells[1], PSA_WIRE_CELLS - 1u);
                cells[PSA_WIRE_CELLS - 1u] = level;
            }

        }
    }

    return 1; /* no valid PSA frame found */
}
