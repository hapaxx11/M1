/* See COPYING.txt for license details. */

/*
 * test_flipper_file.c
 *
 * Regression tests for flipper_file.c utility functions — hex byte
 * parsing and int32 array parsing used by all Flipper file importers.
 *
 * Bug context:
 *   These parsing functions are used when importing Flipper .sub, .rfid,
 *   .nfc, and .ir files from SD card.  Parsing bugs caused:
 *   1. Raw Sub-GHz data lines ("RAW_Data: 350 -200 500 ...") to be
 *      misread, producing garbled waveforms during replay.
 *   2. Hex key fields ("Key: 07 00 AB CD") to be incorrectly decoded,
 *      causing protocol identification failures.
 *   3. Edge cases like single hex digits, leading/trailing whitespace,
 *      and negative values in raw data lines.
 *
 * Build:
 *   cmake -B build-tests -S tests && cmake --build build-tests
 *   ctest --test-dir build-tests --output-on-failure
 */

#include "unity.h"
#include "flipper_file.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/* ===================================================================
 * ff_parse_hex_bytes — parses "07 00 AB CD" → {0x07, 0x00, 0xAB, 0xCD}
 * =================================================================== */

void test_hex_bytes_standard(void)
{
	uint8_t out[8];
	uint8_t count = ff_parse_hex_bytes("07 00 AB CD", out, sizeof(out));
	TEST_ASSERT_EQUAL_UINT8(4, count);
	TEST_ASSERT_EQUAL_UINT8(0x07, out[0]);
	TEST_ASSERT_EQUAL_UINT8(0x00, out[1]);
	TEST_ASSERT_EQUAL_UINT8(0xAB, out[2]);
	TEST_ASSERT_EQUAL_UINT8(0xCD, out[3]);
}

void test_hex_bytes_lowercase(void)
{
	uint8_t out[4];
	uint8_t count = ff_parse_hex_bytes("de ad be ef", out, sizeof(out));
	TEST_ASSERT_EQUAL_UINT8(4, count);
	TEST_ASSERT_EQUAL_UINT8(0xDE, out[0]);
	TEST_ASSERT_EQUAL_UINT8(0xAD, out[1]);
	TEST_ASSERT_EQUAL_UINT8(0xBE, out[2]);
	TEST_ASSERT_EQUAL_UINT8(0xEF, out[3]);
}

void test_hex_bytes_no_spaces(void)
{
	/* Hex bytes can be packed without spaces */
	uint8_t out[4];
	uint8_t count = ff_parse_hex_bytes("DEADBEEF", out, sizeof(out));
	TEST_ASSERT_EQUAL_UINT8(4, count);
	TEST_ASSERT_EQUAL_UINT8(0xDE, out[0]);
	TEST_ASSERT_EQUAL_UINT8(0xAD, out[1]);
	TEST_ASSERT_EQUAL_UINT8(0xBE, out[2]);
	TEST_ASSERT_EQUAL_UINT8(0xEF, out[3]);
}

void test_hex_bytes_leading_trailing_whitespace(void)
{
	uint8_t out[4];
	uint8_t count = ff_parse_hex_bytes("  FF 01  ", out, sizeof(out));
	TEST_ASSERT_EQUAL_UINT8(2, count);
	TEST_ASSERT_EQUAL_UINT8(0xFF, out[0]);
	TEST_ASSERT_EQUAL_UINT8(0x01, out[1]);
}

void test_hex_bytes_single_digit(void)
{
	/* Single hex digit — should parse as a nibble */
	uint8_t out[4];
	uint8_t count = ff_parse_hex_bytes("F", out, sizeof(out));
	TEST_ASSERT_EQUAL_UINT8(1, count);
	TEST_ASSERT_EQUAL_UINT8(0x0F, out[0]);
}

void test_hex_bytes_max_len_limit(void)
{
	/* Only parse up to max_len bytes */
	uint8_t out[2];
	uint8_t count = ff_parse_hex_bytes("01 02 03 04", out, 2);
	TEST_ASSERT_EQUAL_UINT8(2, count);
	TEST_ASSERT_EQUAL_UINT8(0x01, out[0]);
	TEST_ASSERT_EQUAL_UINT8(0x02, out[1]);
}

void test_hex_bytes_empty(void)
{
	uint8_t out[4];
	uint8_t count = ff_parse_hex_bytes("", out, sizeof(out));
	TEST_ASSERT_EQUAL_UINT8(0, count);
}

void test_hex_bytes_null_inputs(void)
{
	uint8_t out[4];
	TEST_ASSERT_EQUAL_UINT8(0, ff_parse_hex_bytes(NULL, out, sizeof(out)));
	TEST_ASSERT_EQUAL_UINT8(0, ff_parse_hex_bytes("FF", NULL, 4));
	TEST_ASSERT_EQUAL_UINT8(0, ff_parse_hex_bytes("FF", out, 0));
}

void test_hex_bytes_flipper_nfc_uid(void)
{
	/* Real NFC UID from a Flipper .nfc file */
	uint8_t out[8];
	uint8_t count = ff_parse_hex_bytes("04 68 95 8A 3C 64 80", out, sizeof(out));
	TEST_ASSERT_EQUAL_UINT8(7, count);
	TEST_ASSERT_EQUAL_UINT8(0x04, out[0]);
	TEST_ASSERT_EQUAL_UINT8(0x68, out[1]);
	TEST_ASSERT_EQUAL_UINT8(0x95, out[2]);
	TEST_ASSERT_EQUAL_UINT8(0x8A, out[3]);
	TEST_ASSERT_EQUAL_UINT8(0x3C, out[4]);
	TEST_ASSERT_EQUAL_UINT8(0x64, out[5]);
	TEST_ASSERT_EQUAL_UINT8(0x80, out[6]);
}

void test_hex_bytes_tab_separated(void)
{
	uint8_t out[4];
	uint8_t count = ff_parse_hex_bytes("AA\tBB\tCC", out, sizeof(out));
	TEST_ASSERT_EQUAL_UINT8(3, count);
	TEST_ASSERT_EQUAL_UINT8(0xAA, out[0]);
	TEST_ASSERT_EQUAL_UINT8(0xBB, out[1]);
	TEST_ASSERT_EQUAL_UINT8(0xCC, out[2]);
}

/* ===================================================================
 * ff_parse_int32_array — parses "350 -200 500 -180" → {350, -200, 500, -180}
 *
 * This is critical for Sub-GHz RAW_Data lines in .sub files.
 * Positive = mark duration (µs), negative = space duration (µs).
 * The overcounting bug in Read Raw sample tracking was related to
 * how these values were accumulated.
 * =================================================================== */

void test_int32_array_standard(void)
{
	int32_t out[8];
	uint16_t count = ff_parse_int32_array("350 -200 500 -180", out, 8);
	TEST_ASSERT_EQUAL_UINT16(4, count);
	TEST_ASSERT_EQUAL_INT32(350, out[0]);
	TEST_ASSERT_EQUAL_INT32(-200, out[1]);
	TEST_ASSERT_EQUAL_INT32(500, out[2]);
	TEST_ASSERT_EQUAL_INT32(-180, out[3]);
}

void test_int32_array_single_value(void)
{
	int32_t out[4];
	uint16_t count = ff_parse_int32_array("42", out, 4);
	TEST_ASSERT_EQUAL_UINT16(1, count);
	TEST_ASSERT_EQUAL_INT32(42, out[0]);
}

void test_int32_array_negative_only(void)
{
	int32_t out[4];
	uint16_t count = ff_parse_int32_array("-100 -200 -300", out, 4);
	TEST_ASSERT_EQUAL_UINT16(3, count);
	TEST_ASSERT_EQUAL_INT32(-100, out[0]);
	TEST_ASSERT_EQUAL_INT32(-200, out[1]);
	TEST_ASSERT_EQUAL_INT32(-300, out[2]);
}

void test_int32_array_max_count_limit(void)
{
	int32_t out[2];
	uint16_t count = ff_parse_int32_array("1 2 3 4 5", out, 2);
	TEST_ASSERT_EQUAL_UINT16(2, count);
	TEST_ASSERT_EQUAL_INT32(1, out[0]);
	TEST_ASSERT_EQUAL_INT32(2, out[1]);
}

void test_int32_array_extra_whitespace(void)
{
	int32_t out[4];
	uint16_t count = ff_parse_int32_array("  100   -200   300  ", out, 4);
	TEST_ASSERT_EQUAL_UINT16(3, count);
	TEST_ASSERT_EQUAL_INT32(100, out[0]);
	TEST_ASSERT_EQUAL_INT32(-200, out[1]);
	TEST_ASSERT_EQUAL_INT32(300, out[2]);
}

void test_int32_array_empty(void)
{
	int32_t out[4];
	uint16_t count = ff_parse_int32_array("", out, 4);
	TEST_ASSERT_EQUAL_UINT16(0, count);
}

void test_int32_array_null_inputs(void)
{
	int32_t out[4];
	TEST_ASSERT_EQUAL_UINT16(0, ff_parse_int32_array(NULL, out, 4));
	TEST_ASSERT_EQUAL_UINT16(0, ff_parse_int32_array("1 2", NULL, 4));
	TEST_ASSERT_EQUAL_UINT16(0, ff_parse_int32_array("1 2", out, 0));
}

void test_int32_array_real_subghz_raw_data(void)
{
	/* Realistic RAW_Data line from a Flipper .sub file */
	const char *raw_line = "361 -107 68 -100 70 -193 64 -388 66";
	int32_t out[16];
	uint16_t count = ff_parse_int32_array(raw_line, out, 16);
	TEST_ASSERT_EQUAL_UINT16(9, count);
	TEST_ASSERT_EQUAL_INT32(361, out[0]);
	TEST_ASSERT_EQUAL_INT32(-107, out[1]);
	TEST_ASSERT_EQUAL_INT32(68, out[2]);
	TEST_ASSERT_EQUAL_INT32(-100, out[3]);
	TEST_ASSERT_EQUAL_INT32(70, out[4]);
	TEST_ASSERT_EQUAL_INT32(-193, out[5]);
	TEST_ASSERT_EQUAL_INT32(64, out[6]);
	TEST_ASSERT_EQUAL_INT32(-388, out[7]);
	TEST_ASSERT_EQUAL_INT32(66, out[8]);
}

void test_int32_array_large_values(void)
{
	/* Sub-GHz raw data can have large gap values */
	int32_t out[4];
	uint16_t count = ff_parse_int32_array("30000 -65535 100", out, 4);
	TEST_ASSERT_EQUAL_UINT16(3, count);
	TEST_ASSERT_EQUAL_INT32(30000, out[0]);
	TEST_ASSERT_EQUAL_INT32(-65535, out[1]);
	TEST_ASSERT_EQUAL_INT32(100, out[2]);
}

void test_int32_array_tab_separated(void)
{
	int32_t out[4];
	uint16_t count = ff_parse_int32_array("10\t-20\t30", out, 4);
	TEST_ASSERT_EQUAL_UINT16(3, count);
	TEST_ASSERT_EQUAL_INT32(10, out[0]);
	TEST_ASSERT_EQUAL_INT32(-20, out[1]);
	TEST_ASSERT_EQUAL_INT32(30, out[2]);
}

/* ===================================================================
 * ff_parse_kv — key-value line parsing
 *
 * Tests the core parser used by all Flipper file importers.
 * The bug was that whitespace handling around the ": " separator
 * needed to be robust for various file generators.
 * =================================================================== */

void test_parse_kv_standard(void)
{
	flipper_file_t ctx;
	memset(&ctx, 0, sizeof(ctx));
	strcpy(ctx.line, "Frequency: 433920000");

	TEST_ASSERT_TRUE(ff_parse_kv(&ctx));
	TEST_ASSERT_EQUAL_STRING("Frequency", ctx.key);
	TEST_ASSERT_EQUAL_STRING("433920000", ctx.value);
}

void test_parse_kv_with_spaces_in_value(void)
{
	flipper_file_t ctx;
	memset(&ctx, 0, sizeof(ctx));
	strcpy(ctx.line, "Preset: FuriHalSubGhzPresetOok650Async");

	TEST_ASSERT_TRUE(ff_parse_kv(&ctx));
	TEST_ASSERT_EQUAL_STRING("Preset", ctx.key);
	TEST_ASSERT_EQUAL_STRING("FuriHalSubGhzPresetOok650Async", ctx.value);
}

void test_parse_kv_key_with_leading_whitespace(void)
{
	flipper_file_t ctx;
	memset(&ctx, 0, sizeof(ctx));
	strcpy(ctx.line, "  Protocol: Princeton");

	TEST_ASSERT_TRUE(ff_parse_kv(&ctx));
	TEST_ASSERT_EQUAL_STRING("Protocol", ctx.key);
	TEST_ASSERT_EQUAL_STRING("Princeton", ctx.value);
}

void test_parse_kv_no_separator(void)
{
	flipper_file_t ctx;
	memset(&ctx, 0, sizeof(ctx));
	strcpy(ctx.line, "This is not a key-value line");

	TEST_ASSERT_FALSE(ff_parse_kv(&ctx));
}

void test_parse_kv_colon_without_space(void)
{
	/* ": " separator requires space after colon */
	flipper_file_t ctx;
	memset(&ctx, 0, sizeof(ctx));
	strcpy(ctx.line, "Key:value");

	TEST_ASSERT_FALSE(ff_parse_kv(&ctx));
}

void test_parse_kv_hex_value(void)
{
	flipper_file_t ctx;
	memset(&ctx, 0, sizeof(ctx));
	strcpy(ctx.line, "Key: 07 00 AB CD EF");

	TEST_ASSERT_TRUE(ff_parse_kv(&ctx));
	TEST_ASSERT_EQUAL_STRING("Key", ctx.key);
	TEST_ASSERT_EQUAL_STRING("07 00 AB CD EF", ctx.value);
}

void test_parse_kv_null_ctx(void)
{
	TEST_ASSERT_FALSE(ff_parse_kv(NULL));
}

/* ===================================================================
 * Runner
 * =================================================================== */

int main(void)
{
	UNITY_BEGIN();

	/* ff_parse_hex_bytes */
	RUN_TEST(test_hex_bytes_standard);
	RUN_TEST(test_hex_bytes_lowercase);
	RUN_TEST(test_hex_bytes_no_spaces);
	RUN_TEST(test_hex_bytes_leading_trailing_whitespace);
	RUN_TEST(test_hex_bytes_single_digit);
	RUN_TEST(test_hex_bytes_max_len_limit);
	RUN_TEST(test_hex_bytes_empty);
	RUN_TEST(test_hex_bytes_null_inputs);
	RUN_TEST(test_hex_bytes_flipper_nfc_uid);
	RUN_TEST(test_hex_bytes_tab_separated);

	/* ff_parse_int32_array */
	RUN_TEST(test_int32_array_standard);
	RUN_TEST(test_int32_array_single_value);
	RUN_TEST(test_int32_array_negative_only);
	RUN_TEST(test_int32_array_max_count_limit);
	RUN_TEST(test_int32_array_extra_whitespace);
	RUN_TEST(test_int32_array_empty);
	RUN_TEST(test_int32_array_null_inputs);
	RUN_TEST(test_int32_array_real_subghz_raw_data);
	RUN_TEST(test_int32_array_large_values);
	RUN_TEST(test_int32_array_tab_separated);

	/* ff_parse_kv */
	RUN_TEST(test_parse_kv_standard);
	RUN_TEST(test_parse_kv_with_spaces_in_value);
	RUN_TEST(test_parse_kv_key_with_leading_whitespace);
	RUN_TEST(test_parse_kv_no_separator);
	RUN_TEST(test_parse_kv_colon_without_space);
	RUN_TEST(test_parse_kv_hex_value);
	RUN_TEST(test_parse_kv_null_ctx);

	return UNITY_END();
}
