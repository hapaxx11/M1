/* See COPYING.txt for license details. */

/*
 * test_subghz_block_encoder.c
 *
 * Unit tests for subghz_block_encoder.h — the Flipper-compatible bit-array
 * and upload generation helpers used by protocol encoders for TX.
 *
 * Tests cover:
 *   - subghz_protocol_blocks_set_bit_array() — MSB-first bit indexing
 *   - subghz_protocol_blocks_get_bit_array() — round-trip
 *   - subghz_protocol_blocks_get_upload_from_bit_array() — waveform gen
 *   - Boundary: out-of-bounds, alignment modes, consecutive-bit merging
 */

#include <string.h>
#include "unity.h"
#include "subghz_block_encoder.h"

void setUp(void) {}
void tearDown(void) {}

/*============================================================================*/
/* Tests: set/get bit array — MSB-first layout                                */
/*============================================================================*/

void test_set_get_bit0_is_msb(void)
{
    uint8_t arr[2] = {0};
    /* Bit 0 = MSB of byte 0 */
    subghz_protocol_blocks_set_bit_array(true, arr, 0, sizeof(arr));
    TEST_ASSERT_EQUAL_HEX8(0x80, arr[0]);
    TEST_ASSERT_TRUE(subghz_protocol_blocks_get_bit_array(arr, 0));
}

void test_set_get_bit7_is_lsb(void)
{
    uint8_t arr[2] = {0};
    subghz_protocol_blocks_set_bit_array(true, arr, 7, sizeof(arr));
    TEST_ASSERT_EQUAL_HEX8(0x01, arr[0]);
    TEST_ASSERT_TRUE(subghz_protocol_blocks_get_bit_array(arr, 7));
}

void test_set_get_bit8_is_msb_of_byte1(void)
{
    uint8_t arr[2] = {0};
    subghz_protocol_blocks_set_bit_array(true, arr, 8, sizeof(arr));
    TEST_ASSERT_EQUAL_HEX8(0x00, arr[0]);
    TEST_ASSERT_EQUAL_HEX8(0x80, arr[1]);
    TEST_ASSERT_TRUE(subghz_protocol_blocks_get_bit_array(arr, 8));
}

void test_set_all_bits(void)
{
    uint8_t arr[2] = {0};
    for (size_t i = 0; i < 16; i++)
        subghz_protocol_blocks_set_bit_array(true, arr, i, sizeof(arr));
    TEST_ASSERT_EQUAL_HEX8(0xFF, arr[0]);
    TEST_ASSERT_EQUAL_HEX8(0xFF, arr[1]);
}

void test_clear_bit(void)
{
    uint8_t arr[1] = {0xFF};
    subghz_protocol_blocks_set_bit_array(false, arr, 3, sizeof(arr));
    TEST_ASSERT_EQUAL_HEX8(0xEF, arr[0]);  /* bit 3 = bit 4 from MSB → mask ~0x10 */
    TEST_ASSERT_FALSE(subghz_protocol_blocks_get_bit_array(arr, 3));
}

void test_out_of_bounds_set_ignored(void)
{
    uint8_t arr[1] = {0};
    subghz_protocol_blocks_set_bit_array(true, arr, 8, sizeof(arr));
    TEST_ASSERT_EQUAL_HEX8(0x00, arr[0]);  /* Should be unchanged */
}

void test_round_trip_byte_pattern(void)
{
    /* Write 0xA5 = 10100101 as individual bits */
    uint8_t arr[1] = {0};
    bool bits[] = {true, false, true, false, false, true, false, true};
    for (int i = 0; i < 8; i++)
        subghz_protocol_blocks_set_bit_array(bits[i], arr, (size_t)i, sizeof(arr));
    TEST_ASSERT_EQUAL_HEX8(0xA5, arr[0]);
    for (int i = 0; i < 8; i++)
        TEST_ASSERT_EQUAL(bits[i],
                           subghz_protocol_blocks_get_bit_array(arr, (size_t)i));
}

/*============================================================================*/
/* Tests: Upload generation (bit array → LevelDuration waveform)              */
/*============================================================================*/

void test_upload_single_bit_high(void)
{
    uint8_t data[1] = {0x80};  /* bit 0 = 1 */
    LevelDuration upload[4];
    size_t n = subghz_protocol_blocks_get_upload_from_bit_array(
        data, 1, upload, 4, 500, SubGhzProtocolBlockAlignBitLeft);
    TEST_ASSERT_EQUAL(1, n);
    TEST_ASSERT_TRUE(level_duration_get_level(upload[0]));
    TEST_ASSERT_EQUAL_UINT32(500, level_duration_get_duration(upload[0]));
}

void test_upload_alternating_bits(void)
{
    /* 0b1010 = 0xA0 in MSB-first byte layout */
    uint8_t data[1] = {0xA0};
    LevelDuration upload[8];
    size_t n = subghz_protocol_blocks_get_upload_from_bit_array(
        data, 4, upload, 8, 300, SubGhzProtocolBlockAlignBitLeft);
    /* 1-0-1-0 → four separate entries, each 300μs */
    TEST_ASSERT_EQUAL(4, n);
    TEST_ASSERT_TRUE(level_duration_get_level(upload[0]));
    TEST_ASSERT_EQUAL_UINT32(300, level_duration_get_duration(upload[0]));
    TEST_ASSERT_FALSE(level_duration_get_level(upload[1]));
    TEST_ASSERT_EQUAL_UINT32(300, level_duration_get_duration(upload[1]));
    TEST_ASSERT_TRUE(level_duration_get_level(upload[2]));
    TEST_ASSERT_EQUAL_UINT32(300, level_duration_get_duration(upload[2]));
    TEST_ASSERT_FALSE(level_duration_get_level(upload[3]));
    TEST_ASSERT_EQUAL_UINT32(300, level_duration_get_duration(upload[3]));
}

void test_upload_consecutive_bits_merged(void)
{
    /* 0b1100 = 0xC0 → two 1s merge into 600μs, two 0s merge into 600μs */
    uint8_t data[1] = {0xC0};
    LevelDuration upload[8];
    size_t n = subghz_protocol_blocks_get_upload_from_bit_array(
        data, 4, upload, 8, 300, SubGhzProtocolBlockAlignBitLeft);
    TEST_ASSERT_EQUAL(2, n);
    TEST_ASSERT_TRUE(level_duration_get_level(upload[0]));
    TEST_ASSERT_EQUAL_UINT32(600, level_duration_get_duration(upload[0]));
    TEST_ASSERT_FALSE(level_duration_get_level(upload[1]));
    TEST_ASSERT_EQUAL_UINT32(600, level_duration_get_duration(upload[1]));
}

void test_upload_all_same_bits(void)
{
    /* All 8 bits = 1 → single entry of 8×300 = 2400μs */
    uint8_t data[1] = {0xFF};
    LevelDuration upload[4];
    size_t n = subghz_protocol_blocks_get_upload_from_bit_array(
        data, 8, upload, 4, 300, SubGhzProtocolBlockAlignBitLeft);
    TEST_ASSERT_EQUAL(1, n);
    TEST_ASSERT_TRUE(level_duration_get_level(upload[0]));
    TEST_ASSERT_EQUAL_UINT32(2400, level_duration_get_duration(upload[0]));
}

void test_upload_right_align(void)
{
    /* 5 bits in 1 byte: data = 0x15 = 0b00010101
     * Right-aligned: bias = 8 - 5 = 3, reads from bit index 3
     * Bits 3-7: 1,0,1,0,1 → alternating, starting HIGH */
    uint8_t data[1] = {0x15};
    LevelDuration upload[8];
    size_t n = subghz_protocol_blocks_get_upload_from_bit_array(
        data, 5, upload, 8, 400, SubGhzProtocolBlockAlignBitRight);
    TEST_ASSERT_EQUAL(5, n);
    TEST_ASSERT_TRUE(level_duration_get_level(upload[0]));   /* bit 3 = 1 */
    TEST_ASSERT_FALSE(level_duration_get_level(upload[1]));  /* bit 4 = 0 */
    TEST_ASSERT_TRUE(level_duration_get_level(upload[2]));   /* bit 5 = 1 */
    TEST_ASSERT_FALSE(level_duration_get_level(upload[3]));  /* bit 6 = 0 */
    TEST_ASSERT_TRUE(level_duration_get_level(upload[4]));   /* bit 7 = 1 */
}

void test_upload_buffer_overflow_stops(void)
{
    /* More transitions than upload buffer can hold */
    uint8_t data[1] = {0xAA};  /* alternating = 8 transitions */
    LevelDuration upload[3];
    size_t n = subghz_protocol_blocks_get_upload_from_bit_array(
        data, 8, upload, 3, 100, SubGhzProtocolBlockAlignBitLeft);
    TEST_ASSERT_EQUAL(3, n);  /* Capped at buffer size */
}

void test_upload_multi_byte(void)
{
    /* 0xFF 0x00 → 8 highs merged + 8 lows merged = 2 entries */
    uint8_t data[2] = {0xFF, 0x00};
    LevelDuration upload[8];
    size_t n = subghz_protocol_blocks_get_upload_from_bit_array(
        data, 16, upload, 8, 100, SubGhzProtocolBlockAlignBitLeft);
    TEST_ASSERT_EQUAL(2, n);
    TEST_ASSERT_TRUE(level_duration_get_level(upload[0]));
    TEST_ASSERT_EQUAL_UINT32(800, level_duration_get_duration(upload[0]));
    TEST_ASSERT_FALSE(level_duration_get_level(upload[1]));
    TEST_ASSERT_EQUAL_UINT32(800, level_duration_get_duration(upload[1]));
}

/*============================================================================*/
/* Main                                                                       */
/*============================================================================*/

int main(void)
{
    UNITY_BEGIN();

    /* set/get bit array */
    RUN_TEST(test_set_get_bit0_is_msb);
    RUN_TEST(test_set_get_bit7_is_lsb);
    RUN_TEST(test_set_get_bit8_is_msb_of_byte1);
    RUN_TEST(test_set_all_bits);
    RUN_TEST(test_clear_bit);
    RUN_TEST(test_out_of_bounds_set_ignored);
    RUN_TEST(test_round_trip_byte_pattern);

    /* upload generation */
    RUN_TEST(test_upload_single_bit_high);
    RUN_TEST(test_upload_alternating_bits);
    RUN_TEST(test_upload_consecutive_bits_merged);
    RUN_TEST(test_upload_all_same_bits);
    RUN_TEST(test_upload_right_align);
    RUN_TEST(test_upload_buffer_overflow_stops);
    RUN_TEST(test_upload_multi_byte);

    return UNITY_END();
}
