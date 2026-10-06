/* See COPYING.txt for license details. */

/*
 * wifi_pcapng.h
 *
 * Pure-logic PCAPNG + radiotap encoder (host side, no HAL / RTOS / FatFS deps).
 *
 * This is the file-format half of the MonstaShark-equivalent WiFi capture
 * feature: it turns raw 802.11 frames (plus their per-frame RSSI / channel
 * metadata) into a Wireshark-openable PCAPNG byte stream.  It knows nothing
 * about where the frames come from — the MtkCore native CAPTURE service
 * (m1_esp32_capture.h) feeds it, and the sniffer scene writes the emitted
 * blocks to SD.
 *
 * A PCAPNG capture file is a Section Header Block (SHB) followed by one
 * Interface Description Block (IDB) and then one Enhanced Packet Block (EPB)
 * per captured frame:
 *
 *   [ SHB ][ IDB ][ EPB ][ EPB ] ...
 *
 * The interface link type is LINKTYPE_IEEE802_11_RADIOTAP (127): every EPB
 * packet payload is a small radiotap header (carrying Flags / Channel / dBm
 * antenna signal) immediately followed by the raw 802.11 frame bytes.  This is
 * the same encapsulation Wireshark expects from a monitor-mode capture, so the
 * resulting file opens directly with full RSSI / channel annotation.
 *
 * Every helper writes little-endian, bounds-checks against the supplied
 * capacity, and returns the number of bytes written (0 on bad args / overflow),
 * so the whole module is exercised directly by tests/test_wifi_pcapng.c.
 *
 * References: PCAPNG spec (SHB/IDB/EPB block layout, 32-bit alignment);
 * radiotap.org (header version/pad/len/present + Flags/Channel/dBm fields).
 *
 * M1 Project
 */

#ifndef WIFI_PCAPNG_H_
#define WIFI_PCAPNG_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- PCAPNG constants -------------------------------------------------- */

/** radiotap-encapsulated 802.11 (DLT_IEEE802_11_RADIOTAP). */
#define WIFI_PCAPNG_LINKTYPE_IEEE802_11_RADIOTAP  127u

/** Block types (stored little-endian). */
#define WIFI_PCAPNG_BT_SHB   0x0A0D0D0Au  /**< Section Header Block           */
#define WIFI_PCAPNG_BT_IDB   0x00000001u  /**< Interface Description Block    */
#define WIFI_PCAPNG_BT_EPB   0x00000006u  /**< Enhanced Packet Block          */

/** SHB byte-order magic — resolves to native-LE on reader + writer. */
#define WIFI_PCAPNG_BYTE_ORDER_MAGIC  0x1A2B3C4Du

/** Fixed block sizes (no options). */
#define WIFI_PCAPNG_SHB_LEN      28u  /**< type+len+magic+ver2+seclen8+len    */
#define WIFI_PCAPNG_IDB_LEN      20u  /**< type+len+link2+res2+snap+len       */
#define WIFI_PCAPNG_EPB_OVERHEAD 32u  /**< everything in an EPB except data   */

/* ---- radiotap constants ----------------------------------------------- */

/** present-bitmap: Flags(bit1) | Channel(bit3) | dBm antenna signal(bit5). */
#define WIFI_RADIOTAP_PRESENT  0x0000002Au

/** Fixed radiotap header length produced by wifi_radiotap_build(). */
#define WIFI_RADIOTAP_LEN  15u

/** Channel flags: 2 GHz spectrum. */
#define WIFI_RADIOTAP_CHAN_2GHZ  0x0080u

/* ---- radiotap ---------------------------------------------------------- */

/**
 * Map a 2.4 GHz 802.11 channel number (1..14) to its centre frequency in MHz.
 * Returns 0 for an out-of-range channel.
 */
uint16_t wifi_radiotap_chan_to_freq(uint8_t channel);

/**
 * Build a WIFI_RADIOTAP_LEN-byte radiotap header into @p out.
 *
 * Layout: version(0) pad(0) it_len(15) present(0x2A) Flags(0) pad channel
 * {freq u16, flags u16} dBm-signal(s8).
 *
 * @return WIFI_RADIOTAP_LEN on success, 0 if @p out is NULL or @p cap too small.
 */
size_t wifi_radiotap_build(uint8_t *out, size_t cap,
                           uint8_t channel, int8_t rssi);

/* ---- PCAPNG blocks ----------------------------------------------------- */

/**
 * Build the Section Header Block (28 bytes).
 * @return WIFI_PCAPNG_SHB_LEN on success, 0 on bad args / insufficient cap.
 */
size_t wifi_pcapng_build_shb(uint8_t *out, size_t cap);

/**
 * Build the Interface Description Block (20 bytes) for a radiotap interface.
 * @return WIFI_PCAPNG_IDB_LEN on success, 0 on bad args / insufficient cap.
 */
size_t wifi_pcapng_build_idb(uint8_t *out, size_t cap, uint32_t snaplen);

/**
 * Build an Enhanced Packet Block wrapping @p pkt (already-encapsulated packet
 * bytes, e.g. radiotap header + 802.11 frame).
 *
 * @param out       output buffer
 * @param cap       capacity of @p out
 * @param pkt       packet bytes (radiotap + frame); may be NULL iff caplen == 0
 * @param caplen    number of bytes present in @p pkt (captured length)
 * @param origlen   original on-wire packet length (>= caplen)
 * @param ts_us     timestamp in microseconds (split into the EPB hi/lo words)
 * @return total EPB length (WIFI_PCAPNG_EPB_OVERHEAD + caplen + pad) on success,
 *         0 on bad args / insufficient cap.
 */
size_t wifi_pcapng_build_epb(uint8_t *out, size_t cap,
                             const uint8_t *pkt, uint32_t caplen,
                             uint32_t origlen, uint64_t ts_us);

/**
 * Convenience: assemble a radiotap header + raw 802.11 frame into one EPB.
 *
 * Produces the same bytes as wifi_radiotap_build() followed by the frame, then
 * wrapped with wifi_pcapng_build_epb().  The EPB captured length is
 * WIFI_RADIOTAP_LEN + @p frame_caplen and the original length is
 * WIFI_RADIOTAP_LEN + @p frame_origlen.
 *
 * @param frame_caplen  bytes of @p frame actually present (captured)
 * @param frame_origlen original frame length on the air (>= frame_caplen)
 * @return total EPB length on success, 0 on bad args / insufficient cap.
 */
size_t wifi_pcapng_build_epb_radiotap(uint8_t *out, size_t cap,
                                      const uint8_t *frame,
                                      uint32_t frame_caplen,
                                      uint32_t frame_origlen,
                                      uint8_t channel, int8_t rssi,
                                      uint64_t ts_us);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_PCAPNG_H_ */
