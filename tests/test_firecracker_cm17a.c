/* See COPYING.txt for license details. */

/*
 * test_firecracker_cm17a.c
 *
 * Unit tests for the FireCracker (CM17A) Sub-GHz protocol decoder.
 *
 * Verifies:
 *   1. Correct decode of valid 40-bit CM17A packets (preamble + header +
 *      data + footer).
 *   2. Rejection of packets with wrong header or wrong footer.
 *   3. Rejection of truncated packets (fewer than 40 bits).
 *   4. House-code, unit-number, and command decoding for several commands.
 *
 * Physical layer used:
 *   - Preamble : 9000µs HIGH, 4500µs LOW
 *   - Bit '0'  : 562µs HIGH, 562µs LOW
 *   - Bit '1'  : 562µs HIGH, 1688µs LOW
 *
 * CM17A 40-bit frame:
 *   [16-bit header 0xD5AA][16-bit data][8-bit footer 0xAD]
 *
 * Data byte layout (from the CM17A spec):
 *   house_byte = data >> 8
 *   cmd_byte   = data & 0xFF
 *   House codes (A-P) are encoded in house_byte[7:4] (non-sequential).
 *   Unit 9-16 flag = house_byte bit 2.
 *   Unit 1-8  code = cmd_byte bits [6,4,3].
 *   Command: bit7=0 → ON (bit5=0) or OFF (bit5=1);
 *            bit7=1 → BRIGHT (bit4=0) or DIM (bit4=1).
 *
 * Build:
 *   cmake -B build-tests -S tests && cmake --build build-tests
 *   ctest --test-dir build-tests --output-on-failure
 */

#include <string.h>
#include "unity.h"
#include "m1_sub_ghz_decenc.h"

/* ======================================================================= */
/* Forward declarations                                                    */
/* ======================================================================= */

uint8_t subghz_decode_firecracker_cm17a(uint16_t p, uint16_t pulsecount);

/* ======================================================================= */
/* Test helpers                                                             */
/* ======================================================================= */

void setUp(void)
{
    memset(&subghz_decenc_ctl, 0, sizeof(subghz_decenc_ctl));
}

void tearDown(void) {}

/*
 * Build a CM17A pulse sequence in subghz_decenc_ctl.pulse_times[]:
 *   [preamble: 9000µs HIGH, 4500µs LOW]
 *   [40 data bits, MSB first: 562µs HIGH, then 562 or 1688µs LOW]
 *
 * Parameters:
 *   packet40  — complete 40-bit packet (header|data|footer), MSB first
 *
 * Returns the total pulse count written.
 */
static uint16_t build_cm17a_frame(uint64_t packet40)
{
    const uint16_t TE_SHORT  = 562;
    const uint16_t TE_LONG   = 1688;
    const uint16_t PREAMBLE_H = 9000;
    const uint16_t PREAMBLE_L = 4500;

    uint16_t idx = 0;

    /* Preamble */
    subghz_decenc_ctl.pulse_times[idx++] = PREAMBLE_H;
    subghz_decenc_ctl.pulse_times[idx++] = PREAMBLE_L;

    /* 40 data bits, MSB first */
    for (int b = 39; b >= 0 && idx + 1 < PACKET_PULSE_COUNT_MAX; b--)
    {
        subghz_decenc_ctl.pulse_times[idx++] = TE_SHORT;
        if (packet40 & (1ULL << b))
            subghz_decenc_ctl.pulse_times[idx++] = TE_LONG;
        else
            subghz_decenc_ctl.pulse_times[idx++] = TE_SHORT;
    }

    return idx;
}

/*
 * Assemble a CM17A 40-bit packet from house_byte, cmd_byte.
 *   packet = (0xD5AA << 24) | (house_byte << 16) | (cmd_byte << 8) | 0xAD
 */
static uint64_t make_packet(uint8_t house_byte, uint8_t cmd_byte)
{
    return ((uint64_t)0xD5AAu << 24)
         | ((uint64_t)house_byte << 16)
         | ((uint64_t)cmd_byte   << 8)
         | (uint64_t)0xADu;
}

/*
 * The decoder reads timing from subghz_protocol_registry[p].
 * PROTO_IDX=0 is the index used by the test registry stub
 * (see subghz_firecracker_test_registry.c linked into this test).
 */
#define PROTO_IDX  0u

/* ======================================================================= */
/* Tests — valid packets                                                   */
/* ======================================================================= */

/*
 * House code A, unit 1, ON
 *   house_byte = 0x60 (A), cmd_byte = 0x00 (unit 1 ON)
 *   Expect: house_letter=0 (A), unit=1, cmd_class=0 (ON)
 *   cnt = (0 << 8) | (1 << 2) | 0 = 4
 */
void test_a1_on(void)
{
    uint64_t pkt = make_packet(0x60, 0x00);
    uint16_t pc = build_cm17a_frame(pkt);
    uint8_t ret = subghz_decode_firecracker_cm17a(PROTO_IDX, pc);
    TEST_ASSERT_EQUAL_UINT8(0, ret);
    /* full 40-bit packet should be stored */
    TEST_ASSERT_EQUAL_UINT64(pkt, subghz_decenc_ctl.n64_decodedvalue);
    TEST_ASSERT_EQUAL_UINT16(40, subghz_decenc_ctl.ndecodedbitlength);
    /* serial = house_byte, btn = cmd_byte */
    TEST_ASSERT_EQUAL_UINT32(0x60, subghz_decenc_ctl.n32_serialnumber);
    TEST_ASSERT_EQUAL_UINT8(0x00, subghz_decenc_ctl.n8_buttonid);
    /* cnt encodes: house_letter=0 (A), unit=1, cmd_class=0 (ON) */
    uint32_t cnt = subghz_decenc_ctl.n32_rollingcode;
    TEST_ASSERT_EQUAL_UINT8(0, (cnt >> 8) & 0xFF);   /* house letter A */
    TEST_ASSERT_EQUAL_UINT8(1, (cnt >> 2) & 0x3F);   /* unit 1 */
    TEST_ASSERT_EQUAL_UINT8(0, cnt & 0x03);           /* ON */
}

/*
 * House code A, unit 1, OFF
 *   cmd_byte = 0x20 (OFF bit set)
 */
void test_a1_off(void)
{
    uint64_t pkt = make_packet(0x60, 0x20);
    uint16_t pc = build_cm17a_frame(pkt);
    uint8_t ret = subghz_decode_firecracker_cm17a(PROTO_IDX, pc);
    TEST_ASSERT_EQUAL_UINT8(0, ret);
    uint32_t cnt = subghz_decenc_ctl.n32_rollingcode;
    TEST_ASSERT_EQUAL_UINT8(0, (cnt >> 8) & 0xFF);   /* house letter A */
    TEST_ASSERT_EQUAL_UINT8(1, (cnt >> 2) & 0x3F);   /* unit 1 */
    TEST_ASSERT_EQUAL_UINT8(1, cnt & 0x03);           /* OFF */
}

/*
 * House code A, unit 2, ON
 *   unit 2 code: cmd_byte bits[6,4,3] = 010 → 0x10
 *   cmd_byte = 0x10 (unit 2, ON)
 */
void test_a2_on(void)
{
    uint64_t pkt = make_packet(0x60, 0x10);
    uint16_t pc = build_cm17a_frame(pkt);
    uint8_t ret = subghz_decode_firecracker_cm17a(PROTO_IDX, pc);
    TEST_ASSERT_EQUAL_UINT8(0, ret);
    uint32_t cnt = subghz_decenc_ctl.n32_rollingcode;
    TEST_ASSERT_EQUAL_UINT8(0, (cnt >> 8) & 0xFF);   /* house letter A */
    TEST_ASSERT_EQUAL_UINT8(2, (cnt >> 2) & 0x3F);   /* unit 2 */
    TEST_ASSERT_EQUAL_UINT8(0, cnt & 0x03);           /* ON */
}

/*
 * House code A, unit 3, OFF
 *   unit 3 code: cmd_byte bits[6,4,3] = 001 → 0x08
 *   cmd_byte = 0x08 | 0x20 = 0x28 (unit 3, OFF)
 */
void test_a3_off(void)
{
    uint64_t pkt = make_packet(0x60, 0x28);
    uint16_t pc = build_cm17a_frame(pkt);
    uint8_t ret = subghz_decode_firecracker_cm17a(PROTO_IDX, pc);
    TEST_ASSERT_EQUAL_UINT8(0, ret);
    uint32_t cnt = subghz_decenc_ctl.n32_rollingcode;
    TEST_ASSERT_EQUAL_UINT8(0, (cnt >> 8) & 0xFF);   /* house letter A */
    TEST_ASSERT_EQUAL_UINT8(3, (cnt >> 2) & 0x3F);   /* unit 3 */
    TEST_ASSERT_EQUAL_UINT8(1, cnt & 0x03);           /* OFF */
}

/*
 * House code B, unit 5, ON
 *   B: house_byte = 0x70
 *   unit 5: cmd_byte bits[6,4,3] = 100 → 0x40
 *   cmd_byte = 0x40 (unit 5, ON)
 *   house_letter for B = 1
 */
void test_b5_on(void)
{
    uint64_t pkt = make_packet(0x70, 0x40);
    uint16_t pc = build_cm17a_frame(pkt);
    uint8_t ret = subghz_decode_firecracker_cm17a(PROTO_IDX, pc);
    TEST_ASSERT_EQUAL_UINT8(0, ret);
    uint32_t cnt = subghz_decenc_ctl.n32_rollingcode;
    TEST_ASSERT_EQUAL_UINT8(1, (cnt >> 8) & 0xFF);   /* house letter B */
    TEST_ASSERT_EQUAL_UINT8(5, (cnt >> 2) & 0x3F);   /* unit 5 */
    TEST_ASSERT_EQUAL_UINT8(0, cnt & 0x03);           /* ON */
}

/*
 * House code E, unit 9, ON
 *   E: house_byte = 0x80 (base) + 0x04 (unit 9-16 flag) = 0x84
 *   unit 9 = group-high + unit offset 0 = units 9-16, offset 0 → unit 9
 *   unit 9's offset within its group is 0, so cmd_byte bits[6,4,3] = 000 → 0x00
 *   cmd_byte = 0x00 (unit 9, ON)
 */
void test_e9_on(void)
{
    uint64_t pkt = make_packet(0x84, 0x00);
    uint16_t pc = build_cm17a_frame(pkt);
    uint8_t ret = subghz_decode_firecracker_cm17a(PROTO_IDX, pc);
    TEST_ASSERT_EQUAL_UINT8(0, ret);
    uint32_t cnt = subghz_decenc_ctl.n32_rollingcode;
    TEST_ASSERT_EQUAL_UINT8(4, (cnt >> 8) & 0xFF);   /* house letter E (index 4) */
    TEST_ASSERT_EQUAL_UINT8(9, (cnt >> 2) & 0x3F);   /* unit 9 */
    TEST_ASSERT_EQUAL_UINT8(0, cnt & 0x03);           /* ON */
}

/*
 * House code P, unit 8, DIM
 *   P: house_byte = 0x30
 *   unit 8: cmd_byte bits[6,4,3] = 111 → 0x58
 *   DIM: cmd_byte[7]=1, cmd_byte[4]=1 → 0x98
 *   cmd_byte = 0x58 | 0x98 = 0xD8  (unit 8 + DIM)
 *   house_letter for P = 15
 */
void test_p8_dim(void)
{
    uint64_t pkt = make_packet(0x30, 0xD8);
    uint16_t pc = build_cm17a_frame(pkt);
    uint8_t ret = subghz_decode_firecracker_cm17a(PROTO_IDX, pc);
    TEST_ASSERT_EQUAL_UINT8(0, ret);
    uint32_t cnt = subghz_decenc_ctl.n32_rollingcode;
    TEST_ASSERT_EQUAL_UINT8(15, (cnt >> 8) & 0xFF);  /* house letter P (index 15) */
    TEST_ASSERT_EQUAL_UINT8(8,  (cnt >> 2) & 0x3F);  /* unit 8 */
    TEST_ASSERT_EQUAL_UINT8(2,  cnt & 0x03);          /* DIM */
}

/*
 * BRIGHT command
 *   house_byte = 0x60 (A)
 *   BRIGHT: cmd_byte bit7=1, bit4=0 → 0x88
 */
void test_a_bright(void)
{
    uint64_t pkt = make_packet(0x60, 0x88);
    uint16_t pc = build_cm17a_frame(pkt);
    uint8_t ret = subghz_decode_firecracker_cm17a(PROTO_IDX, pc);
    TEST_ASSERT_EQUAL_UINT8(0, ret);
    uint32_t cnt = subghz_decenc_ctl.n32_rollingcode;
    TEST_ASSERT_EQUAL_UINT8(3, cnt & 0x03);           /* BRIGHT */
}

/* ======================================================================= */
/* Tests — rejection cases                                                 */
/* ======================================================================= */

/*
 * Wrong header: replace 0xD5AA with 0xAAAA — should reject.
 */
void test_rejects_wrong_header(void)
{
    /* manually build 40-bit packet with bad header */
    uint64_t bad_pkt = ((uint64_t)0xAAAAu << 24)
                     | ((uint64_t)0x60u   << 16)
                     | ((uint64_t)0x00u   << 8)
                     | (uint64_t)0xADu;
    uint16_t pc = build_cm17a_frame(bad_pkt);
    uint8_t ret = subghz_decode_firecracker_cm17a(PROTO_IDX, pc);
    TEST_ASSERT_EQUAL_UINT8(1, ret);
}

/*
 * Wrong footer: replace 0xAD with 0xFF — should reject.
 */
void test_rejects_wrong_footer(void)
{
    uint64_t bad_pkt = ((uint64_t)0xD5AAu << 24)
                     | ((uint64_t)0x60u   << 16)
                     | ((uint64_t)0x00u   << 8)
                     | (uint64_t)0xFFu;
    uint16_t pc = build_cm17a_frame(bad_pkt);
    uint8_t ret = subghz_decode_firecracker_cm17a(PROTO_IDX, pc);
    TEST_ASSERT_EQUAL_UINT8(1, ret);
}

/*
 * Truncated packet: only 24 bits — should reject (< 40 min bits).
 */
void test_rejects_truncated_packet(void)
{
    const uint16_t TE_SHORT = 562;
    uint16_t idx = 0;

    /* preamble */
    subghz_decenc_ctl.pulse_times[idx++] = 9000;
    subghz_decenc_ctl.pulse_times[idx++] = 4500;

    /* only 24 data bits (not enough) */
    for (int b = 39; b >= 16 && idx + 1 < PACKET_PULSE_COUNT_MAX; b--)
    {
        subghz_decenc_ctl.pulse_times[idx++] = TE_SHORT;
        subghz_decenc_ctl.pulse_times[idx++] = TE_SHORT;  /* all zeros */
    }

    uint8_t ret = subghz_decode_firecracker_cm17a(PROTO_IDX, idx);
    TEST_ASSERT_EQUAL_UINT8(1, ret);
}

/*
 * No preamble: just raw data bits with no 9ms/4.5ms leader.
 * The decoder must fail to locate the preamble and decode no bits.
 */
void test_rejects_no_preamble(void)
{
    const uint16_t TE_SHORT = 562;
    uint16_t idx = 0;

    /* raw data bits only, no preamble */
    for (int b = 39; b >= 0 && idx + 1 < PACKET_PULSE_COUNT_MAX; b--)
    {
        subghz_decenc_ctl.pulse_times[idx++] = TE_SHORT;
        subghz_decenc_ctl.pulse_times[idx++] = TE_SHORT;
    }

    uint8_t ret = subghz_decode_firecracker_cm17a(PROTO_IDX, idx);
    TEST_ASSERT_EQUAL_UINT8(1, ret);
}

/* ======================================================================= */
/* main                                                                    */
/* ======================================================================= */

int main(void)
{
    UNITY_BEGIN();

    /* Valid CM17A packets — various house codes, units, and commands */
    RUN_TEST(test_a1_on);
    RUN_TEST(test_a1_off);
    RUN_TEST(test_a2_on);
    RUN_TEST(test_a3_off);
    RUN_TEST(test_b5_on);
    RUN_TEST(test_e9_on);
    RUN_TEST(test_p8_dim);
    RUN_TEST(test_a_bright);

    /* Rejection tests */
    RUN_TEST(test_rejects_wrong_header);
    RUN_TEST(test_rejects_wrong_footer);
    RUN_TEST(test_rejects_truncated_packet);
    RUN_TEST(test_rejects_no_preamble);

    return UNITY_END();
}
