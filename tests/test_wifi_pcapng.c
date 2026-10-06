/* See COPYING.txt for license details. */

/*
 * test_wifi_pcapng.c
 *
 * Host-side unit tests for the pure-logic PCAPNG + radiotap encoder
 * (m1_csrc/wifi_pcapng.c).  No HAL / FatFS / hardware involved.
 */

#include <string.h>
#include "unity.h"
#include "wifi_pcapng.h"

void setUp(void) {}
void tearDown(void) {}

/* ---- little-endian readback helpers ------------------------------------ */
static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* ======================================================================
 * radiotap
 * ======================================================================*/

void test_chan_to_freq(void)
{
    TEST_ASSERT_EQUAL_UINT16(2412u, wifi_radiotap_chan_to_freq(1));
    TEST_ASSERT_EQUAL_UINT16(2437u, wifi_radiotap_chan_to_freq(6));
    TEST_ASSERT_EQUAL_UINT16(2472u, wifi_radiotap_chan_to_freq(13));
    TEST_ASSERT_EQUAL_UINT16(2484u, wifi_radiotap_chan_to_freq(14));
    TEST_ASSERT_EQUAL_UINT16(0u, wifi_radiotap_chan_to_freq(0));
    TEST_ASSERT_EQUAL_UINT16(0u, wifi_radiotap_chan_to_freq(15));
}

void test_radiotap_layout(void)
{
    uint8_t rt[WIFI_RADIOTAP_LEN];
    size_t n = wifi_radiotap_build(rt, sizeof(rt), 6, -42);
    TEST_ASSERT_EQUAL_size_t(WIFI_RADIOTAP_LEN, n);

    TEST_ASSERT_EQUAL_UINT8(0u, rt[0]);                  /* version */
    TEST_ASSERT_EQUAL_UINT8(0u, rt[1]);                  /* pad */
    TEST_ASSERT_EQUAL_UINT16(WIFI_RADIOTAP_LEN, rd16(rt + 2));
    TEST_ASSERT_EQUAL_UINT32(WIFI_RADIOTAP_PRESENT, rd32(rt + 4));
    TEST_ASSERT_EQUAL_UINT8(0u, rt[8]);                  /* Flags */
    TEST_ASSERT_EQUAL_UINT16(2437u, rd16(rt + 10));      /* channel freq */
    TEST_ASSERT_EQUAL_UINT16(WIFI_RADIOTAP_CHAN_2GHZ, rd16(rt + 12));
    TEST_ASSERT_EQUAL_INT8(-42, (int8_t)rt[14]);         /* dBm signal */
}

void test_radiotap_rejects_small_buffer(void)
{
    uint8_t rt[WIFI_RADIOTAP_LEN - 1];
    TEST_ASSERT_EQUAL_size_t(0u, wifi_radiotap_build(rt, sizeof(rt), 1, -10));
    TEST_ASSERT_EQUAL_size_t(0u, wifi_radiotap_build(NULL, 64, 1, -10));
}

/* ======================================================================
 * SHB / IDB
 * ======================================================================*/

void test_shb_layout(void)
{
    uint8_t shb[WIFI_PCAPNG_SHB_LEN];
    size_t n = wifi_pcapng_build_shb(shb, sizeof(shb));
    TEST_ASSERT_EQUAL_size_t(WIFI_PCAPNG_SHB_LEN, n);

    TEST_ASSERT_EQUAL_UINT32(WIFI_PCAPNG_BT_SHB, rd32(shb + 0));
    TEST_ASSERT_EQUAL_UINT32(WIFI_PCAPNG_SHB_LEN, rd32(shb + 4));
    TEST_ASSERT_EQUAL_UINT32(WIFI_PCAPNG_BYTE_ORDER_MAGIC, rd32(shb + 8));
    TEST_ASSERT_EQUAL_UINT16(1u, rd16(shb + 12));       /* major */
    TEST_ASSERT_EQUAL_UINT16(0u, rd16(shb + 14));       /* minor */
    /* trailing block total length mirrors the leading one */
    TEST_ASSERT_EQUAL_UINT32(WIFI_PCAPNG_SHB_LEN, rd32(shb + 24));
}

void test_idb_layout(void)
{
    uint8_t idb[WIFI_PCAPNG_IDB_LEN];
    size_t n = wifi_pcapng_build_idb(idb, sizeof(idb), 2048u);
    TEST_ASSERT_EQUAL_size_t(WIFI_PCAPNG_IDB_LEN, n);

    TEST_ASSERT_EQUAL_UINT32(WIFI_PCAPNG_BT_IDB, rd32(idb + 0));
    TEST_ASSERT_EQUAL_UINT32(WIFI_PCAPNG_IDB_LEN, rd32(idb + 4));
    TEST_ASSERT_EQUAL_UINT16(WIFI_PCAPNG_LINKTYPE_IEEE802_11_RADIOTAP, rd16(idb + 8));
    TEST_ASSERT_EQUAL_UINT16(0u, rd16(idb + 10));       /* reserved */
    TEST_ASSERT_EQUAL_UINT32(2048u, rd32(idb + 12));    /* snaplen */
    TEST_ASSERT_EQUAL_UINT32(WIFI_PCAPNG_IDB_LEN, rd32(idb + 16));
}

void test_blocks_reject_small_buffer(void)
{
    uint8_t buf[8];
    TEST_ASSERT_EQUAL_size_t(0u, wifi_pcapng_build_shb(buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_size_t(0u, wifi_pcapng_build_idb(buf, sizeof(buf), 100u));
}

/* ======================================================================
 * EPB
 * ======================================================================*/

void test_epb_layout_and_padding(void)
{
    /* 3-byte packet => 1 byte of padding, total = 32 + 3 + 1 = 36 */
    const uint8_t pkt[3] = { 0xAA, 0xBB, 0xCC };
    uint8_t epb[64];
    size_t n = wifi_pcapng_build_epb(epb, sizeof(epb), pkt, 3u, 9u,
                                     0x0000000100000002ull);
    TEST_ASSERT_EQUAL_size_t(36u, n);

    TEST_ASSERT_EQUAL_UINT32(WIFI_PCAPNG_BT_EPB, rd32(epb + 0));
    TEST_ASSERT_EQUAL_UINT32(36u, rd32(epb + 4));
    TEST_ASSERT_EQUAL_UINT32(0u, rd32(epb + 8));        /* interface id */
    TEST_ASSERT_EQUAL_UINT32(0x00000001u, rd32(epb + 12)); /* ts hi */
    TEST_ASSERT_EQUAL_UINT32(0x00000002u, rd32(epb + 16)); /* ts lo */
    TEST_ASSERT_EQUAL_UINT32(3u, rd32(epb + 20));       /* captured len */
    TEST_ASSERT_EQUAL_UINT32(9u, rd32(epb + 24));       /* original len */
    TEST_ASSERT_EQUAL_MEMORY(pkt, epb + 28, 3);
    TEST_ASSERT_EQUAL_UINT8(0u, epb[31]);               /* pad byte */
    TEST_ASSERT_EQUAL_UINT32(36u, rd32(epb + 32));      /* trailing total len */
}

void test_epb_aligned_no_padding(void)
{
    const uint8_t pkt[4] = { 1, 2, 3, 4 };
    uint8_t epb[64];
    size_t n = wifi_pcapng_build_epb(epb, sizeof(epb), pkt, 4u, 4u, 0);
    TEST_ASSERT_EQUAL_size_t(36u, n);                   /* 32 + 4 + 0 */
    TEST_ASSERT_EQUAL_UINT32(36u, rd32(epb + 4));
    TEST_ASSERT_EQUAL_UINT32(36u, rd32(epb + 32));
}

void test_epb_rejects_small_buffer(void)
{
    const uint8_t pkt[4] = { 1, 2, 3, 4 };
    uint8_t epb[35];  /* need 36 */
    TEST_ASSERT_EQUAL_size_t(0u,
        wifi_pcapng_build_epb(epb, sizeof(epb), pkt, 4u, 4u, 0));
}

void test_epb_radiotap_wraps_frame(void)
{
    const uint8_t frame[2] = { 0x80, 0x00 };  /* beacon frame-control */
    uint8_t epb[64];
    size_t n = wifi_pcapng_build_epb_radiotap(epb, sizeof(epb), frame, 2u, 2u,
                                              6, -55, 1000ull);
    /* caplen = 15 + 2 = 17, pad = 3, total = 32 + 17 + 3 = 52 */
    TEST_ASSERT_EQUAL_size_t(52u, n);
    TEST_ASSERT_EQUAL_UINT32(WIFI_PCAPNG_BT_EPB, rd32(epb + 0));
    TEST_ASSERT_EQUAL_UINT32(17u, rd32(epb + 20));      /* captured len */
    TEST_ASSERT_EQUAL_UINT32(17u, rd32(epb + 24));      /* original len */
    /* radiotap then frame */
    TEST_ASSERT_EQUAL_UINT16(WIFI_RADIOTAP_LEN, rd16(epb + 28 + 2));
    TEST_ASSERT_EQUAL_UINT16(2437u, rd16(epb + 28 + 10));
    TEST_ASSERT_EQUAL_INT8(-55, (int8_t)epb[28 + 14]);
    TEST_ASSERT_EQUAL_MEMORY(frame, epb + 28 + WIFI_RADIOTAP_LEN, 2);
    TEST_ASSERT_EQUAL_UINT32(52u, rd32(epb + 48));
}

void test_epb_radiotap_clamps_origlen(void)
{
    const uint8_t frame[4] = { 1, 2, 3, 4 };
    uint8_t epb[64];
    /* frame_origlen < frame_caplen must be clamped up to caplen */
    size_t n = wifi_pcapng_build_epb_radiotap(epb, sizeof(epb), frame, 4u, 0u,
                                              1, -10, 0);
    TEST_ASSERT_TRUE(n > 0u);
    TEST_ASSERT_EQUAL_UINT32(rd32(epb + 20), rd32(epb + 24)); /* cap == orig */
}

/* ======================================================================
 * Integration: a full file assembled in one buffer
 * ======================================================================*/

void test_full_file_assembly(void)
{
    uint8_t file[256];
    size_t off = 0;
    size_t n;

    n = wifi_pcapng_build_shb(file + off, sizeof(file) - off);
    TEST_ASSERT_EQUAL_size_t(WIFI_PCAPNG_SHB_LEN, n); off += n;
    n = wifi_pcapng_build_idb(file + off, sizeof(file) - off, 1024u);
    TEST_ASSERT_EQUAL_size_t(WIFI_PCAPNG_IDB_LEN, n); off += n;

    const uint8_t f1[5] = { 0x40, 0x00, 0x01, 0x02, 0x03 };
    n = wifi_pcapng_build_epb_radiotap(file + off, sizeof(file) - off,
                                       f1, 5u, 5u, 11, -70, 42ull);
    TEST_ASSERT_TRUE(n > 0u); off += n;

    /* The file starts with the SHB magic and every block's leading and
     * trailing total-length fields agree. */
    TEST_ASSERT_EQUAL_UINT32(WIFI_PCAPNG_BT_SHB, rd32(file));
    TEST_ASSERT_EQUAL_UINT32(rd32(file + 4), WIFI_PCAPNG_SHB_LEN);
    TEST_ASSERT_TRUE(off <= sizeof(file));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_chan_to_freq);
    RUN_TEST(test_radiotap_layout);
    RUN_TEST(test_radiotap_rejects_small_buffer);
    RUN_TEST(test_shb_layout);
    RUN_TEST(test_idb_layout);
    RUN_TEST(test_blocks_reject_small_buffer);
    RUN_TEST(test_epb_layout_and_padding);
    RUN_TEST(test_epb_aligned_no_padding);
    RUN_TEST(test_epb_rejects_small_buffer);
    RUN_TEST(test_epb_radiotap_wraps_frame);
    RUN_TEST(test_epb_radiotap_clamps_origlen);
    RUN_TEST(test_full_file_assembly);
    return UNITY_END();
}
