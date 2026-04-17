/* See COPYING.txt for license details. */

/*
 * m1_hormann_bisecur_decode.c
 *
 * M1 sub-ghz Hörmann BiSecur 176-bit decode-only decoder.
 *
 * Hörmann BiSecur is an AES-128 encrypted rolling code protocol used by
 * Hörmann garage door operators and gate systems operating at 868 MHz.
 * The protocol uses Manchester-encoded OOK modulation with a preamble
 * sequence of alternating short pulses.
 *
 * OTA format (Momentum reference: lib/subghz/protocols/hormann_bisecur.c):
 *   Encoding: Manchester (OOK biphase, IEEE 802.3 convention)
 *   Data bits: 176 (22 bytes)
 *   te_short = 208 µs, te_long = 416 µs, te_delta = 104 µs
 *   Preamble: alternating short pulses (sync sequence)
 *
 * Packet layout (post-Manchester-decode, 22 bytes):
 *   Byte 0:     message type (identifies button / command)
 *   Bytes 1-4:  device serial number (4 bytes)  → n32_serialnumber
 *   Bytes 5-8:  rolling counter (4 bytes)        → n32_rollingcode
 *   Bytes 9-21: AES-encrypted payload (not decoded here)
 *   Final byte: CRC-8 checksum
 *
 * Implementation note:
 *   The M1 generic Manchester decoder (subghz_decode_generic_manchester)
 *   handles the bit accumulation.  BiSecur has a more complex preamble
 *   than standard Manchester protocols, so this function delegates to the
 *   generic Manchester decoder and then extracts the relevant fields from
 *   the decoded 64-bit value (upper 64 bits of 176-bit frame).
 *
 * NOTE: AES decryption requires the BiSecur manufacturer key.
 *       This decoder is detect-only; serial/counter values are raw.
 */

#include <string.h>
#include <stdlib.h>
#include "stm32h5xx_hal.h"
#include "bit_util.h"
#include "m1_sub_ghz_decenc.h"
#include "subghz_protocol_registry.h"
#include "m1_log_debug.h"

#define M1_LOGDB_TAG "SUBGHZ_BISECUR"

/*
 * Hörmann BiSecur Manchester decoder.
 *
 * The protocol uses a complex preamble: alternating short pulses, then a
 * very long high pulse, then alternating long pulses, then data.
 * For M1's pulse_times[] capture model, the preamble is typically consumed
 * as part of the gap detection, and data starts near the beginning of the
 * captured pulse array.
 *
 * This implementation reuses the generic Manchester decoder and extracts
 * device type, serial, and counter from the decoded bit stream.
 */
uint8_t subghz_decode_hormann_bisecur(uint16_t p, uint16_t pulsecount)
{
    const SubGhzProtocolDef *proto = &subghz_protocol_registry[p];
    const uint16_t te_short = proto->timing.te_short;  /* 208 µs */
    const uint16_t te_long  = proto->timing.te_long;   /* 416 µs */
    const uint16_t te_delta = proto->timing.te_delta;  /* 104 µs */
    const uint16_t min_bits = proto->timing.min_count_bit_for_found;  /* 176 */

    SubGhzBlockDecoder decoder = {0};
    ManchesterState manchester_state = ManchesterStateMid1;

    /* Skip preamble alternating pulses — BiSecur preamble consists of
     * many alternating short pulses before the data region.
     * We skip pulses until we see a long pulse that signals end of preamble. */
    uint16_t i = 0;

    /* Scan past the preamble: look for first long pulse indicating data start.
     * In M1's capture model, the preamble pulses are typically short (~208 µs)
     * and the data region begins after the long sync pulse. */
    for (i = 0; i < pulsecount; i++)
    {
        if (DURATION_DIFF(subghz_decenc_ctl.pulse_times[i], te_long) < te_delta)
        {
            i++;  /* skip the long sync pulse itself, data starts after */
            break;
        }
        if (DURATION_DIFF(subghz_decenc_ctl.pulse_times[i], te_short) >= te_delta * 2)
        {
            return 1;  /* not a BiSecur packet */
        }
    }

    if (i >= pulsecount) return 1;

    /* Manchester decode the data region */
    for (; i < pulsecount && decoder.decode_count_bit < min_bits; i++)
    {
        uint16_t dur = subghz_decenc_ctl.pulse_times[i];
        bool is_high = ((i & 1) == 0);

        ManchesterEvent event;
        if (DURATION_DIFF(dur, te_short) < te_delta)
        {
            event = is_high ? ManchesterEventShortHigh : ManchesterEventShortLow;
        }
        else if (DURATION_DIFF(dur, te_long) < te_delta)
        {
            event = is_high ? ManchesterEventLongHigh : ManchesterEventLongLow;
        }
        else
        {
            manchester_advance(manchester_state, ManchesterEventReset,
                               &manchester_state, NULL);
            continue;
        }

        bool bit_value;
        if (manchester_advance(manchester_state, event, &manchester_state, &bit_value))
        {
            subghz_protocol_blocks_add_bit(&decoder, bit_value ? 1 : 0);
        }
    }

    if (decoder.decode_count_bit >= min_bits)
    {
        uint64_t raw = decoder.decode_data;

        subghz_decenc_ctl.n64_decodedvalue = raw;
        subghz_decenc_ctl.ndecodedbitlength = decoder.decode_count_bit;
        subghz_decenc_ctl.ndecodeddelay = 0;
        subghz_decenc_ctl.ndecodedprotocol = p;

        /* Extract fields from upper 64 bits of the 176-bit Manchester frame.
         * Layout (byte positions in full 22-byte frame, MSB first):
         *   Byte 0:    message type
         *   Bytes 1-4: device serial (32 bits)
         *   Bytes 5-8: rolling counter (32 bits)
         *   Bytes 9+:  AES payload (not captured in 64-bit raw)
         *
         * From the 64-bit captured value (bits 63:0 = frame bytes 0..7):
         *   serial  = bits [55:24]  (bytes 1-4)
         *   counter = bits [23:0] + byte 8 partial
         */
        subghz_decenc_ctl.n32_serialnumber = (uint32_t)((raw >> 24) & 0xFFFFFFFF);
        subghz_decenc_ctl.n32_rollingcode  = (uint32_t)(raw & 0xFFFFFF);
        subghz_decenc_ctl.n8_buttonid      = (uint8_t)((raw >> 56) & 0xFF); /* message type */

        M1_LOG_I(M1_LOGDB_TAG, "BiSecur serial=0x%08lX counter=0x%06lX type=0x%02X\r\n",
                 (unsigned long)subghz_decenc_ctl.n32_serialnumber,
                 (unsigned long)subghz_decenc_ctl.n32_rollingcode,
                 (unsigned)subghz_decenc_ctl.n8_buttonid);

        return 0;
    }

    return 1;
}
