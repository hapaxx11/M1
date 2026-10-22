/* See COPYING.txt for license details. */

/*
 * M1 Sub-GHz decoder for the ProtoPirate "Kia V1" protocol.
 *
 * Decode algorithm derived from RocketGod-git/ProtoPirate protocols/kia_v1.c
 * (GPLv3): 800/1600 us PWM half-symbols (te_delta 200), ~90 low/high 1600 us
 * preamble pairs, a low 800 us sync, then 56 manchester data bits on top of a
 * seeded MSB (bit 56 = 1) for 57 bits total.  The TX encoder lives in
 * Sub_Ghz/subghz_proto_pirate.c; this file implements RX identification only.
 *
 * Adaptation notes vs the upstream event-driven decoder:
 *  - M1 delivers one capture buffer per packet (the pulse handler splits at
 *    >= INTERPACKET_GAP_MIN, and the 25 ms inter-burst gap always splits), so
 *    each frame burst arrives as a separate buffer and the upstream state
 *    machine is re-scanned per buffer with stack state only.
 *  - Upstream feeds only the pulses AFTER the sync into Flipper's manchester
 *    machine, leaving it a half-symbol short of the data boundary (the sync
 *    low is the second half of the seeded bit-56 symbol).  Also, when data
 *    bit 55 is 0 the sync merges on air with the first data half into one
 *    1600 us low, and every leading "01" data bit pair keeps merging into
 *    preamble-shaped long/long pairs, so a streaming decoder cannot find the
 *    symbol boundary.  Because M1 hands us the whole buffer, the sync region
 *    is resolved deterministically: the data stream starts at the low pulse
 *    right after the last high-long of the preamble run (the seed bit 56 is
 *    always 1, so that low pulse always carries the seed symbol's second
 *    half), and the number of leading "01" bit pairs hidden in the long run
 *    is recovered by half-symbol accounting: the data stream from that anchor
 *    to the end of the burst is always exactly 113 half-symbols (1 sync half
 *    + 56 bits x 2 halves), so each 4 missing halves are one hidden pair.
 *  - Upstream computes CRC-4 (XOR-fold each byte's nibbles, plus 1) for
 *    display only and never rejects on it; this port commits the same way
 *    for frames anchored with zero hidden pairs, while a frame that required
 *    hidden-pair reconstruction must additionally pass CRC-4 to commit
 *    (m1_kia_v1_parse_fields reports the CRC and its validity).
 *  - The upstream pulse->manchester-event mapping is inverted
 *    (pp_manchester_event in protocols_common.h: a HIGH pulse maps to
 *    ManchesterEventShortLow); this port keeps that mapping.  It round-trips
 *    exactly against the wire format emitted by encode_kia_v1().
 *
 * M1 Project -- Hapax fork
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "m1_sub_ghz_decenc.h"
#include "subghz_manchester_decoder.h"

#define KIA_V1_TE_SHORT 800u
#define KIA_V1_TE_LONG  1600u
#define KIA_V1_TE_DELTA 200u

#define KIA_V1_BITS 57u /* 1 seeded MSB + 56 wire bits */

/* Upstream requires header_count > 70 long preamble pulses before the sync. */
#define KIA_V1_MIN_PREAMBLE 70u
/* Half-symbols from the sync half to the end of the burst: 1 + 56 x 2. */
#define KIA_V1_DATA_HALVES 113u
/* Leading "01" bit pairs that can hide inside the preamble-shaped long run. */
#define KIA_V1_HIDDEN_PAIRS_MAX 27u
/* A run of this many consecutive longs after the data anchor can only be the
 * next burst's preamble: 56 data bits merge into at most 56 long pulses. */
#define KIA_V1_BURST_RUN_LIMIT 64u

static bool kia_v1_is_short(uint16_t duration)
{
    return get_diff(duration, KIA_V1_TE_SHORT) < KIA_V1_TE_DELTA;
}

static bool kia_v1_is_long(uint16_t duration)
{
    return get_diff(duration, KIA_V1_TE_LONG) < KIA_V1_TE_DELTA;
}

/* Upstream pp_manchester_event(): inverted level/event mapping (a HIGH pulse
 * produces ManchesterEventShortLow/LongLow). */
static ManchesterEvent kia_v1_event(uint16_t duration, bool level)
{
    if (kia_v1_is_short(duration))
        return level ? ManchesterEventShortLow : ManchesterEventShortHigh;
    if (kia_v1_is_long(duration))
        return level ? ManchesterEventLongLow : ManchesterEventLongHigh;
    return ManchesterEventReset;
}

/*--------------------------------------------------------------------*/
/* Pure frame helpers (exported for host tests)                        */
/*--------------------------------------------------------------------*/

/* Upstream kia_v1_crc4(): XOR-fold the nibbles of each byte, add the offset. */
uint8_t m1_kia_v1_crc4(const uint8_t *bytes, uint8_t count, uint8_t offset)
{
    uint8_t crc = 0;
    for (uint8_t i = 0; i < count; i++)
        crc = (uint8_t)(crc ^ ((bytes[i] & 0x0Fu) ^ (bytes[i] >> 4)));
    return (uint8_t)((crc + offset) & 0x0Fu);
}

/* Field layout of the committed 57-bit word (bit 56 = seeded 1):
 * serial = bits 24..55, button = bits 16..23, counter = low byte at bits
 * 8..15 plus high nibble at bits 4..7, CRC-4 = low nibble. */
void m1_kia_v1_parse_fields(uint64_t data, uint32_t *serial, uint8_t *button,
                            uint16_t *counter, uint8_t *crc, bool *crc_valid)
{
    const uint32_t s = (uint32_t)(data >> 24); /* bit 56 truncated away */
    const uint8_t b = (uint8_t)((data >> 16) & 0xFFu);
    const uint16_t c =
        (uint16_t)((((data >> 4) & 0xFu) << 8) | ((data >> 8) & 0xFFu));
    const uint8_t bytes[7] = {
        (uint8_t)(s >> 24), (uint8_t)(s >> 16), (uint8_t)(s >> 8),
        (uint8_t)s,         b,                  (uint8_t)(c & 0xFFu),
        (uint8_t)((c >> 8) & 0x0Fu),
    };
    const uint8_t computed = m1_kia_v1_crc4(bytes, sizeof(bytes), 1u);

    if (serial) *serial = s;
    if (button) *button = b;
    if (counter) *counter = c;
    if (crc) *crc = computed;
    if (crc_valid) *crc_valid = (computed == (uint8_t)(data & 0x0Fu));
}

/*--------------------------------------------------------------------*/
/* Commit                                                              */
/*--------------------------------------------------------------------*/

static void kia_v1_commit(uint64_t data, uint16_t protocol_index)
{
    uint32_t serial = 0;
    uint16_t counter = 0;
    uint8_t button = 0;
    m1_kia_v1_parse_fields(data, &serial, &button, &counter, NULL, NULL);

    subghz_decenc_ctl.n64_decodedvalue = data;
    subghz_decenc_ctl.n32_serialnumber = serial;
    subghz_decenc_ctl.n32_rollingcode = counter;
    subghz_decenc_ctl.n8_buttonid = button;
    subghz_decenc_ctl.ndecodedbitlength = KIA_V1_BITS;
    subghz_decenc_ctl.ndecodeddelay = 0;
    subghz_decenc_ctl.ndecodedprotocol = protocol_index;
}

/*--------------------------------------------------------------------*/
/* Buffer scan                                                         */
/*--------------------------------------------------------------------*/

uint8_t subghz_decode_kia_v1(uint16_t protocol_index, uint16_t pulse_count)
{
    const uint16_t n = pulse_count;
    uint16_t pos = 0;

    while (pos < n)
    {
        /* Preamble scan (upstream KiaV1DecoderStepReset/CheckPreamble):
         * enter on a high long, count low longs preceded by a long, and
         * anchor the data stream at the low pulse after the last high long
         * once the first short pulse arrives past the pair threshold. */
        uint8_t step = 0; /* 0 = reset, 1 = preamble */
        uint16_t te_last = 0;
        uint16_t header_count = 0;
        int32_t last_high_long = -1;
        int32_t anchor = -1;
        uint16_t i = pos;
        for (; i < n; i++)
        {
            const uint16_t d = subghz_decenc_ctl.pulse_times[i];
            const bool level = (i & 1u) == 0u;
            if (step == 0)
            {
                if (level && kia_v1_is_long(d))
                {
                    step = 1;
                    te_last = d;
                    header_count = 0;
                    last_high_long = (int32_t)i;
                }
                continue;
            }
            if (kia_v1_is_long(d))
            {
                if (level)
                {
                    last_high_long = (int32_t)i;
                }
                else if (kia_v1_is_long(te_last))
                {
                    header_count++;
                    te_last = d;
                }
                else
                {
                    step = 0;
                }
            }
            else if (kia_v1_is_short(d))
            {
                if (header_count > KIA_V1_MIN_PREAMBLE && last_high_long >= 0 &&
                    (uint16_t)(last_high_long + 1) < n)
                {
                    anchor = last_high_long + 1;
                    break;
                }
                if (!level)
                    step = 0;
                else
                    te_last = d;
            }
            else
            {
                step = 0;
            }
        }
        if (anchor < 0)
            return 1;

        /* Bound the data region: a run of >= KIA_V1_BURST_RUN_LIMIT longs
         * can only be the next burst's preamble. */
        uint16_t region_end = n;
        uint16_t run = 0;
        for (i = (uint16_t)anchor; i < n; i++)
        {
            if (kia_v1_is_long(subghz_decenc_ctl.pulse_times[i]))
            {
                if (++run >= KIA_V1_BURST_RUN_LIMIT)
                {
                    region_end = (uint16_t)(i - (KIA_V1_BURST_RUN_LIMIT - 1u));
                    break;
                }
            }
            else
            {
                run = 0;
            }
        }

        /* Half-symbol accounting: a complete burst carries exactly
         * KIA_V1_DATA_HALVES halves from the anchor onward; every 4 missing
         * halves are one "01" bit pair that merged into the preamble-shaped
         * long run.  An invalid LOW at the end of the region absorbs the
         * last data half (a gap merging with the final low half-symbol). */
        uint16_t halves = 0;
        bool stopped_on_invalid_low = false;
        for (i = (uint16_t)anchor; i < region_end; i++)
        {
            const uint16_t d = subghz_decenc_ctl.pulse_times[i];
            const bool level = (i & 1u) == 0u;
            if (kia_v1_is_short(d))
                halves++;
            else if (kia_v1_is_long(d))
                halves = (uint16_t)(halves + 2u);
            else
            {
                stopped_on_invalid_low = !level;
                break;
            }
        }

        /* Cap the hidden-pair guess by the observed long run: the true
         * preamble alone must exceed KIA_V1_MIN_PREAMBLE counted low-longs,
         * so at most (header_count - 71) extra pairs can be hidden data.
         * This rejects phantom prefixes on tail-truncated captures. */
        const int32_t pair_cap = (int32_t)header_count - (KIA_V1_MIN_PREAMBLE + 1u);

        int32_t hidden_pairs = -1;
        for (uint8_t with_implicit = 0; with_implicit <= 1u; with_implicit++)
        {
            if (with_implicit && !stopped_on_invalid_low)
                continue;
            const int32_t missing =
                (int32_t)KIA_V1_DATA_HALVES - (int32_t)halves - with_implicit;
            if (missing >= 0 && (missing & 3) == 0 &&
                missing / 4 <= (int32_t)KIA_V1_HIDDEN_PAIRS_MAX &&
                missing / 4 <= pair_cap)
            {
                hidden_pairs = missing / 4;
                break;
            }
        }
        if (hidden_pairs < 0)
        {
            pos = (uint16_t)(anchor + 1u);
            continue;
        }

        /* Data phase (upstream KiaV1DecoderStepDecodeData): seeded MSB plus
         * the hidden "01" pairs pre-loaded, then Flipper's manchester machine
         * over the remaining pulses. */
        uint64_t data = 1uLL; /* seeded MSB (bit 56 = 1) */
        uint8_t count = 1;
        for (uint8_t j = 0; j < 2u * (uint8_t)hidden_pairs; j++)
        {
            data = (data << 1) | ((j & 1u) ? 1u : 0u);
            count++;
        }

        ManchesterState man = ManchesterStateMid1;
        int32_t commit = -1;
        for (i = (uint16_t)anchor; i < region_end; i++)
        {
            const ManchesterEvent event =
                kia_v1_event(subghz_decenc_ctl.pulse_times[i], (i & 1u) == 0u);
            if (event == ManchesterEventReset)
                break; /* gap: end of the decodable region */
            bool bit = false;
            if (manchester_advance(man, event, &man, &bit))
            {
                data = (data << 1) | (bit ? 1u : 0u);
                count++;
                if (count == KIA_V1_BITS)
                {
                    commit = (int32_t)i;
                    break;
                }
            }
        }
        if (commit < 0)
        {
            pos = (uint16_t)(anchor + 1u);
            continue;
        }

        /* Residue check: at most one half-symbol (the last bit's second
         * half) may remain in the region after the commit. */
        uint8_t residue = 0;
        bool residue_ok = true;
        for (i = (uint16_t)(commit + 1); i < region_end; i++)
        {
            const uint16_t d = subghz_decenc_ctl.pulse_times[i];
            const bool level = (i & 1u) == 0u;
            if (kia_v1_is_short(d))
                residue++;
            else if (kia_v1_is_long(d))
                residue = (uint8_t)(residue + 2u);
            else
            {
                if (!level)
                    residue++;
                break;
            }
            if (residue > 1u)
            {
                residue_ok = false;
                break;
            }
        }
        if (!residue_ok)
        {
            pos = (uint16_t)(anchor + 1u);
            continue;
        }

        /* Commit gate: a frame that needed hidden-pair reconstruction
         * commits only when its CRC-4 checks out (a tail-truncated capture
         * that picks up a phantom prefix fails 15/16 of the time and is
         * dropped); zero-hidden-pair frames keep upstream's display-only
         * CRC policy and always commit. */
        if (hidden_pairs > 0)
        {
            bool crc_valid = false;
            m1_kia_v1_parse_fields(data, NULL, NULL, NULL, NULL, &crc_valid);
            if (!crc_valid)
            {
                pos = (uint16_t)(anchor + 1u);
                continue;
            }
        }

        kia_v1_commit(data, protocol_index);
        return 0;
    }
    return 1;
}
