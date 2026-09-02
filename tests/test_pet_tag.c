/* See COPYING.txt for license details. */

/*
 * test_pet_tag.c
 *
 * Unit tests for m1_pet_tag.c — the Pet Tag Scanner FDX-B decoder.
 *
 * FDX-B (ISO 11784/11785) transmits each multi-bit field LSB-first, so the
 * numeric value is recovered by weighting received bit i by 2^i.  These tests
 * build payloads according to that transmission convention and verify the
 * decoder reconstructs the country code, national code, animal flag and the
 * formatted 15-digit ID string.
 *
 * Build:
 *   cmake -B build-tests -S tests && cmake --build build-tests
 *   ctest --test-dir build-tests --output-on-failure
 */

#include <string.h>

#include "unity.h"
#include "lfrfid_bit_lib.h"
#include "m1_pet_tag.h"

void setUp(void) {}
void tearDown(void) {}

/* Write a field of `len` bits LSB-first starting at bit `pos` (received bit i
 * carries weight 2^i — the FDX-B on-air convention). */
static void set_field(uint8_t *data, size_t pos, uint8_t len, uint64_t value)
{
	for (uint8_t i = 0; i < len; i++)
		bl_set_bit(data, pos + i, (value >> i) & 1U);
}

/* Build an 11-byte FDX-B decoded payload from field values. */
static void build_fdxb(uint8_t data[11], uint16_t country, uint64_t national,
		       bool animal, bool block_status)
{
	memset(data, 0, 11);
	set_field(data, 0, 38, national);
	set_field(data, 38, 10, country);
	bl_set_bit(data, 48, block_status);
	bl_set_bit(data, 63, animal);
}

/* ===================================================================
 * Basic field extraction
 * =================================================================== */

void test_pet_decode_basic(void)
{
	uint8_t data[11];
	m1_pet_tag_info_t info;

	build_fdxb(data, 900, 1, true, false);
	TEST_ASSERT_TRUE(m1_pet_tag_decode_fdxb(data, &info));
	TEST_ASSERT_EQUAL_UINT16(900, info.country_code);
	TEST_ASSERT_EQUAL_UINT64(1, info.national_code);
	TEST_ASSERT_TRUE(info.animal);
	TEST_ASSERT_FALSE(info.block_status);
	TEST_ASSERT_EQUAL_STRING("900-000000000001", info.id_string);
}

/* Reference example from the FDX-B spec: country 999, national 1008. */
void test_pet_decode_spec_example(void)
{
	uint8_t data[11];
	m1_pet_tag_info_t info;

	build_fdxb(data, 999, 1008, true, false);
	TEST_ASSERT_TRUE(m1_pet_tag_decode_fdxb(data, &info));
	TEST_ASSERT_EQUAL_UINT16(999, info.country_code);
	TEST_ASSERT_EQUAL_UINT64(1008, info.national_code);
	TEST_ASSERT_EQUAL_STRING("999-000000001008", info.id_string);
}

/* Maximum 38-bit national code (2^38 - 1) — full 12-digit field. */
void test_pet_decode_national_max(void)
{
	uint8_t data[11];
	m1_pet_tag_info_t info;
	uint64_t max_national = ((uint64_t)1 << 38) - 1; /* 274877906943 */

	build_fdxb(data, 826, max_national, true, false);
	TEST_ASSERT_TRUE(m1_pet_tag_decode_fdxb(data, &info));
	TEST_ASSERT_EQUAL_UINT16(826, info.country_code);
	TEST_ASSERT_EQUAL_UINT64(max_national, info.national_code);
	TEST_ASSERT_EQUAL_STRING("826-274877906943", info.id_string);
}

/* Maximum 10-bit country code (1023). */
void test_pet_decode_country_max(void)
{
	uint8_t data[11];
	m1_pet_tag_info_t info;

	build_fdxb(data, 1023, 42, false, false);
	TEST_ASSERT_TRUE(m1_pet_tag_decode_fdxb(data, &info));
	TEST_ASSERT_EQUAL_UINT16(1023, info.country_code);
	TEST_ASSERT_EQUAL_UINT64(42, info.national_code);
}

/* Animal flag cleared → non-animal application. */
void test_pet_decode_animal_flag_clear(void)
{
	uint8_t data[11];
	m1_pet_tag_info_t info;

	build_fdxb(data, 900, 5, false, false);
	TEST_ASSERT_TRUE(m1_pet_tag_decode_fdxb(data, &info));
	TEST_ASSERT_FALSE(info.animal);
}

/* Data-block status flag. */
void test_pet_decode_block_status(void)
{
	uint8_t data[11];
	m1_pet_tag_info_t info;

	build_fdxb(data, 900, 5, true, true);
	TEST_ASSERT_TRUE(m1_pet_tag_decode_fdxb(data, &info));
	TEST_ASSERT_TRUE(info.block_status);
}

/* NULL arguments are rejected. */
void test_pet_decode_null_args(void)
{
	uint8_t data[11] = {0};
	m1_pet_tag_info_t info;

	TEST_ASSERT_FALSE(m1_pet_tag_decode_fdxb(NULL, &info));
	TEST_ASSERT_FALSE(m1_pet_tag_decode_fdxb(data, NULL));
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_pet_decode_basic);
	RUN_TEST(test_pet_decode_spec_example);
	RUN_TEST(test_pet_decode_national_max);
	RUN_TEST(test_pet_decode_country_max);
	RUN_TEST(test_pet_decode_animal_flag_clear);
	RUN_TEST(test_pet_decode_block_status);
	RUN_TEST(test_pet_decode_null_args);
	return UNITY_END();
}
