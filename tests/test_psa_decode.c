/* See COPYING.txt for license details. */

/*
 * test_psa_decode.c
 *
 * Host unit tests for Sub_Ghz/protocols/m1_psa_decode.c — the ported
 * ProtoPirate "PSA" automotive keyfob decoder (cipher-free Direct-XOR path).
 *
 * The expected field values below were derived by hand-tracing the reference
 * XOR network for the chosen frame (see comments).
 */

#include <string.h>
#include "unity.h"
#include "m1_sub_ghz_decenc.h"

#define PSA_BYTES 10u
#define PSA_BITS  80u
#define PSA_TE_SHORT 250u
#define PSA_TE_LONG  500u

bool m1_psa_parse_frame(const uint8_t frame[PSA_BYTES],
                        uint32_t *serial, uint8_t *button, uint16_t *counter);
uint8_t subghz_decode_psa(uint16_t p, uint16_t pulsecount);

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

static uint16_t add_psa_preamble_and_end(uint16_t pulse_count)
{
    const uint16_t prefix_count = 144u;
    memmove(&subghz_decenc_ctl.pulse_times[prefix_count],
            subghz_decenc_ctl.pulse_times,
            pulse_count * sizeof(subghz_decenc_ctl.pulse_times[0]));
    for (uint16_t i = 0; i < prefix_count - 1u; i++)
        subghz_decenc_ctl.pulse_times[i] = PSA_TE_SHORT;
    subghz_decenc_ctl.pulse_times[prefix_count - 1u] = PSA_TE_LONG;
    subghz_decenc_ctl.pulse_times[prefix_count + pulse_count] = 1000u;
    return (uint16_t)(pulse_count + prefix_count + 1u);
}

/*
 * Chosen for a compact Manchester pulse train that fits with the PSA preamble
 * in the production pulse buffer. After un-mixing: serial=0x051309,
 * counter=0x86F0.
 */
static void make_frame(uint8_t f[PSA_BYTES])
{
    memset(f, 0, PSA_BYTES);
    f[0] = 0x00;
    f[1] = 0x0A;                 /* marker: low nibble 0xA */
    f[2] = 0xAA; f[3] = 0xB5; f[4] = 0x5A; f[5] = 0xA6; f[6] = 0xDC; f[7] = 0xA3;
    f[8] = 0x91;                 /* Key2 high (checksum match + gate + button) */
    f[9] = 0xAA;                 /* Key2 low */
}

void test_parse_fields(void)
{
    uint8_t f[PSA_BYTES]; make_frame(f);
    uint32_t serial = 0; uint16_t cnt = 0; uint8_t btn = 0;
    TEST_ASSERT_TRUE(m1_psa_parse_frame(f, &serial, &btn, &cnt));
    TEST_ASSERT_EQUAL_HEX32(0x051309u, serial);
    TEST_ASSERT_EQUAL_HEX8(0x1u, btn);
    TEST_ASSERT_EQUAL_HEX16(0x86F0u, cnt);
}

void test_parse_rejects_bad_marker(void)
{
    uint8_t f[PSA_BYTES]; make_frame(f);
    f[1] = 0x0B; /* low nibble not 0xA */
    TEST_ASSERT_FALSE(m1_psa_parse_frame(f, NULL, NULL, NULL));
}

void test_parse_rejects_bad_checksum(void)
{
    uint8_t f[PSA_BYTES]; make_frame(f);
    f[8] = 0xB1; /* high nibble no longer matches nibble-sum -> checksum fails */
    TEST_ASSERT_FALSE(m1_psa_parse_frame(f, NULL, NULL, NULL));
}

void test_parse_null(void)
{
    TEST_ASSERT_FALSE(m1_psa_parse_frame(NULL, NULL, NULL, NULL));
}

void test_decode_waveform(void)
{
    uint8_t f[PSA_BYTES]; make_frame(f);
    uint16_t n = add_psa_preamble_and_end(
        build_manchester_pulses(f, PSA_BITS, PSA_TE_SHORT, PSA_TE_LONG));
    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_psa(PSA, n));
    TEST_ASSERT_EQUAL_UINT16(PSA_BITS, subghz_decenc_ctl.ndecodedbitlength);
    TEST_ASSERT_EQUAL_UINT16(PSA, subghz_decenc_ctl.ndecodedprotocol);
    TEST_ASSERT_EQUAL_HEX32(0x051309u, subghz_decenc_ctl.n32_serialnumber);
    TEST_ASSERT_EQUAL_HEX8(0x1u, subghz_decenc_ctl.n8_buttonid);
}

void test_decode_rejects_frame_without_preamble(void)
{
    uint8_t f[PSA_BYTES]; make_frame(f);
    uint16_t n = build_manchester_pulses(f, PSA_BITS, PSA_TE_SHORT, PSA_TE_LONG);
    subghz_decenc_ctl.pulse_times[n++] = 1000u;
    TEST_ASSERT_EQUAL_UINT8(1, subghz_decode_psa(PSA, n));
}

void test_decode_rejects_noise(void)
{
    for (uint16_t i = 0; i < 64; i++)
        subghz_decenc_ctl.pulse_times[i] = (uint16_t)(1200 + i * 11);
    TEST_ASSERT_EQUAL_UINT8(1, subghz_decode_psa(PSA, 64));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_parse_fields);
    RUN_TEST(test_parse_rejects_bad_marker);
    RUN_TEST(test_parse_rejects_bad_checksum);
    RUN_TEST(test_parse_null);
    RUN_TEST(test_decode_waveform);
    RUN_TEST(test_decode_rejects_frame_without_preamble);
    RUN_TEST(test_decode_rejects_noise);
    return UNITY_END();
}
