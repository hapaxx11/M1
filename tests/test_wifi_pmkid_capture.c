#include "unity.h"
#include "wifi_pmkid_capture.h"
#include <string.h>

static uint8_t pcap[24 + 16 + 153];
static size_t pcap_len;

static void put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

void setUp(void)
{
    memset(pcap, 0, sizeof(pcap));
    put32(pcap, 0xA1B2C3D4);
    pcap[4] = 2;
    pcap[6] = 4;
    put32(pcap + 20, 105);
    uint8_t *rec = pcap + 24;
    put32(rec + 8, 153);
    put32(rec + 12, 153);
    uint8_t *f = rec + 16;
    f[0] = 0x08;
    const uint8_t bssid[6] = { 0x10, 0x20, 0x30, 0x40, 0x50, 0x60 };
    memcpy(f + 10, bssid, 6);
    uint8_t *llc = f + 24;
    const uint8_t snap[] = { 0xAA, 0xAA, 0x03, 0, 0, 0, 0x88, 0x8E };
    memcpy(llc, snap, 8);
    uint8_t *eapol = llc + 8;
    eapol[1] = 3;
    eapol[6] = 0x80;
    eapol[98] = 22;
    uint8_t *kd = eapol + 99;
    kd[0] = 0xDD;
    kd[1] = 20;
    kd[2] = 0x00;
    kd[3] = 0x0F;
    kd[4] = 0xAC;
    kd[5] = 0x04;
    for (uint8_t i = 0; i < 16; i++) kd[6 + i] = (uint8_t)(0xA0 + i);
    pcap_len = sizeof(pcap);
}

void tearDown(void) { }

static void test_extracts_standard_pmkid(void)
{
    uint8_t bssid[6], pmkid[16];
    TEST_ASSERT_TRUE(wifi_pmkid_pcap_extract(pcap, pcap_len, bssid, pmkid));
    const uint8_t expected_bssid[] = { 0x10, 0x20, 0x30, 0x40, 0x50, 0x60 };
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected_bssid, bssid, 6);
    for (uint8_t i = 0; i < 16; i++) TEST_ASSERT_EQUAL_UINT8(0xA0 + i, pmkid[i]);
}

static void test_rejects_truncated_kde(void)
{
    uint8_t bssid[6], pmkid[16];
    TEST_ASSERT_FALSE(wifi_pmkid_pcap_extract(pcap, pcap_len - 4, bssid, pmkid));
}

static void test_rejects_non_pmkid_vendor_element(void)
{
    uint8_t bssid[6], pmkid[16];
    pcap[24 + 16 + 24 + 8 + 99 + 5] = 0x01;
    TEST_ASSERT_FALSE(wifi_pmkid_pcap_extract(pcap, pcap_len, bssid, pmkid));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_extracts_standard_pmkid);
    RUN_TEST(test_rejects_truncated_kde);
    RUN_TEST(test_rejects_non_pmkid_vendor_element);
    return UNITY_END();
}
