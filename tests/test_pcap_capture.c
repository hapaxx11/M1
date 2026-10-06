/* See COPYING.txt for license details. */

/*
 * test_pcap_capture.c
 *
 * Host-side unit tests for the SD PCAPNG capture session
 * (m1_csrc/m1_pcap_capture.c).  FatFS calls go through the stdio-backed stub
 * (tests/stubs/ff.h), so the whole on-SD byte layout is validated by reading
 * the produced file back and checking the PCAPNG block structure.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "unity.h"
#include "m1_pcap_capture.h"
#include "wifi_pcapng.h"

void setUp(void) {}
void tearDown(void) {}

static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* Read a whole file into @p buf; returns byte count. */
static size_t slurp(const char *path, uint8_t *buf, size_t cap)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    size_t n = fread(buf, 1, cap, f);
    fclose(f);
    return n;
}

/* ======================================================================
 * Filename formatter
 * ======================================================================*/

void test_format_name(void)
{
    char buf[M1_PCAP_PATH_MAX];
    size_t n = m1_pcap_format_name(buf, sizeof(buf), 7u);
    TEST_ASSERT_EQUAL_size_t(23u, n);
    TEST_ASSERT_EQUAL_STRING("capture/sniff007.pcapng", buf);

    TEST_ASSERT_EQUAL_size_t(23u, m1_pcap_format_name(buf, sizeof(buf), 123u));
    TEST_ASSERT_EQUAL_STRING("capture/sniff123.pcapng", buf);
}

void test_format_name_rejects_bad_args(void)
{
    char buf[M1_PCAP_PATH_MAX];
    TEST_ASSERT_EQUAL_size_t(0u, m1_pcap_format_name(NULL, sizeof(buf), 0u));
    TEST_ASSERT_EQUAL_size_t(0u,
        m1_pcap_format_name(buf, sizeof(buf), M1_PCAP_MAX_INDEX + 1u));
    char tiny[8];
    TEST_ASSERT_EQUAL_size_t(0u, m1_pcap_format_name(tiny, sizeof(tiny), 1u));
}

/* ======================================================================
 * Session round-trip (open_path -> write -> close -> read back)
 * ======================================================================*/

void test_session_header_and_frames(void)
{
    const char *path = "/tmp/m1_pcap_test_a.pcapng";
    m1_pcap_session_t s;
    TEST_ASSERT_TRUE(m1_pcap_session_open_path(&s, path, 0u)); /* 0 => default */
    TEST_ASSERT_TRUE(m1_pcap_session_active(&s));
    TEST_ASSERT_EQUAL_STRING(path, s.path);

    const uint8_t f1[4] = { 0x80, 0x00, 0xAA, 0xBB };
    const uint8_t f2[6] = { 0x40, 0x00, 0x01, 0x02, 0x03, 0x04 };
    TEST_ASSERT_TRUE(m1_pcap_session_write(&s, f1, 4u, 6u, -50, 1000u));
    TEST_ASSERT_TRUE(m1_pcap_session_write(&s, f2, 6u, 11u, -72, 2000u));
    TEST_ASSERT_EQUAL_UINT32(2u, s.packets);
    m1_pcap_session_close(&s);
    TEST_ASSERT_FALSE(m1_pcap_session_active(&s));

    uint8_t buf[512];
    size_t n = slurp(path, buf, sizeof(buf));
    /* SHB(28) + IDB(20) + EPB(32+15+4+pad1=52... f1 cap=19 -> pad1 ->52)
     * + EPB(32+15+6=53 -> pad3 -> 56) */
    TEST_ASSERT_TRUE(n >= WIFI_PCAPNG_SHB_LEN + WIFI_PCAPNG_IDB_LEN);

    /* SHB */
    TEST_ASSERT_EQUAL_UINT32(WIFI_PCAPNG_BT_SHB, rd32(buf + 0));
    TEST_ASSERT_EQUAL_UINT32(WIFI_PCAPNG_BYTE_ORDER_MAGIC, rd32(buf + 8));
    /* IDB at offset 28 */
    size_t off = WIFI_PCAPNG_SHB_LEN;
    TEST_ASSERT_EQUAL_UINT32(WIFI_PCAPNG_BT_IDB, rd32(buf + off));
    TEST_ASSERT_EQUAL_UINT16(WIFI_PCAPNG_LINKTYPE_IEEE802_11_RADIOTAP,
                             rd16(buf + off + 8));
    /* Interface snaplen covers radiotap header + captured frame bytes. */
    TEST_ASSERT_EQUAL_UINT32((uint32_t)WIFI_RADIOTAP_LEN + M1_PCAP_SNAPLEN,
                             rd32(buf + off + 12));
    off += WIFI_PCAPNG_IDB_LEN;

    /* First EPB */
    TEST_ASSERT_EQUAL_UINT32(WIFI_PCAPNG_BT_EPB, rd32(buf + off));
    uint32_t epb1_total = rd32(buf + off + 4);
    TEST_ASSERT_EQUAL_UINT32(WIFI_RADIOTAP_LEN + 4u, rd32(buf + off + 20)); /* caplen */
    TEST_ASSERT_EQUAL_UINT32(WIFI_RADIOTAP_LEN + 4u, rd32(buf + off + 24)); /* origlen */
    /* radiotap channel freq for channel 6 */
    TEST_ASSERT_EQUAL_UINT16(2437u, rd16(buf + off + 28 + 10));
    TEST_ASSERT_EQUAL_INT8(-50, (int8_t)buf[off + 28 + 14]);
    TEST_ASSERT_EQUAL_MEMORY(f1, buf + off + 28 + WIFI_RADIOTAP_LEN, 4);
    off += epb1_total;

    /* Second EPB */
    TEST_ASSERT_EQUAL_UINT32(WIFI_PCAPNG_BT_EPB, rd32(buf + off));
    TEST_ASSERT_EQUAL_UINT32(WIFI_RADIOTAP_LEN + 6u, rd32(buf + off + 20));
    TEST_ASSERT_EQUAL_UINT16(2462u, rd16(buf + off + 28 + 10)); /* channel 11 */
    TEST_ASSERT_EQUAL_MEMORY(f2, buf + off + 28 + WIFI_RADIOTAP_LEN, 6);
    off += rd32(buf + off + 4);

    TEST_ASSERT_EQUAL_size_t(n, off);  /* file is exactly these blocks */
    remove(path);
}

void test_session_truncates_to_snaplen(void)
{
    const char *path = "/tmp/m1_pcap_test_b.pcapng";
    m1_pcap_session_t s;
    TEST_ASSERT_TRUE(m1_pcap_session_open_path(&s, path, 32u)); /* snaplen 32 */

    uint8_t big[100];
    for (int i = 0; i < 100; i++) big[i] = (uint8_t)i;
    TEST_ASSERT_TRUE(m1_pcap_session_write(&s, big, 100u, 1u, -10, 0u));
    m1_pcap_session_close(&s);

    uint8_t buf[256];
    size_t n = slurp(path, buf, sizeof(buf));
    TEST_ASSERT_TRUE(n > 0u);
    size_t off = WIFI_PCAPNG_SHB_LEN + WIFI_PCAPNG_IDB_LEN;
    /* IDB snaplen reflects the session snaplen */
    uint32_t idb_snap = rd32(buf + WIFI_PCAPNG_SHB_LEN + 12);
    TEST_ASSERT_EQUAL_UINT32(WIFI_RADIOTAP_LEN + 32u, idb_snap);
    TEST_ASSERT_TRUE(rd32(buf + off + 20) <= idb_snap);   /* EPB caplen fits */
    /* captured = radiotap + 32 (truncated); original = radiotap + 100 */
    TEST_ASSERT_EQUAL_UINT32(WIFI_RADIOTAP_LEN + 32u, rd32(buf + off + 20));
    TEST_ASSERT_EQUAL_UINT32(WIFI_RADIOTAP_LEN + 100u, rd32(buf + off + 24));
    /* only the first 32 frame bytes are stored */
    TEST_ASSERT_EQUAL_MEMORY(big, buf + off + 28 + WIFI_RADIOTAP_LEN, 32);
    remove(path);
}

void test_write_after_close_fails(void)
{
    const char *path = "/tmp/m1_pcap_test_c.pcapng";
    m1_pcap_session_t s;
    TEST_ASSERT_TRUE(m1_pcap_session_open_path(&s, path, 0u));
    m1_pcap_session_close(&s);

    const uint8_t f[2] = { 1, 2 };
    TEST_ASSERT_FALSE(m1_pcap_session_write(&s, f, 2u, 1u, 0, 0u));
    remove(path);
}

void test_open_path_rejects_null(void)
{
    m1_pcap_session_t s;
    TEST_ASSERT_FALSE(m1_pcap_session_open_path(NULL, "/tmp/x", 0u));
    TEST_ASSERT_FALSE(m1_pcap_session_open_path(&s, NULL, 0u));
    TEST_ASSERT_FALSE(m1_pcap_session_active(NULL));
}

void test_session_open_auto_picks_index_zero(void)
{
    /* The ff stub's f_stat always returns FR_NO_FILE, so the auto picker lands
     * on index 0 and f_open creates "capture/sniff000.pcapng" relative to CWD.
     * Run from /tmp with the capture dir present. */
    TEST_ASSERT_EQUAL_INT(0, chdir("/tmp"));
    (void)system("mkdir -p /tmp/capture");

    m1_pcap_session_t s;
    TEST_ASSERT_TRUE(m1_pcap_session_open(&s));
    TEST_ASSERT_EQUAL_STRING("capture/sniff000.pcapng", s.path);
    const uint8_t f[3] = { 0x48, 0x00, 0x11 };
    TEST_ASSERT_TRUE(m1_pcap_session_write(&s, f, 3u, 6u, -60, 5u));
    m1_pcap_session_close(&s);

    uint8_t buf[128];
    size_t got = slurp("/tmp/capture/sniff000.pcapng", buf, sizeof(buf));
    TEST_ASSERT_TRUE(got > WIFI_PCAPNG_SHB_LEN);
    TEST_ASSERT_EQUAL_UINT32(WIFI_PCAPNG_BT_SHB, rd32(buf));
    remove("/tmp/capture/sniff000.pcapng");
}

void test_session_open_aborts_on_stat_error(void)
{
    /* Only FR_NO_FILE means "slot free"; any other error must not fall through
     * to FA_CREATE_ALWAYS (which could truncate an existing capture). */
    TEST_ASSERT_EQUAL_INT(0, chdir("/tmp"));
    (void)system("mkdir -p /tmp/capture; rm -f /tmp/capture/sniff000.pcapng");
    ff_stub_stat_result = FR_DISK_ERR;
    m1_pcap_session_t s;
    TEST_ASSERT_FALSE(m1_pcap_session_open(&s));
    ff_stub_stat_result = FR_NO_FILE;
    TEST_ASSERT_EQUAL_INT(-1, access("/tmp/capture/sniff000.pcapng", F_OK));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_format_name);
    RUN_TEST(test_format_name_rejects_bad_args);
    RUN_TEST(test_session_header_and_frames);
    RUN_TEST(test_session_truncates_to_snaplen);
    RUN_TEST(test_write_after_close_fails);
    RUN_TEST(test_open_path_rejects_null);
    RUN_TEST(test_session_open_auto_picks_index_zero);
    RUN_TEST(test_session_open_aborts_on_stat_error);
    return UNITY_END();
}
