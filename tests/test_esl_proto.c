/* See COPYING.txt for license details. */

/**
 * @file   test_esl_proto.c
 * @brief  Unit tests for m1_esl_proto — ESL frame builder and barcode decoder.
 *
 * Tests CRC16, barcode-to-PLID decoding, and broadcast/ping frame construction.
 * m1_esl_proto.c is pure logic (no hardware), so it compiles directly on host.
 */

#include "unity.h"
#include "m1_esl_proto.h"
#include <string.h>

void setUp(void)   {}
void tearDown(void) {}

/* --------------------------------------------------------------------------- */
/* barcode_to_plid                                                             */
/* --------------------------------------------------------------------------- */

void test_barcode_null_returns_false(void)
{
    uint8_t plid[4];
    TEST_ASSERT_FALSE(m1_esl_barcode_to_plid(NULL, plid));
}

void test_barcode_null_plid_returns_false(void)
{
    TEST_ASSERT_FALSE(m1_esl_barcode_to_plid("04000100020000000", NULL));
}

void test_barcode_too_short_returns_false(void)
{
    uint8_t plid[4];
    TEST_ASSERT_FALSE(m1_esl_barcode_to_plid("040001000200000", plid)); /* 15 chars */
}

void test_barcode_too_long_returns_false(void)
{
    uint8_t plid[4];
    TEST_ASSERT_FALSE(m1_esl_barcode_to_plid("040001000200000001", plid)); /* 18 chars */
}

void test_barcode_non_digit_at_position_3_returns_false(void)
{
    uint8_t plid[4];
    /* index 2 (position 3) is 'X', not a digit */
    TEST_ASSERT_FALSE(m1_esl_barcode_to_plid("04X00100020000000", plid));
}

void test_barcode_valid_all_zeros(void)
{
    uint8_t plid[4];
    /* barcode: "04" + "00000" + "00000" + "0000" + "0"
     * lo = 0, hi = 0 → id = 0 → plid = {0,0,0,0} */
    TEST_ASSERT_TRUE(m1_esl_barcode_to_plid("04000000000000000", plid));
    TEST_ASSERT_EQUAL_UINT8(0U, plid[0]);
    TEST_ASSERT_EQUAL_UINT8(0U, plid[1]);
    TEST_ASSERT_EQUAL_UINT8(0U, plid[2]);
    TEST_ASSERT_EQUAL_UINT8(0U, plid[3]);
}

void test_barcode_valid_known_decode(void)
{
    /* barcode: "04" + "00001" + "00002" + "0000" + "0"
     * lo = 1, hi = 2
     * id = 1 + (2 << 16) = 1 + 131072 = 131073 = 0x00020001
     * plid[0] = (id >> 8)  & 0xFF = 0x00
     * plid[1] = id          & 0xFF = 0x01
     * plid[2] = (id >> 24) & 0xFF = 0x00
     * plid[3] = (id >> 16) & 0xFF = 0x02
     */
    uint8_t plid[4];
    TEST_ASSERT_TRUE(m1_esl_barcode_to_plid("04000010000200000", plid));
    TEST_ASSERT_EQUAL_UINT8(0x00U, plid[0]);
    TEST_ASSERT_EQUAL_UINT8(0x01U, plid[1]);
    TEST_ASSERT_EQUAL_UINT8(0x00U, plid[2]);
    TEST_ASSERT_EQUAL_UINT8(0x02U, plid[3]);
}

void test_barcode_valid_max_digits(void)
{
    /* Use barcode: "04" + "12345" + "67890" + "00000" (17 chars total).
     * lo = 12345 = 0x3039
     * hi = 67890 = 0x10932
     * hi << 16 as uint32_t: 0x10932 << 16 overflows, wraps to 0x09320000
     * id = 0x09320000 + 0x3039 = 0x09323039
     * plid[0] = (id >> 8)  & 0xFF = 0x30
     * plid[1] =  id        & 0xFF = 0x39
     * plid[2] = (id >> 24) & 0xFF = 0x09
     * plid[3] = (id >> 16) & 0xFF = 0x32
     */
    uint8_t plid[4] = {0};
    TEST_ASSERT_TRUE(m1_esl_barcode_to_plid("04123456789000000", plid));
    TEST_ASSERT_EQUAL_HEX8(0x30U, plid[0]);
    TEST_ASSERT_EQUAL_HEX8(0x39U, plid[1]);
    TEST_ASSERT_EQUAL_HEX8(0x09U, plid[2]);
    TEST_ASSERT_EQUAL_HEX8(0x32U, plid[3]);
}

/* --------------------------------------------------------------------------- */
/* broadcast_page_frame                                                        */
/* --------------------------------------------------------------------------- */

void test_broadcast_page_frame_null_buf_returns_zero(void)
{
    TEST_ASSERT_EQUAL(0U, m1_esl_build_broadcast_page_frame(NULL, 0, false, 60));
}

void test_broadcast_page_frame_length(void)
{
    uint8_t buf[M1_ESL_MAX_FRAME_SIZE];
    /* raw_frame (6) + payload (5) + CRC (2) = 13 bytes */
    size_t len = m1_esl_build_broadcast_page_frame(buf, 0, false, 120);
    TEST_ASSERT_EQUAL(13U, len);
}

void test_broadcast_page_frame_proto_byte(void)
{
    uint8_t buf[M1_ESL_MAX_FRAME_SIZE];
    m1_esl_build_broadcast_page_frame(buf, 0, false, 120);
    TEST_ASSERT_EQUAL_UINT8(M1_ESL_PROTO_DM, buf[0]);
}

void test_broadcast_page_frame_broadcast_plid(void)
{
    uint8_t buf[M1_ESL_MAX_FRAME_SIZE];
    m1_esl_build_broadcast_page_frame(buf, 3, false, 120);
    /* PLID bytes 1-4 should all be zero (broadcast address). */
    TEST_ASSERT_EQUAL_UINT8(0U, buf[1]);
    TEST_ASSERT_EQUAL_UINT8(0U, buf[2]);
    TEST_ASSERT_EQUAL_UINT8(0U, buf[3]);
    TEST_ASSERT_EQUAL_UINT8(0U, buf[4]);
}

void test_broadcast_page_frame_page_field(void)
{
    uint8_t buf[M1_ESL_MAX_FRAME_SIZE];
    /* Page 3: byte[6] should have bits [5:3] = 011 → ((3 << 3) | 0x01) = 0x19 */
    m1_esl_build_broadcast_page_frame(buf, 3, false, 120);
    TEST_ASSERT_EQUAL_UINT8(0x19U, buf[6]);
}

void test_broadcast_page_frame_forever_flag(void)
{
    uint8_t buf[M1_ESL_MAX_FRAME_SIZE];
    /* forever flag sets bit 7 of the page byte */
    m1_esl_build_broadcast_page_frame(buf, 0, true, 0);
    TEST_ASSERT_BITS(0x80U, 0x80U, buf[6]);
}

void test_broadcast_page_frame_not_forever_no_flag(void)
{
    uint8_t buf[M1_ESL_MAX_FRAME_SIZE];
    m1_esl_build_broadcast_page_frame(buf, 0, false, 120);
    TEST_ASSERT_BITS(0x80U, 0x00U, buf[6]);
}

void test_broadcast_page_frame_page_clamped_to_3bits(void)
{
    uint8_t buf_p0[M1_ESL_MAX_FRAME_SIZE];
    uint8_t buf_p8[M1_ESL_MAX_FRAME_SIZE];
    /* Page 8 wraps to page 0 (page & 7 == 0). */
    m1_esl_build_broadcast_page_frame(buf_p0, 0, false, 120);
    m1_esl_build_broadcast_page_frame(buf_p8, 8, false, 120);
    TEST_ASSERT_EQUAL_UINT8(buf_p0[6], buf_p8[6]);
}

/* --------------------------------------------------------------------------- */
/* broadcast_debug_frame                                                       */
/* --------------------------------------------------------------------------- */

void test_broadcast_debug_frame_null_buf_returns_zero(void)
{
    TEST_ASSERT_EQUAL(0U, m1_esl_build_broadcast_debug_frame(NULL));
}

void test_broadcast_debug_frame_length(void)
{
    uint8_t buf[M1_ESL_MAX_FRAME_SIZE];
    /* raw_frame (6) + payload (5) + CRC (2) = 13 bytes */
    size_t len = m1_esl_build_broadcast_debug_frame(buf);
    TEST_ASSERT_EQUAL(13U, len);
}

void test_broadcast_debug_frame_proto_byte(void)
{
    uint8_t buf[M1_ESL_MAX_FRAME_SIZE];
    m1_esl_build_broadcast_debug_frame(buf);
    TEST_ASSERT_EQUAL_UINT8(M1_ESL_PROTO_DM, buf[0]);
}

void test_broadcast_debug_frame_mode_byte(void)
{
    uint8_t buf[M1_ESL_MAX_FRAME_SIZE];
    m1_esl_build_broadcast_debug_frame(buf);
    /* Debug mode byte should be 0xF1. */
    TEST_ASSERT_EQUAL_UINT8(0xF1U, buf[6]);
}

/* --------------------------------------------------------------------------- */
/* ping_frame                                                                  */
/* --------------------------------------------------------------------------- */

void test_ping_frame_null_buf_returns_zero(void)
{
    const uint8_t plid[4] = {1, 2, 3, 4};
    TEST_ASSERT_EQUAL(0U, m1_esl_build_ping_frame(NULL, plid));
}

void test_ping_frame_null_plid_returns_zero(void)
{
    uint8_t buf[M1_ESL_MAX_FRAME_SIZE];
    TEST_ASSERT_EQUAL(0U, m1_esl_build_ping_frame(buf, NULL));
}

void test_ping_frame_length(void)
{
    uint8_t buf[M1_ESL_MAX_FRAME_SIZE];
    const uint8_t plid[4] = {0x12, 0x34, 0x56, 0x78};
    /* raw_frame (6) + 4 payload bytes + 22 × 0x01 bytes + CRC (2) = 34 bytes */
    size_t len = m1_esl_build_ping_frame(buf, plid);
    TEST_ASSERT_EQUAL(34U, len);
}

void test_ping_frame_plid_encoded_reversed(void)
{
    uint8_t buf[M1_ESL_MAX_FRAME_SIZE];
    const uint8_t plid[4] = {0x01, 0x02, 0x03, 0x04};
    m1_esl_build_ping_frame(buf, plid);
    /* raw_frame puts plid in reverse order: [plid3][plid2][plid1][plid0] */
    TEST_ASSERT_EQUAL_UINT8(plid[3], buf[1]);
    TEST_ASSERT_EQUAL_UINT8(plid[2], buf[2]);
    TEST_ASSERT_EQUAL_UINT8(plid[1], buf[3]);
    TEST_ASSERT_EQUAL_UINT8(plid[0], buf[4]);
}

void test_ping_frame_within_max_size(void)
{
    uint8_t buf[M1_ESL_MAX_FRAME_SIZE];
    const uint8_t plid[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    size_t len = m1_esl_build_ping_frame(buf, plid);
    TEST_ASSERT_LESS_OR_EQUAL(M1_ESL_MAX_FRAME_SIZE, len);
}

/* --------------------------------------------------------------------------- */
/* CRC consistency — same input always yields same frame bytes                 */
/* --------------------------------------------------------------------------- */

void test_page_frame_is_deterministic(void)
{
    uint8_t buf1[M1_ESL_MAX_FRAME_SIZE];
    uint8_t buf2[M1_ESL_MAX_FRAME_SIZE];
    size_t len1 = m1_esl_build_broadcast_page_frame(buf1, 2, false, 60);
    size_t len2 = m1_esl_build_broadcast_page_frame(buf2, 2, false, 60);
    TEST_ASSERT_EQUAL(len1, len2);
    TEST_ASSERT_EQUAL_MEMORY(buf1, buf2, len1);
}

void test_different_pages_yield_different_frames(void)
{
    uint8_t buf0[M1_ESL_MAX_FRAME_SIZE];
    uint8_t buf1[M1_ESL_MAX_FRAME_SIZE];
    size_t  len0 = m1_esl_build_broadcast_page_frame(buf0, 0, false, 120);
    size_t  len1 = m1_esl_build_broadcast_page_frame(buf1, 1, false, 120);
    TEST_ASSERT_EQUAL(len0, len1);
    /* At minimum the page byte and CRC must differ. */
    int same = (memcmp(buf0, buf1, len0) == 0);
    TEST_ASSERT_FALSE(same);
}

/* --------------------------------------------------------------------------- */
/* CRC known-good vector                                                       */
/* --------------------------------------------------------------------------- */

void test_page_frame_crc_known_good(void)
{
    /*
     * Broadcast page frame: page=2, forever=false, duration=60
     *   Raw bytes: 0x85 0x00 0x00 0x00 0x00 0x06 0x11 0x00 0x00 0x00 0x3C
     *   CRC16 (Pricer variant): 0x6B0E  → lo=0x0E, hi=0x6B
     *   Full frame length: 13 bytes
     */
    uint8_t buf[M1_ESL_MAX_FRAME_SIZE];
    size_t len = m1_esl_build_broadcast_page_frame(buf, 2, false, 60);
    TEST_ASSERT_EQUAL(13U, len);
    TEST_ASSERT_EQUAL_HEX8(0x0EU, buf[11]);  /* CRC lo byte */
    TEST_ASSERT_EQUAL_HEX8(0x6BU, buf[12]);  /* CRC hi byte */
}

/* --------------------------------------------------------------------------- */
/* main                                                                        */
/* --------------------------------------------------------------------------- */

int main(void)
{
    UNITY_BEGIN();

    /* barcode_to_plid */
    RUN_TEST(test_barcode_null_returns_false);
    RUN_TEST(test_barcode_null_plid_returns_false);
    RUN_TEST(test_barcode_too_short_returns_false);
    RUN_TEST(test_barcode_too_long_returns_false);
    RUN_TEST(test_barcode_non_digit_at_position_3_returns_false);
    RUN_TEST(test_barcode_valid_all_zeros);
    RUN_TEST(test_barcode_valid_known_decode);
    RUN_TEST(test_barcode_valid_max_digits);

    /* broadcast_page_frame */
    RUN_TEST(test_broadcast_page_frame_null_buf_returns_zero);
    RUN_TEST(test_broadcast_page_frame_length);
    RUN_TEST(test_broadcast_page_frame_proto_byte);
    RUN_TEST(test_broadcast_page_frame_broadcast_plid);
    RUN_TEST(test_broadcast_page_frame_page_field);
    RUN_TEST(test_broadcast_page_frame_forever_flag);
    RUN_TEST(test_broadcast_page_frame_not_forever_no_flag);
    RUN_TEST(test_broadcast_page_frame_page_clamped_to_3bits);

    /* broadcast_debug_frame */
    RUN_TEST(test_broadcast_debug_frame_null_buf_returns_zero);
    RUN_TEST(test_broadcast_debug_frame_length);
    RUN_TEST(test_broadcast_debug_frame_proto_byte);
    RUN_TEST(test_broadcast_debug_frame_mode_byte);

    /* ping_frame */
    RUN_TEST(test_ping_frame_null_buf_returns_zero);
    RUN_TEST(test_ping_frame_null_plid_returns_zero);
    RUN_TEST(test_ping_frame_length);
    RUN_TEST(test_ping_frame_plid_encoded_reversed);
    RUN_TEST(test_ping_frame_within_max_size);

    /* determinism */
    RUN_TEST(test_page_frame_is_deterministic);
    RUN_TEST(test_different_pages_yield_different_frames);

    /* CRC known-good vector */
    RUN_TEST(test_page_frame_crc_known_good);

    return UNITY_END();
}
