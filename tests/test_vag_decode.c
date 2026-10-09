/* See COPYING.txt for license details. */

/*
 * test_vag_decode.c
 *
 * Host unit tests for Sub_Ghz/protocols/m1_vag_decode.c — the ported
 * ProtoPirate "VAG" automotive keyfob *identify* decoder (T12 format).
 *
 *   1. m1_vag_t12_parse() — pure prefix/type + dispatch-byte(button) validation,
 *      with the payload one's-complement applied.
 *   2. subghz_decode_vag() — full 95-bit Manchester pulse-array decode.
 */

#include <string.h>
#include "unity.h"
#include "m1_sub_ghz_decenc.h"

#define VAG_BITS 95u
#define VAG_TE_SHORT 300u
#define VAG_TE_LONG  600u
#define VAG_PREFIX_T1 0x2F3Fu
#define VAG_PREFIX_T2 0x2F1Cu
#define VAG_BTN_UNLOCK 0x1u
#define VAG_BTN_LOCK   0x2u
#define VAG_BTN_BOOT   0x4u

bool m1_vag_t12_parse(uint16_t prefix, uint64_t key1_raw, uint16_t key2_raw,
                      uint8_t *type, uint8_t *button, uint64_t *key1_out);
uint8_t subghz_decode_vag(uint16_t p, uint16_t pulsecount);

void setUp(void) { memset(&subghz_decenc_ctl, 0, sizeof(subghz_decenc_ctl)); }
void tearDown(void) {}

/*
 * Build a 95-bit VAG T12 waveform into pulse_times[] from the three raw wire
 * fields (as received, before the decoder applies the one's-complement).
 */
static uint16_t build_vag_pulses(uint16_t prefix, uint64_t key1_raw,
                                 uint16_t key2_raw)
{
    uint8_t bits[VAG_BITS];
    uint16_t nb = 0;
    for (int i = 14; i >= 0; i--)  bits[nb++] = (prefix >> i) & 1u;      /* 15 */
    for (int i = 63; i >= 0; i--)  bits[nb++] = (key1_raw >> i) & 1u;    /* 64 */
    for (int i = 15; i >= 0; i--)  bits[nb++] = (key2_raw >> i) & 1u;    /* 16 */

    uint8_t cells[VAG_BITS * 2u];
    uint16_t nc = 0;
    for (uint16_t b = 0; b < VAG_BITS; b++)
    {
        if (bits[b]) { cells[nc++] = 1; cells[nc++] = 0; }
        else         { cells[nc++] = 0; cells[nc++] = 1; }
    }
    uint16_t np = 0, i = 0;
    while (i < nc)
    {
        uint16_t run = 1;
        while (i + run < nc && cells[i + run] == cells[i]) run++;
        subghz_decenc_ctl.pulse_times[np++] = (run >= 2) ? VAG_TE_LONG : VAG_TE_SHORT;
        i += run;
    }
    return np;
}

/* desired key1 = 0xAABBCCDD11223344, dispatch 0x2A (Lock). */
#define TEST_KEY1    0xAABBCCDD11223344ull
#define TEST_KEY1RAW (~TEST_KEY1)
#define TEST_KEY2    0x002Au                 /* high=0, low=dispatch 0x2A */
#define TEST_KEY2RAW ((uint16_t)~TEST_KEY2)

void test_parse_type1_lock(void)
{
    uint8_t type = 0, btn = 0; uint64_t key1 = 0;
    TEST_ASSERT_TRUE(m1_vag_t12_parse(VAG_PREFIX_T1, TEST_KEY1RAW, TEST_KEY2RAW,
                                      &type, &btn, &key1));
    TEST_ASSERT_EQUAL_UINT8(1u, type);
    TEST_ASSERT_EQUAL_HEX8(VAG_BTN_LOCK, btn);
    TEST_ASSERT_EQUAL_HEX64(TEST_KEY1, key1);
}

void test_parse_type2_unlock(void)
{
    uint8_t type = 0, btn = 0;
    uint16_t k2raw = (uint16_t)~0x001Cu;  /* dispatch 0x1C = Unlock */
    TEST_ASSERT_TRUE(m1_vag_t12_parse(VAG_PREFIX_T2, TEST_KEY1RAW, k2raw,
                                      &type, &btn, NULL));
    TEST_ASSERT_EQUAL_UINT8(2u, type);
    TEST_ASSERT_EQUAL_HEX8(VAG_BTN_UNLOCK, btn);
}

void test_parse_rejects_bad_prefix(void)
{
    TEST_ASSERT_FALSE(m1_vag_t12_parse(0x1234u, TEST_KEY1RAW, TEST_KEY2RAW,
                                       NULL, NULL, NULL));
}

void test_parse_rejects_bad_dispatch(void)
{
    uint16_t k2raw = (uint16_t)~0x0099u;  /* 0x99 not a valid dispatch */
    TEST_ASSERT_FALSE(m1_vag_t12_parse(VAG_PREFIX_T1, TEST_KEY1RAW, k2raw,
                                       NULL, NULL, NULL));
}

void test_decode_waveform(void)
{
    uint16_t n = build_vag_pulses(VAG_PREFIX_T1, TEST_KEY1RAW, TEST_KEY2RAW);
    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_vag(VAG, n));
    TEST_ASSERT_EQUAL_UINT16(VAG_BITS, subghz_decenc_ctl.ndecodedbitlength);
    TEST_ASSERT_EQUAL_UINT16(VAG, subghz_decenc_ctl.ndecodedprotocol);
    TEST_ASSERT_EQUAL_HEX8(VAG_BTN_LOCK, subghz_decenc_ctl.n8_buttonid);
    TEST_ASSERT_EQUAL_HEX64(TEST_KEY1, subghz_decenc_ctl.n64_decodedvalue);
    TEST_ASSERT_EQUAL_HEX32(0xAABBCCDDu, subghz_decenc_ctl.n32_serialnumber);
}

void test_decode_rejects_noise(void)
{
    for (uint16_t i = 0; i < 64; i++)
        subghz_decenc_ctl.pulse_times[i] = (uint16_t)(2000 + i * 13);
    TEST_ASSERT_EQUAL_UINT8(1, subghz_decode_vag(VAG, 64));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_parse_type1_lock);
    RUN_TEST(test_parse_type2_unlock);
    RUN_TEST(test_parse_rejects_bad_prefix);
    RUN_TEST(test_parse_rejects_bad_dispatch);
    RUN_TEST(test_decode_waveform);
    RUN_TEST(test_decode_rejects_noise);
    return UNITY_END();
}
