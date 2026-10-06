/* See COPYING.txt for license details. */

/*
 * wifi_pcapng.c
 *
 * Pure-logic PCAPNG + radiotap encoder.  See wifi_pcapng.h for the format
 * overview.  No HAL / RTOS / FatFS dependencies — every function writes into a
 * caller-supplied buffer and is host-tested by tests/test_wifi_pcapng.c.
 *
 * M1 Project
 */

#include "wifi_pcapng.h"
#include <string.h>

/* ---- little-endian store helpers --------------------------------------- */

static inline void le_store16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
}

static inline void le_store32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

/* ---- radiotap ---------------------------------------------------------- */

uint16_t wifi_radiotap_chan_to_freq(uint8_t channel)
{
    if (channel >= 1u && channel <= 13u)
        return (uint16_t)(2407u + (uint16_t)channel * 5u);
    if (channel == 14u)
        return 2484u;
    return 0u;
}

size_t wifi_radiotap_build(uint8_t *out, size_t cap,
                           uint8_t channel, int8_t rssi)
{
    if (!out || cap < WIFI_RADIOTAP_LEN)
        return 0u;

    memset(out, 0, WIFI_RADIOTAP_LEN);
    out[0] = 0u;                                /* it_version                 */
    out[1] = 0u;                                /* it_pad                     */
    le_store16(out + 2, WIFI_RADIOTAP_LEN);     /* it_len                     */
    le_store32(out + 4, WIFI_RADIOTAP_PRESENT); /* it_present                 */
    out[8] = 0u;                                /* Flags                      */
    /* out[9] is alignment padding before the 2-byte-aligned Channel field.  */
    le_store16(out + 10, wifi_radiotap_chan_to_freq(channel)); /* chan freq   */
    le_store16(out + 12, WIFI_RADIOTAP_CHAN_2GHZ);            /* chan flags   */
    out[14] = (uint8_t)rssi;                    /* dBm antenna signal (s8)    */
    return WIFI_RADIOTAP_LEN;
}

/* ---- PCAPNG blocks ----------------------------------------------------- */

size_t wifi_pcapng_build_shb(uint8_t *out, size_t cap)
{
    if (!out || cap < WIFI_PCAPNG_SHB_LEN)
        return 0u;

    le_store32(out + 0,  WIFI_PCAPNG_BT_SHB);
    le_store32(out + 4,  WIFI_PCAPNG_SHB_LEN);
    le_store32(out + 8,  WIFI_PCAPNG_BYTE_ORDER_MAGIC);
    le_store16(out + 12, 1u);          /* major version                      */
    le_store16(out + 14, 0u);          /* minor version                      */
    le_store32(out + 16, 0xFFFFFFFFu); /* section length (-1 = unspecified)   */
    le_store32(out + 20, 0xFFFFFFFFu);
    le_store32(out + 24, WIFI_PCAPNG_SHB_LEN);
    return WIFI_PCAPNG_SHB_LEN;
}

size_t wifi_pcapng_build_idb(uint8_t *out, size_t cap, uint32_t snaplen)
{
    if (!out || cap < WIFI_PCAPNG_IDB_LEN)
        return 0u;

    le_store32(out + 0,  WIFI_PCAPNG_BT_IDB);
    le_store32(out + 4,  WIFI_PCAPNG_IDB_LEN);
    le_store16(out + 8,  WIFI_PCAPNG_LINKTYPE_IEEE802_11_RADIOTAP);
    le_store16(out + 10, 0u);          /* reserved                           */
    le_store32(out + 12, snaplen);
    le_store32(out + 16, WIFI_PCAPNG_IDB_LEN);
    return WIFI_PCAPNG_IDB_LEN;
}

size_t wifi_pcapng_build_epb(uint8_t *out, size_t cap,
                             const uint8_t *pkt, uint32_t caplen,
                             uint32_t origlen, uint64_t ts_us)
{
    if (!out)
        return 0u;
    if (caplen > 0u && !pkt)
        return 0u;

    uint32_t pad   = (4u - (caplen & 3u)) & 3u;
    uint32_t total = WIFI_PCAPNG_EPB_OVERHEAD + caplen + pad;
    if (cap < total)
        return 0u;

    uint32_t ts_hi = (uint32_t)(ts_us >> 32);
    uint32_t ts_lo = (uint32_t)(ts_us & 0xFFFFFFFFu);

    le_store32(out + 0,  WIFI_PCAPNG_BT_EPB);
    le_store32(out + 4,  total);
    le_store32(out + 8,  0u);          /* interface id                       */
    le_store32(out + 12, ts_hi);
    le_store32(out + 16, ts_lo);
    le_store32(out + 20, caplen);
    le_store32(out + 24, origlen);
    if (caplen > 0u)
        memcpy(out + 28, pkt, caplen);
    if (pad > 0u)
        memset(out + 28 + caplen, 0, pad);
    le_store32(out + 28 + caplen + pad, total);
    return total;
}

size_t wifi_pcapng_build_epb_radiotap(uint8_t *out, size_t cap,
                                      const uint8_t *frame,
                                      uint32_t frame_caplen,
                                      uint32_t frame_origlen,
                                      uint8_t channel, int8_t rssi,
                                      uint64_t ts_us)
{
    if (!out)
        return 0u;
    if (frame_caplen > 0u && !frame)
        return 0u;
    if (frame_origlen < frame_caplen)
        frame_origlen = frame_caplen;

    uint32_t caplen  = WIFI_RADIOTAP_LEN + frame_caplen;
    uint32_t origlen = WIFI_RADIOTAP_LEN + frame_origlen;
    uint32_t pad     = (4u - (caplen & 3u)) & 3u;
    uint32_t total   = WIFI_PCAPNG_EPB_OVERHEAD + caplen + pad;
    if (cap < total)
        return 0u;

    uint32_t ts_hi = (uint32_t)(ts_us >> 32);
    uint32_t ts_lo = (uint32_t)(ts_us & 0xFFFFFFFFu);

    le_store32(out + 0,  WIFI_PCAPNG_BT_EPB);
    le_store32(out + 4,  total);
    le_store32(out + 8,  0u);
    le_store32(out + 12, ts_hi);
    le_store32(out + 16, ts_lo);
    le_store32(out + 20, caplen);
    le_store32(out + 24, origlen);
    /* Packet data = radiotap header immediately followed by the frame. */
    (void)wifi_radiotap_build(out + 28, cap - 28u, channel, rssi);
    if (frame_caplen > 0u)
        memcpy(out + 28 + WIFI_RADIOTAP_LEN, frame, frame_caplen);
    if (pad > 0u)
        memset(out + 28 + caplen, 0, pad);
    le_store32(out + 28 + caplen + pad, total);
    return total;
}
