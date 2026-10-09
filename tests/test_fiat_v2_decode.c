/* See COPYING.txt for license details. */

/*
 * test_fiat_v2_decode.c
 *
 * Host unit tests for Sub_Ghz/protocols/m1_fiat_v2_decode.c — the ported
 * ProtoPirate "Fiat V2" (FCA) automotive keyfob decoder.
 *
 * Two layers are exercised:
 *   1. m1_fiat_v2_parse_frame() — pure 14-byte frame validation + field
 *      extraction (markers, UID, button, hop, counter; FCA vs non-FCA).
 *   2. subghz_decode_fiat_v2() — full pulse-array decode: a synthetic
 *      Manchester waveform is built from a known frame, copied into
 *      subghz_decenc_ctl.pulse_times[], and decoded back to its fields.
 *
 * Build:
 *   cmake -B build-tests -S tests && cmake --build build-tests
 *   ctest --test-dir build-tests --output-on-failure
 */

#include <string.h>
#include "unity.h"
#include "m1_sub_ghz_decenc.h"

#define FV2_BYTES 14u
#define FV2_BITS  112u
#define FV2_TE_SHORT 210u
#define FV2_TE_LONG  420u

/* Functions under test. */
bool m1_fiat_v2_parse_frame(const uint8_t raw[FV2_BYTES],
                            uint32_t *uid, uint8_t *button,
                            uint32_t *hop, uint32_t *counter);
uint8_t subghz_decode_fiat_v2(uint16_t p, uint16_t pulsecount);

void setUp(void)
{
    memset(&subghz_decenc_ctl, 0, sizeof(subghz_decenc_ctl));
}
void tearDown(void) {}

/*
 * Build a synthetic Manchester pulse train for a 14-byte frame into
 * subghz_decenc_ctl.pulse_times[].
 *
 * Each logical bit (MSB first) maps to two Manchester cells:
 *   bit 1 -> {1,0}   bit 0 -> {0,1}
 * Adjacent equal cells are merged into runs; a run of length 1 emits a SHORT
 * pulse, a run of length 2 emits a LONG pulse.  Runs alternate level, which is
 * exactly what a real OOK capture looks like.  Returns the pulse count.
 */
static uint16_t build_fiat_v2_pulses(const uint8_t raw[FV2_BYTES])
{
    uint8_t cells[FV2_BITS * 2u];
    uint16_t nc = 0;
    for (uint8_t b = 0; b < FV2_BITS; b++)
    {
        uint8_t bit = (raw[b >> 3] >> (7u - (b & 7u))) & 1u;
        if (bit) { cells[nc++] = 1; cells[nc++] = 0; }
        else     { cells[nc++] = 0; cells[nc++] = 1; }
    }

    uint16_t np = 0;
    uint16_t i = 0;
    while (i < nc)
    {
        uint16_t run = 1;
        while (i + run < nc && cells[i + run] == cells[i])
            run++;
        /* Manchester runs are at most length 2. */
        subghz_decenc_ctl.pulse_times[np++] = (run >= 2) ? FV2_TE_LONG : FV2_TE_SHORT;
        i += run;
    }
    return np;
}

/* A known-valid non-FCA frame. */
static void make_frame_nonfca(uint8_t raw[FV2_BYTES])
{
    memset(raw, 0, FV2_BYTES);
    raw[0] = 0x00; raw[1] = 0x01;                 /* markers */
    raw[2] = 0x12; raw[3] = 0x34; raw[4] = 0x56; raw[5] = 0x78; /* UID */
    raw[6] = 0x00;                                /* type nibble != 0xD -> non-FCA */
    raw[7] = 0xC0;                                /* button sel=3 (Unlock), low6=0 */
    raw[8] = 0x00;
    raw[9]  = 0xDE; raw[10] = 0xAD; raw[11] = 0xBE; raw[12] = 0xEF; /* hop */
    raw[13] = 0x00;
}

/* A known-valid FCA frame (type nibble 0xD). */
static void make_frame_fca(uint8_t raw[FV2_BYTES])
{
    memset(raw, 0, FV2_BYTES);
    raw[0] = 0x00; raw[1] = 0x01;
    raw[2] = 0xAB; raw[3] = 0xCD; raw[4] = 0xEF; raw[5] = 0x01; /* UID */
    raw[6] = 0xD0;                                /* FCA layout */
    raw[7] = 0x80;                                /* button sel=2 (Lock) */
    raw[8] = 0x00; raw[9] = 0x00;
    raw[10] = 0xCA; raw[11] = 0xFE; raw[12] = 0xF0; raw[13] = 0x0D; /* hop (FCA) */
}

/* ===================================================================
 * Pure frame parser
 * =================================================================== */

void test_parse_nonfca_fields(void)
{
    uint8_t raw[FV2_BYTES];
    make_frame_nonfca(raw);

    uint32_t uid = 0, hop = 0, cnt = 0;
    uint8_t  btn = 0;
    TEST_ASSERT_TRUE(m1_fiat_v2_parse_frame(raw, &uid, &btn, &hop, &cnt));
    TEST_ASSERT_EQUAL_HEX32(0x12345678u, uid);
    TEST_ASSERT_EQUAL_HEX8(0xC0u, btn);
    TEST_ASSERT_EQUAL_HEX32(0xDEADBEEFu, hop);
    /* non-FCA counter: ~(((raw[7]&0x3F)<<5)|(raw[8]>>3)) & 0x7FF = 0x7FF */
    TEST_ASSERT_EQUAL_HEX32(0x7FFu, cnt);
}

void test_parse_fca_fields(void)
{
    uint8_t raw[FV2_BYTES];
    make_frame_fca(raw);

    uint32_t uid = 0, hop = 0, cnt = 0;
    uint8_t  btn = 0;
    TEST_ASSERT_TRUE(m1_fiat_v2_parse_frame(raw, &uid, &btn, &hop, &cnt));
    TEST_ASSERT_EQUAL_HEX32(0xABCDEF01u, uid);
    TEST_ASSERT_EQUAL_HEX8(0x80u, btn);
    TEST_ASSERT_EQUAL_HEX32(0xCAFEF00Du, hop);
    /* FCA counter: ~((raw[8]<<6)|(raw[9]>>2)) & 0x3FFF = 0x3FFF */
    TEST_ASSERT_EQUAL_HEX32(0x3FFFu, cnt);
}

void test_parse_rejects_bad_markers(void)
{
    uint8_t raw[FV2_BYTES];
    make_frame_nonfca(raw);
    raw[0] = 0xFF; /* corrupt marker */
    TEST_ASSERT_FALSE(m1_fiat_v2_parse_frame(raw, NULL, NULL, NULL, NULL));
}

void test_parse_rejects_bad_button(void)
{
    uint8_t raw[FV2_BYTES];
    make_frame_nonfca(raw);
    raw[7] = 0x00; /* sel=0 invalid */
    TEST_ASSERT_FALSE(m1_fiat_v2_parse_frame(raw, NULL, NULL, NULL, NULL));
}

void test_parse_rejects_zero_uid(void)
{
    uint8_t raw[FV2_BYTES];
    make_frame_nonfca(raw);
    raw[2] = raw[3] = raw[4] = raw[5] = 0x00;
    TEST_ASSERT_FALSE(m1_fiat_v2_parse_frame(raw, NULL, NULL, NULL, NULL));
}

void test_parse_null(void)
{
    TEST_ASSERT_FALSE(m1_fiat_v2_parse_frame(NULL, NULL, NULL, NULL, NULL));
}

/* ===================================================================
 * Full pulse-array decode
 * =================================================================== */

void test_decode_nonfca_waveform(void)
{
    uint8_t raw[FV2_BYTES];
    make_frame_nonfca(raw);
    uint16_t n = build_fiat_v2_pulses(raw);

    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_fiat_v2(FIAT_V2, n));
    TEST_ASSERT_EQUAL_UINT16(FV2_BITS, subghz_decenc_ctl.ndecodedbitlength);
    TEST_ASSERT_EQUAL_UINT16(FIAT_V2, subghz_decenc_ctl.ndecodedprotocol);
    TEST_ASSERT_EQUAL_HEX32(0x12345678u, subghz_decenc_ctl.n32_serialnumber);
    TEST_ASSERT_EQUAL_HEX8(0xC0u, subghz_decenc_ctl.n8_buttonid);
    TEST_ASSERT_EQUAL_HEX64(0x12345678DEADBEEFull, subghz_decenc_ctl.n64_decodedvalue);
}

void test_decode_fca_waveform(void)
{
    uint8_t raw[FV2_BYTES];
    make_frame_fca(raw);
    uint16_t n = build_fiat_v2_pulses(raw);

    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_fiat_v2(FIAT_V2, n));
    TEST_ASSERT_EQUAL_HEX32(0xABCDEF01u, subghz_decenc_ctl.n32_serialnumber);
    TEST_ASSERT_EQUAL_HEX8(0x80u, subghz_decenc_ctl.n8_buttonid);
    TEST_ASSERT_EQUAL_HEX64(0xABCDEF01CAFEF00Dull, subghz_decenc_ctl.n64_decodedvalue);
}

void test_decode_rejects_noise(void)
{
    for (uint16_t i = 0; i < 64; i++)
        subghz_decenc_ctl.pulse_times[i] = (uint16_t)(1000 + i * 7);
    TEST_ASSERT_EQUAL_UINT8(1, subghz_decode_fiat_v2(FIAT_V2, 64));
}

void test_decode_rejects_truncated(void)
{
    uint8_t raw[FV2_BYTES];
    make_frame_nonfca(raw);
    uint16_t n = build_fiat_v2_pulses(raw);
    /* Drop the second half — not enough cells for a full frame. */
    TEST_ASSERT_EQUAL_UINT8(1, subghz_decode_fiat_v2(FIAT_V2, (uint16_t)(n / 2)));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_parse_nonfca_fields);
    RUN_TEST(test_parse_fca_fields);
    RUN_TEST(test_parse_rejects_bad_markers);
    RUN_TEST(test_parse_rejects_bad_button);
    RUN_TEST(test_parse_rejects_zero_uid);
    RUN_TEST(test_parse_null);
    RUN_TEST(test_decode_nonfca_waveform);
    RUN_TEST(test_decode_fca_waveform);
    RUN_TEST(test_decode_rejects_noise);
    RUN_TEST(test_decode_rejects_truncated);
    return UNITY_END();
}
