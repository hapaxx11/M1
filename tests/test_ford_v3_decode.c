/* See COPYING.txt for license details. */

/*
 * test_ford_v3_decode.c
 *
 * Host unit tests for Sub_Ghz/protocols/m1_ford_v3_decode.c — the ported
 * ProtoPirate "Ford V3" automotive keyfob decoder (US and EU field variants).
 */

#include <string.h>
#include "unity.h"
#include "m1_sub_ghz_decenc.h"

#define FV3_BYTES 13u
#define FV3_BITS  104u
#define FV3_TE_SHORT 240u
#define FV3_TE_LONG  480u
#define FV3_VARIANT_EU 0u
#define FV3_VARIANT_US 1u

bool m1_ford_v3_parse_frame(const uint8_t raw[FV3_BYTES],
                            uint8_t *variant, uint32_t *serial,
                            uint8_t *button, uint16_t *counter);
uint8_t subghz_decode_ford_v3(uint16_t p, uint16_t pulsecount);

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

static void make_frame_us(uint8_t raw[FV3_BYTES])
{
    memset(raw, 0, FV3_BYTES);
    raw[0] = 0xFF;
    raw[1] = 0x12; raw[2] = 0x34; raw[3] = 0x56; raw[4] = 0x78; /* serial */
    raw[5] = 0x80;                                  /* flag bit7 set -> US */
    raw[6] = 0x02;                                  /* button = Unlock */
    raw[7] = 0x11; raw[8] = 0x22;                   /* counter 0x1122 (plain) */
}

static void make_frame_eu(uint8_t raw[FV3_BYTES])
{
    memset(raw, 0, FV3_BYTES);
    raw[0] = 0xFF;
    raw[1] = 0x12; raw[2] = 0x34; raw[3] = 0x56; raw[4] = 0x78; /* serial */
    raw[5] = 0x00;                                  /* bit7 clear -> EU */
    raw[6] = 0x01;                                  /* bit0 set -> Unlock */
    raw[7] = 0x11; raw[8] = 0x22;                   /* counter inverted on read */
}

void test_parse_us_fields(void)
{
    uint8_t raw[FV3_BYTES]; make_frame_us(raw);
    uint8_t var = 0, btn = 0; uint32_t serial = 0; uint16_t cnt = 0;
    TEST_ASSERT_TRUE(m1_ford_v3_parse_frame(raw, &var, &serial, &btn, &cnt));
    TEST_ASSERT_EQUAL_UINT8(FV3_VARIANT_US, var);
    TEST_ASSERT_EQUAL_HEX32(0x12345678u, serial);
    TEST_ASSERT_EQUAL_HEX8(0x02u, btn);
    TEST_ASSERT_EQUAL_HEX16(0x1122u, cnt);
}

void test_parse_eu_fields(void)
{
    uint8_t raw[FV3_BYTES]; make_frame_eu(raw);
    uint8_t var = 0, btn = 0; uint32_t serial = 0; uint16_t cnt = 0;
    TEST_ASSERT_TRUE(m1_ford_v3_parse_frame(raw, &var, &serial, &btn, &cnt));
    TEST_ASSERT_EQUAL_UINT8(FV3_VARIANT_EU, var);
    TEST_ASSERT_EQUAL_HEX32(0x12345678u, serial);
    TEST_ASSERT_EQUAL_HEX8(0x02u, btn);          /* bit0 set -> Unlock(2) */
    /* counter = ((~0x11)<<8)|(~0x22) = 0xEEDD */
    TEST_ASSERT_EQUAL_HEX16(0xEEDDu, cnt);
}

void test_parse_rejects_bad_marker(void)
{
    uint8_t raw[FV3_BYTES]; make_frame_us(raw);
    raw[0] = 0x00;
    TEST_ASSERT_FALSE(m1_ford_v3_parse_frame(raw, NULL, NULL, NULL, NULL));
}

void test_parse_rejects_zero_serial(void)
{
    uint8_t raw[FV3_BYTES]; make_frame_us(raw);
    raw[1] = raw[2] = raw[3] = raw[4] = 0x00;
    TEST_ASSERT_FALSE(m1_ford_v3_parse_frame(raw, NULL, NULL, NULL, NULL));
}

void test_decode_us_waveform(void)
{
    uint8_t raw[FV3_BYTES]; make_frame_us(raw);
    uint16_t n = build_manchester_pulses(raw, FV3_BITS, FV3_TE_SHORT, FV3_TE_LONG);
    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_ford_v3(FORD_V3, n));
    TEST_ASSERT_EQUAL_UINT16(FORD_V3, subghz_decenc_ctl.ndecodedprotocol);
    TEST_ASSERT_EQUAL_HEX32(0x12345678u, subghz_decenc_ctl.n32_serialnumber);
    TEST_ASSERT_EQUAL_HEX8(0x02u, subghz_decenc_ctl.n8_buttonid);
}

void test_decode_rejects_noise(void)
{
    for (uint16_t i = 0; i < 64; i++)
        subghz_decenc_ctl.pulse_times[i] = (uint16_t)(1500 + i * 9);
    TEST_ASSERT_EQUAL_UINT8(1, subghz_decode_ford_v3(FORD_V3, 64));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_parse_us_fields);
    RUN_TEST(test_parse_eu_fields);
    RUN_TEST(test_parse_rejects_bad_marker);
    RUN_TEST(test_parse_rejects_zero_serial);
    RUN_TEST(test_decode_us_waveform);
    RUN_TEST(test_decode_rejects_noise);
    return UNITY_END();
}
