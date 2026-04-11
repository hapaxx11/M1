/* See COPYING.txt for license details. */

/*
 * test_subghz_block_decoder.c
 *
 * Unit tests for subghz_block_decoder.h — the Flipper-compatible bit
 * accumulation helpers used by ALL protocol decoders.
 *
 * Tests cover:
 *   - subghz_block_decoder_reset()
 *   - subghz_protocol_blocks_add_bit() — single-bit accumulation
 *   - subghz_protocol_blocks_add_to_128_bit() — 128-bit accumulation
 *   - subghz_protocol_blocks_get_hash_data() — dedup hash
 *   - Boundary conditions: 64-bit overflow, MSB propagation
 */

#include <string.h>
#include "unity.h"
#include "subghz_block_decoder.h"

void setUp(void) {}
void tearDown(void) {}

/*============================================================================*/
/* Tests: Reset                                                               */
/*============================================================================*/

void test_reset_clears_all_fields(void)
{
    SubGhzBlockDecoder dec = {.parser_step = 42, .te_last = 999,
                               .decode_data = 0xDEADBEEF, .decode_count_bit = 24};
    subghz_block_decoder_reset(&dec);
    TEST_ASSERT_EQUAL_UINT32(0, dec.parser_step);
    TEST_ASSERT_EQUAL_UINT32(0, dec.te_last);
    TEST_ASSERT_EQUAL_HEX64(0, dec.decode_data);
    TEST_ASSERT_EQUAL_UINT8(0, dec.decode_count_bit);
}

/*============================================================================*/
/* Tests: add_bit — single 64-bit accumulation                                */
/*============================================================================*/

void test_add_bit_single_one(void)
{
    SubGhzBlockDecoder dec = {0};
    subghz_protocol_blocks_add_bit(&dec, true);
    TEST_ASSERT_EQUAL_HEX64(1, dec.decode_data);
    TEST_ASSERT_EQUAL_UINT8(1, dec.decode_count_bit);
}

void test_add_bit_single_zero(void)
{
    SubGhzBlockDecoder dec = {0};
    subghz_protocol_blocks_add_bit(&dec, false);
    TEST_ASSERT_EQUAL_HEX64(0, dec.decode_data);
    TEST_ASSERT_EQUAL_UINT8(1, dec.decode_count_bit);
}

void test_add_bit_sequence_0b1010(void)
{
    SubGhzBlockDecoder dec = {0};
    subghz_protocol_blocks_add_bit(&dec, true);   /* 1 */
    subghz_protocol_blocks_add_bit(&dec, false);  /* 10 */
    subghz_protocol_blocks_add_bit(&dec, true);   /* 101 */
    subghz_protocol_blocks_add_bit(&dec, false);  /* 1010 */
    TEST_ASSERT_EQUAL_HEX64(0xA, dec.decode_data);
    TEST_ASSERT_EQUAL_UINT8(4, dec.decode_count_bit);
}

void test_add_bit_24bit_princeton_key(void)
{
    /* Simulate a 24-bit Princeton key: 0xABCDEF */
    SubGhzBlockDecoder dec = {0};
    uint32_t key = 0xABCDEF;
    for (int i = 23; i >= 0; i--)
        subghz_protocol_blocks_add_bit(&dec, (key >> i) & 1);
    TEST_ASSERT_EQUAL_HEX64(0xABCDEF, dec.decode_data);
    TEST_ASSERT_EQUAL_UINT8(24, dec.decode_count_bit);
}

void test_add_bit_fills_64_bits(void)
{
    /* Fill all 64 bits with 1s */
    SubGhzBlockDecoder dec = {0};
    for (int i = 0; i < 64; i++)
        subghz_protocol_blocks_add_bit(&dec, true);
    TEST_ASSERT_EQUAL_HEX64(0xFFFFFFFFFFFFFFFFULL, dec.decode_data);
    TEST_ASSERT_EQUAL_UINT8(64, dec.decode_count_bit);
}

void test_add_bit_overflow_64_wraps(void)
{
    /* After 64 bits, adding more shifts old MSBs out */
    SubGhzBlockDecoder dec = {0};
    /* Fill with 0xFF...FF */
    for (int i = 0; i < 64; i++)
        subghz_protocol_blocks_add_bit(&dec, true);
    /* Add a 0 — shifts everything left, MSB drops out */
    subghz_protocol_blocks_add_bit(&dec, false);
    TEST_ASSERT_EQUAL_HEX64(0xFFFFFFFFFFFFFFFEULL, dec.decode_data);
    TEST_ASSERT_EQUAL_UINT8(65, dec.decode_count_bit);
}

/*============================================================================*/
/* Tests: add_to_128_bit — dual accumulator                                   */
/*============================================================================*/

void test_128bit_basic_accumulation(void)
{
    SubGhzBlockDecoder dec = {0};
    uint64_t head = 0;
    /* Add 8 bits: 0b10110100 = 0xB4 */
    uint8_t bits[] = {1,0,1,1,0,1,0,0};
    for (int i = 0; i < 8; i++)
        subghz_protocol_blocks_add_to_128_bit(&dec, bits[i], &head);
    TEST_ASSERT_EQUAL_HEX64(0xB4, dec.decode_data);
    TEST_ASSERT_EQUAL_HEX64(0, head);
    TEST_ASSERT_EQUAL_UINT8(8, dec.decode_count_bit);
}

void test_128bit_overflow_into_head(void)
{
    SubGhzBlockDecoder dec = {0};
    uint64_t head = 0;
    /* Fill lower 64 bits with all 1s */
    for (int i = 0; i < 64; i++)
        subghz_protocol_blocks_add_to_128_bit(&dec, 1, &head);
    TEST_ASSERT_EQUAL_HEX64(0xFFFFFFFFFFFFFFFFULL, dec.decode_data);
    TEST_ASSERT_EQUAL_HEX64(0, head);

    /* Add one more 1 — MSB of decode_data (1) overflows into head */
    subghz_protocol_blocks_add_to_128_bit(&dec, 1, &head);
    TEST_ASSERT_EQUAL_HEX64(0xFFFFFFFFFFFFFFFFULL, dec.decode_data);
    TEST_ASSERT_EQUAL_HEX64(1, head);
}

void test_128bit_65th_bit_is_zero(void)
{
    SubGhzBlockDecoder dec = {0};
    uint64_t head = 0;
    /* Fill lower 64 bits with all 1s */
    for (int i = 0; i < 64; i++)
        subghz_protocol_blocks_add_to_128_bit(&dec, 1, &head);
    /* Add a 0 — MSB(1) overflows into head, new LSB = 0 */
    subghz_protocol_blocks_add_to_128_bit(&dec, 0, &head);
    TEST_ASSERT_EQUAL_HEX64(0xFFFFFFFFFFFFFFFEULL, dec.decode_data);
    TEST_ASSERT_EQUAL_HEX64(1, head);
}

void test_128bit_full_128_bits(void)
{
    SubGhzBlockDecoder dec = {0};
    uint64_t head = 0;
    /* Push 128 bits alternating 1/0 */
    for (int i = 0; i < 128; i++)
        subghz_protocol_blocks_add_to_128_bit(&dec, (uint8_t)(i & 1), &head);
    TEST_ASSERT_EQUAL_UINT8(128, dec.decode_count_bit);
    /* head and decode_data should both be 0x5555...5555 (alternating) */
    TEST_ASSERT_EQUAL_HEX64(0x5555555555555555ULL, head);
    TEST_ASSERT_EQUAL_HEX64(0x5555555555555555ULL, dec.decode_data);
}

/*============================================================================*/
/* Tests: get_hash_data                                                       */
/*============================================================================*/

void test_hash_zero_data(void)
{
    SubGhzBlockDecoder dec = {0};
    uint8_t hash = subghz_protocol_blocks_get_hash_data(&dec, sizeof(dec.decode_data));
    TEST_ASSERT_EQUAL_HEX8(0, hash);
}

void test_hash_nonzero_data(void)
{
    SubGhzBlockDecoder dec = {0};
    dec.decode_data = 0xABCDEF0123456789ULL;
    uint8_t hash = subghz_protocol_blocks_get_hash_data(&dec, sizeof(dec.decode_data));
    /* XOR of all 8 bytes */
    uint8_t expected = 0x89 ^ 0x67 ^ 0x45 ^ 0x23 ^ 0x01 ^ 0xEF ^ 0xCD ^ 0xAB;
    TEST_ASSERT_EQUAL_HEX8(expected, hash);
}

void test_hash_partial_len(void)
{
    SubGhzBlockDecoder dec = {0};
    dec.decode_data = 0x00000000000000FFULL;
    /* Hash of just first byte — on little-endian, byte[0] = 0xFF */
    uint8_t hash = subghz_protocol_blocks_get_hash_data(&dec, 1);
    TEST_ASSERT_EQUAL_HEX8(0xFF, hash);
}

void test_hash_same_data_same_hash(void)
{
    SubGhzBlockDecoder dec1 = {0};
    SubGhzBlockDecoder dec2 = {0};
    dec1.decode_data = 0xDEADBEEFCAFEBABEULL;
    dec2.decode_data = 0xDEADBEEFCAFEBABEULL;
    TEST_ASSERT_EQUAL_HEX8(
        subghz_protocol_blocks_get_hash_data(&dec1, 8),
        subghz_protocol_blocks_get_hash_data(&dec2, 8));
}

/*============================================================================*/
/* Main                                                                       */
/*============================================================================*/

int main(void)
{
    UNITY_BEGIN();

    /* Reset */
    RUN_TEST(test_reset_clears_all_fields);

    /* add_bit */
    RUN_TEST(test_add_bit_single_one);
    RUN_TEST(test_add_bit_single_zero);
    RUN_TEST(test_add_bit_sequence_0b1010);
    RUN_TEST(test_add_bit_24bit_princeton_key);
    RUN_TEST(test_add_bit_fills_64_bits);
    RUN_TEST(test_add_bit_overflow_64_wraps);

    /* add_to_128_bit */
    RUN_TEST(test_128bit_basic_accumulation);
    RUN_TEST(test_128bit_overflow_into_head);
    RUN_TEST(test_128bit_65th_bit_is_zero);
    RUN_TEST(test_128bit_full_128_bits);

    /* get_hash_data */
    RUN_TEST(test_hash_zero_data);
    RUN_TEST(test_hash_nonzero_data);
    RUN_TEST(test_hash_partial_len);
    RUN_TEST(test_hash_same_data_same_hash);

    return UNITY_END();
}
