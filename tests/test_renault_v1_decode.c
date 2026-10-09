/* See COPYING.txt for license details. */

/*
 * test_renault_v1_decode.c
 *
 * Host unit tests for Sub_Ghz/protocols/m1_renault_v1_decode.c — the ported
 * ProtoPirate "Renault V1" (HITAG2-frame) automotive keyfob decoder.
 *
 *   1. m1_renault_v1_parse_frame() — pure 13-byte frame validation (16-bit
 *      header + XOR8 checksum) + field extraction.
 *   2. subghz_decode_renault_v1() — full pulse-array Manchester decode.
 */

#include <string.h>
#include "unity.h"
#include "m1_sub_ghz_decenc.h"

#define RV1_BYTES 13u
#define RV1_BITS  104u
#define RV1_TE_SHORT 125u
#define RV1_TE_LONG  250u

bool m1_renault_v1_parse_frame(const uint8_t full[RV1_BYTES],
                               uint32_t *serial, uint8_t *button,
                               uint16_t *counter, uint32_t *hop);
uint8_t subghz_decode_renault_v1(uint16_t p, uint16_t pulsecount);

void setUp(void) { memset(&subghz_decenc_ctl, 0, sizeof(subghz_decenc_ctl)); }
void tearDown(void) {}

static uint16_t build_manchester_pulses(const uint8_t *raw, uint16_t nbits,
                                        uint16_t te_short, uint16_t te_long)
{
    uint8_t cells[256 * 2u];
    uint16_t nc = 0;
    for (uint16_t b = 0; b < nbits; b++)
    {
        uint8_t bit = (raw[b >> 3] >> (7u - (b & 7u))) & 1u;
        if (bit) { cells[nc++] = 1; cells[nc++] = 0; }
        else     { cells[nc++] = 0; cells[nc++] = 1; }
    }
    uint16_t np = 0, i = 0;
    while (i < nc)
    {
        uint16_t run = 1;
        while (i + run < nc && cells[i + run] == cells[i]) run++;
        subghz_decenc_ctl.pulse_times[np++] = (run >= 2) ? te_long : te_short;
        i += run;
    }
    return np;
}

/* Build a valid frame: header 0x0001, chosen payload, computed XOR8. */
static void make_frame(uint8_t full[RV1_BYTES])
{
    memset(full, 0, RV1_BYTES);
    full[0] = 0x00; full[1] = 0x01;                 /* header */
    uint8_t *raw = &full[2];
    raw[0] = 0x12; raw[1] = 0x34; raw[2] = 0x56; raw[3] = 0x78; /* serial */
    raw[4] = 0x1A;                                  /* button=1, cnt hi=0xA */
    raw[5] = 0x8C;                                  /* cnt lo = 0x8C>>2 = 0x23 */
    raw[6] = 0xDE; raw[7] = 0xAD; raw[8] = 0xBE; raw[9] = 0xEF; /* hop */
    uint8_t x = 0;
    for (uint8_t i = 0; i < 10u; i++) x ^= raw[i];
    raw[10] = x;                                    /* XOR8 checksum */
}

void test_parse_fields(void)
{
    uint8_t full[RV1_BYTES];
    make_frame(full);
    uint32_t serial = 0, hop = 0; uint16_t cnt = 0; uint8_t btn = 0;
    TEST_ASSERT_TRUE(m1_renault_v1_parse_frame(full, &serial, &btn, &cnt, &hop));
    TEST_ASSERT_EQUAL_HEX32(0x12345678u, serial);
    TEST_ASSERT_EQUAL_HEX8(1u, btn);
    /* counter = ((raw[4]&0xF)<<6)|(raw[5]>>2) = (0xA<<6)|0x23 = 0x2A3 */
    TEST_ASSERT_EQUAL_HEX16(0x2A3u, cnt);
}

void test_parse_rejects_bad_header(void)
{
    uint8_t full[RV1_BYTES];
    make_frame(full);
    full[1] = 0x02; /* header != 0x0001 */
    TEST_ASSERT_FALSE(m1_renault_v1_parse_frame(full, NULL, NULL, NULL, NULL));
}

void test_parse_rejects_bad_checksum(void)
{
    uint8_t full[RV1_BYTES];
    make_frame(full);
    full[12] ^= 0xFF; /* corrupt XOR8 */
    TEST_ASSERT_FALSE(m1_renault_v1_parse_frame(full, NULL, NULL, NULL, NULL));
}

void test_parse_rejects_zero_serial(void)
{
    uint8_t full[RV1_BYTES];
    make_frame(full);
    full[2] = full[3] = full[4] = full[5] = 0x00;
    /* recompute checksum so only the serial rule rejects */
    uint8_t *raw = &full[2]; uint8_t x = 0;
    for (uint8_t i = 0; i < 10u; i++) x ^= raw[i];
    raw[10] = x;
    TEST_ASSERT_FALSE(m1_renault_v1_parse_frame(full, NULL, NULL, NULL, NULL));
}

void test_parse_null(void)
{
    TEST_ASSERT_FALSE(m1_renault_v1_parse_frame(NULL, NULL, NULL, NULL, NULL));
}

void test_decode_waveform(void)
{
    uint8_t full[RV1_BYTES];
    make_frame(full);
    uint16_t n = build_manchester_pulses(full, RV1_BITS, RV1_TE_SHORT, RV1_TE_LONG);
    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_renault_v1(RENAULT_V1, n));
    TEST_ASSERT_EQUAL_UINT16(RV1_BITS, subghz_decenc_ctl.ndecodedbitlength);
    TEST_ASSERT_EQUAL_UINT16(RENAULT_V1, subghz_decenc_ctl.ndecodedprotocol);
    TEST_ASSERT_EQUAL_HEX32(0x12345678u, subghz_decenc_ctl.n32_serialnumber);
    TEST_ASSERT_EQUAL_HEX8(1u, subghz_decenc_ctl.n8_buttonid);
}

void test_decode_rejects_noise(void)
{
    for (uint16_t i = 0; i < 64; i++)
        subghz_decenc_ctl.pulse_times[i] = (uint16_t)(900 + i * 7);
    TEST_ASSERT_EQUAL_UINT8(1, subghz_decode_renault_v1(RENAULT_V1, 64));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_parse_fields);
    RUN_TEST(test_parse_rejects_bad_header);
    RUN_TEST(test_parse_rejects_bad_checksum);
    RUN_TEST(test_parse_rejects_zero_serial);
    RUN_TEST(test_parse_null);
    RUN_TEST(test_decode_waveform);
    RUN_TEST(test_decode_rejects_noise);
    return UNITY_END();
}
