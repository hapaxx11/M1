/* See COPYING.txt for license details. */

/*
 * m1_magellan_decode.c
 *
 * M1 sub-ghz Magellan decoder
 * te_short=200µs, te_long=400µs, fixed 32-bit payload.
 *
 * BUG FIX: The original decoder delegated to subghz_decode_generic_pwm()
 * which uses STANDARD OOK PWM polarity:
 *   bit 0: SHORT HIGH + LONG LOW
 *   bit 1: LONG  HIGH + SHORT LOW
 *
 * Magellan uses INVERTED polarity relative to standard OOK PWM:
 *   bit 1: SHORT HIGH (200µs) + LONG  LOW (400µs)
 *   bit 0: LONG  HIGH (400µs) + SHORT LOW (200µs)
 *
 * The generic decoder also ignores the Magellan frame header that precedes
 * the 32 data bits.  The on-air Magellan frame is:
 *   1 × header burst  : 800µs HIGH + 200µs LOW
 *   12 × toggle pairs : 200µs HIGH + 200µs LOW (each)
 *   1 × header end    : 200µs HIGH + 400µs LOW
 *   1 × start bit     : 1200µs HIGH + 400µs LOW   ← key landmark
 *   32 × data bits
 *   1 × stop bit      : 200µs HIGH + 40000µs LOW
 *
 * This decoder scans for the 1200µs start bit HIGH pulse and begins
 * decoding data from the following pair, using inverted polarity.
 */

#include "m1_sub_ghz_decenc.h"

/* Fixed Magellan timings (µs).  Not overridable — the Magellan on-air
 * protocol is always 200/400 regardless of any registry TE field. */
#define MAG_TE_SHORT   200u
#define MAG_TE_LONG    400u
#define MAG_DELTA       80u   /* 20 % tolerance of TE_LONG */

/* Start bit: 3 × TE_LONG = 1200µs.  Accept [900, 1500] µs to tolerate
 * real-world drift while rejecting the 800µs header burst and 200/400µs
 * data pulses. */
#define MAG_START_MIN   900u
#define MAG_START_MAX  1500u

#define MAG_DATA_BITS    32u

uint8_t subghz_decode_magellan(uint16_t p, uint16_t pulsecount)
{
    uint16_t i;
    uint16_t data_start = 0; /* index of first data HIGH pulse */

    /* Step 1 — find the start bit by scanning even (HIGH-pulse) indices.
     * The start bit is the first HIGH pulse in the range [900, 1500] µs.
     * If no start bit is found (e.g. a bare data-only test buffer) data
     * decoding starts from index 0. */
    for (i = 0; i + 1 < pulsecount; i += 2)
    {
        uint16_t t = subghz_decenc_ctl.pulse_times[i];
        if (t >= MAG_START_MIN && t <= MAG_START_MAX)
        {
            data_start = i + 2; /* skip start-bit HIGH + LOW */
            break;
        }
    }

    /* Step 2 — decode 32 data bits with INVERTED Magellan polarity:
     *   bit 1: SHORT HIGH (≈200µs) + LONG  LOW (≈400µs)
     *   bit 0: LONG  HIGH (≈400µs) + SHORT LOW (≈200µs) */
    uint32_t code = 0;
    uint8_t  bits_count = 0;

    for (i = data_start; i + 1 < pulsecount && bits_count < MAG_DATA_BITS; i += 2)
    {
        uint16_t t_hi = subghz_decenc_ctl.pulse_times[i];
        uint16_t t_lo = subghz_decenc_ctl.pulse_times[i + 1];

        code <<= 1;

        if (get_diff(t_hi, MAG_TE_SHORT) < MAG_DELTA &&
            get_diff(t_lo, MAG_TE_LONG)  < MAG_DELTA)
        {
            code |= 1; /* bit 1: SHORT HIGH + LONG LOW (inverted Magellan) */
        }
        else if (get_diff(t_hi, MAG_TE_LONG)  < MAG_DELTA &&
                 get_diff(t_lo, MAG_TE_SHORT) < MAG_DELTA)
        {
            /* bit 0: LONG HIGH + SHORT LOW */
        }
        else
        {
            break; /* invalid pulse — abort */
        }
        bits_count++;
    }

    if (bits_count >= MAG_DATA_BITS)
    {
        subghz_decenc_ctl.n64_decodedvalue  = code;
        subghz_decenc_ctl.ndecodedbitlength = bits_count;
        subghz_decenc_ctl.ndecodeddelay     = 0;
        subghz_decenc_ctl.ndecodedprotocol  = p;
        return 0; /* success */
    }

    return 1; /* decode failed */
}
