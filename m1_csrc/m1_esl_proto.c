/* See COPYING.txt for license details. */

/**
 * @file   m1_esl_proto.c
 * @brief  ESL protocol frame builder.
 *
 * Port of the protocol layer from TagTinker (github.com/i12bp8/TagTinker),
 * adapted for the M1 firmware (no Flipper/FURI runtime dependencies).
 *
 * Based on reverse-engineering research by furrtek (github.com/furrtek/PrecIR).
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "m1_esl_proto.h"
#include <string.h>

/* --------------------------------------------------------------------------- */
/* Internal helpers                                                            */
/* --------------------------------------------------------------------------- */

/** CRC-16 variant used by the Pricer ESL wire format. */
static uint16_t esl_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0x8408U;
    for(size_t i = 0U; i < len; i++) {
        crc ^= (uint16_t)data[i];
        for(int b = 0; b < 8; b++) {
            crc = (crc & 1U) ? ((crc >> 1U) ^ 0x8408U) : (crc >> 1U);
        }
    }
    return crc;
}

/** Append 2-byte little-endian CRC to buf[len] and return new length. */
static size_t terminate(uint8_t *buf, size_t len)
{
    uint16_t crc    = esl_crc16(buf, len);
    buf[len]        = (uint8_t)(crc & 0xFFU);
    buf[len + 1U]   = (uint8_t)((crc >> 8U) & 0xFFU);
    return len + 2U;
}

/**
 * Write the 6-byte addressed frame header:
 *   [proto][plid3][plid2][plid1][plid0][cmd]
 * Returns the number of bytes written (always 6).
 */
static size_t raw_frame(uint8_t *buf, uint8_t proto,
                        const uint8_t plid[4], uint8_t cmd)
{
    buf[0U] = proto;
    buf[1U] = plid[3U];
    buf[2U] = plid[2U];
    buf[3U] = plid[1U];
    buf[4U] = plid[0U];
    buf[5U] = cmd;
    return 6U;
}

/* --------------------------------------------------------------------------- */
/* Public API                                                                  */
/* --------------------------------------------------------------------------- */

bool m1_esl_barcode_to_plid(const char *barcode, uint8_t plid[4])
{
    if(!barcode || !plid) return false;

    /* Barcode must be exactly 17 characters plus a terminating NUL. */
    size_t bc_len = strnlen(barcode, 18U);
    if(bc_len != 17U) return false;

    /* Positions 2–11 must be decimal digits (the two 5-digit groups that
     * encode the PLID).  Positions 0–1 ("04" manufacturer prefix) and
     * positions 12–16 (type code + check digit) are not decoded into the
     * PLID, so they are not digit-validated here. */
    for(int i = 2; i < 12; i++) {
        if(barcode[i] < '0' || barcode[i] > '9') return false;
    }

    /* Decode the two 5-digit groups that encode the physical layer ID. */
    uint32_t lo = 0U;
    uint32_t hi = 0U;
    for(int i = 2; i < 7;  i++) lo = lo * 10U + (uint32_t)(barcode[i] - '0');
    for(int i = 7; i < 12; i++) hi = hi * 10U + (uint32_t)(barcode[i] - '0');

    uint32_t id = lo + (hi << 16U);
    plid[0U] = (uint8_t)((id >>  8U) & 0xFFU);
    plid[1U] = (uint8_t)( id         & 0xFFU);
    plid[2U] = (uint8_t)((id >> 24U) & 0xFFU);
    plid[3U] = (uint8_t)((id >> 16U) & 0xFFU);
    return true;
}

size_t m1_esl_build_broadcast_page_frame(
    uint8_t *buf, uint8_t page, bool forever, uint16_t duration)
{
    if(!buf) return 0U;

    const uint8_t plid[4] = {0U, 0U, 0U, 0U};
    size_t p = raw_frame(buf, M1_ESL_PROTO_DM, plid, 0x06U);

    buf[p++] = (uint8_t)(((page & 7U) << 3U) | 0x01U | (forever ? 0x80U : 0x00U));
    buf[p++] = 0x00U;
    buf[p++] = 0x00U;
    buf[p++] = forever ? 0x00U : (uint8_t)((duration >> 8U) & 0xFFU);
    buf[p++] = forever ? 0x00U : (uint8_t)( duration         & 0xFFU);
    return terminate(buf, p);
}

size_t m1_esl_build_broadcast_debug_frame(uint8_t *buf)
{
    if(!buf) return 0U;

    const uint8_t plid[4] = {0U, 0U, 0U, 0U};
    size_t p = raw_frame(buf, M1_ESL_PROTO_DM, plid, 0x06U);

    buf[p++] = 0xF1U;
    buf[p++] = 0x00U;
    buf[p++] = 0x00U;
    buf[p++] = 0x00U;
    buf[p++] = 0x0AU;
    return terminate(buf, p);
}

size_t m1_esl_build_ping_frame(uint8_t *buf, const uint8_t plid[4])
{
    if(!buf || !plid) return 0U;

    size_t p = raw_frame(buf, M1_ESL_PROTO_DM, plid, 0x17U);
    buf[p++] = 0x01U;
    buf[p++] = 0x00U;
    buf[p++] = 0x00U;
    buf[p++] = 0x00U;
    for(int i = 0; i < 22; i++) buf[p++] = 0x01U;
    return terminate(buf, p);
}
