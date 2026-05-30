/* See COPYING.txt for license details. */

/**
 * test_ir_signal_record.c
 *
 * Unit tests for the pure-logic helpers in ir_signal_record.h / ir_signal_record.c:
 *   ir_map_flipper_protocol()  — Flipper protocol name → IRMP protocol ID
 *   ir_is_ir_file()            — ".ir" extension check
 *   ir_path_append()           — path component append with buffer-size guard
 *   ir_path_go_up()            — navigate one level up in a path
 *   ir_str_contains_icase()    — case-insensitive substring search
 *
 * Build:
 *   cmake -B build-tests -S tests && cmake --build build-tests
 *   ctest --test-dir build-tests --output-on-failure
 */

#include "unity.h"
#include "ir_signal_record.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

void setUp(void)    {}
void tearDown(void) {}

/* =========================================================================
 * ir_map_flipper_protocol() — Flipper protocol name → IRMP protocol ID
 * =========================================================================*/

void test_protocol_nec(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_NEC_PROTOCOL, ir_map_flipper_protocol("NEC"));
}

void test_protocol_necext(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_NEC_PROTOCOL, ir_map_flipper_protocol("NECext"));
}

void test_protocol_samsung48(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_SAMSUNG48_PROTOCOL, ir_map_flipper_protocol("Samsung48"));
}

void test_protocol_samsung32(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_SAMSUNG32_PROTOCOL, ir_map_flipper_protocol("Samsung32"));
	TEST_ASSERT_EQUAL_UINT8(IRMP_SAMSUNG32_PROTOCOL, ir_map_flipper_protocol("Samsung"));
}

void test_protocol_rc5(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_RC5_PROTOCOL, ir_map_flipper_protocol("RC5"));
	TEST_ASSERT_EQUAL_UINT8(IRMP_RC5_PROTOCOL, ir_map_flipper_protocol("RC5X"));
}

void test_protocol_rc6(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_RC6_PROTOCOL, ir_map_flipper_protocol("RC6"));
}

void test_protocol_sony_variants(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_SIRCS_PROTOCOL, ir_map_flipper_protocol("Sony12"));
	TEST_ASSERT_EQUAL_UINT8(IRMP_SIRCS_PROTOCOL, ir_map_flipper_protocol("Sony15"));
	TEST_ASSERT_EQUAL_UINT8(IRMP_SIRCS_PROTOCOL, ir_map_flipper_protocol("Sony20"));
	TEST_ASSERT_EQUAL_UINT8(IRMP_SIRCS_PROTOCOL, ir_map_flipper_protocol("SIRC"));
	TEST_ASSERT_EQUAL_UINT8(IRMP_SIRCS_PROTOCOL, ir_map_flipper_protocol("SIRC15"));
	TEST_ASSERT_EQUAL_UINT8(IRMP_SIRCS_PROTOCOL, ir_map_flipper_protocol("SIRC20"));
}

void test_protocol_kaseikyo(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_KASEIKYO_PROTOCOL, ir_map_flipper_protocol("Kaseikyo"));
	TEST_ASSERT_EQUAL_UINT8(IRMP_KASEIKYO_PROTOCOL, ir_map_flipper_protocol("Panasonic"));
}

void test_protocol_nec42(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_NEC42_PROTOCOL, ir_map_flipper_protocol("NEC42"));
	TEST_ASSERT_EQUAL_UINT8(IRMP_NEC42_PROTOCOL, ir_map_flipper_protocol("NEC42ext"));
}

void test_protocol_denon(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_DENON_PROTOCOL, ir_map_flipper_protocol("Denon"));
	TEST_ASSERT_EQUAL_UINT8(IRMP_DENON_PROTOCOL, ir_map_flipper_protocol("Sharp"));
}

void test_protocol_jvc(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_JVC_PROTOCOL, ir_map_flipper_protocol("JVC"));
}

void test_protocol_lg(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_LGAIR_PROTOCOL, ir_map_flipper_protocol("LG"));
}

void test_protocol_pioneer_maps_to_nec(void)
{
	/* Pioneer uses NEC encoding in Flipper's database */
	TEST_ASSERT_EQUAL_UINT8(IRMP_NEC_PROTOCOL, ir_map_flipper_protocol("Pioneer"));
}

void test_protocol_apple(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_APPLE_PROTOCOL, ir_map_flipper_protocol("Apple"));
}

void test_protocol_bose(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_BOSE_PROTOCOL, ir_map_flipper_protocol("Bose"));
}

void test_protocol_nokia(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_NOKIA_PROTOCOL, ir_map_flipper_protocol("Nokia"));
}

void test_protocol_rca(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_RCCAR_PROTOCOL, ir_map_flipper_protocol("RCA"));
}

void test_protocol_rcmm(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_RCMM32_PROTOCOL, ir_map_flipper_protocol("RCMM"));
}

void test_protocol_unknown_returns_zero(void)
{
	TEST_ASSERT_EQUAL_UINT8(0, ir_map_flipper_protocol("UnknownProto"));
	TEST_ASSERT_EQUAL_UINT8(0, ir_map_flipper_protocol(""));
}

void test_protocol_null_returns_zero(void)
{
	TEST_ASSERT_EQUAL_UINT8(0, ir_map_flipper_protocol(NULL));
}

void test_protocol_case_sensitive(void)
{
	/* Protocol names are case-sensitive in Flipper files */
	TEST_ASSERT_EQUAL_UINT8(0, ir_map_flipper_protocol("nec"));
	TEST_ASSERT_EQUAL_UINT8(0, ir_map_flipper_protocol("rc5"));
	TEST_ASSERT_EQUAL_UINT8(0, ir_map_flipper_protocol("SAMSUNG"));
}

/* =========================================================================
 * ir_is_ir_file() — ".ir" extension check
 * =========================================================================*/

void test_is_ir_file_typical(void)
{
	TEST_ASSERT_TRUE(ir_is_ir_file("Samsung_TV.ir"));
	TEST_ASSERT_TRUE(ir_is_ir_file("tv.ir"));
	TEST_ASSERT_TRUE(ir_is_ir_file("0:/IR/TV/Samsung_K6500.ir"));
}

void test_is_ir_file_no_extension(void)
{
	TEST_ASSERT_FALSE(ir_is_ir_file("Samsung_TV"));
}

void test_is_ir_file_wrong_extension(void)
{
	TEST_ASSERT_FALSE(ir_is_ir_file("Samsung_TV.sub"));
	TEST_ASSERT_FALSE(ir_is_ir_file("Samsung_TV.txt"));
	TEST_ASSERT_FALSE(ir_is_ir_file("Samsung_TV.IR")); /* case-sensitive */
}

void test_is_ir_file_short_name(void)
{
	/* fname must be at least 4 chars to contain ".ir\0" */
	TEST_ASSERT_FALSE(ir_is_ir_file(".ir"));  /* len == 3, not >= 4 */
	TEST_ASSERT_TRUE(ir_is_ir_file("a.ir"));  /* len == 4, ok */
}

void test_is_ir_file_null(void)
{
	TEST_ASSERT_FALSE(ir_is_ir_file(NULL));
}

/* =========================================================================
 * ir_path_append() — path component append with buffer-size guard
 * =========================================================================*/

void test_path_append_basic(void)
{
	char buf[64];
	strcpy(buf, "0:/IR");
	ir_path_append(buf, "TV", sizeof(buf));
	TEST_ASSERT_EQUAL_STRING("0:/IR/TV", buf);
}

void test_path_append_trailing_slash(void)
{
	char buf[64];
	strcpy(buf, "0:/IR/");
	ir_path_append(buf, "TV", sizeof(buf));
	TEST_ASSERT_EQUAL_STRING("0:/IR/TV", buf);
}

void test_path_append_empty_base(void)
{
	char buf[64];
	buf[0] = '\0';
	ir_path_append(buf, "0:/IR", sizeof(buf));
	TEST_ASSERT_EQUAL_STRING("0:/IR", buf);
}

void test_path_append_null_safe(void)
{
	char buf[64];
	strcpy(buf, "0:/IR");
	ir_path_append(NULL, "TV", sizeof(buf));  /* must not crash */
	ir_path_append(buf, NULL, sizeof(buf));   /* must not crash */
	ir_path_append(buf, "TV", 0);             /* must not crash */
	/* buf unchanged when item is NULL */
	TEST_ASSERT_EQUAL_STRING("0:/IR", buf);
}

void test_path_append_buffer_boundary(void)
{
	/* Buffer exactly big enough for "ab/cd\0" (7 bytes) */
	char buf[7];
	strcpy(buf, "ab");
	ir_path_append(buf, "cd", sizeof(buf));
	TEST_ASSERT_EQUAL_STRING("ab/cd", buf);
	TEST_ASSERT_EQUAL_UINT8('\0', buf[6]);
}

void test_path_append_does_not_overflow(void)
{
	char buf[8];
	memset(buf, 0xAB, sizeof(buf));
	strcpy(buf, "0:/IR");
	/* "Samsung_TV_UHD" won't fully fit — must not go past buf[7] */
	ir_path_append(buf, "Samsung_TV_UHD", sizeof(buf));
	TEST_ASSERT_EQUAL_UINT8('\0', buf[7]);
}

/* =========================================================================
 * ir_path_go_up() — navigate one level up in a path
 * =========================================================================*/

void test_path_go_up_simple(void)
{
	char buf[64];
	strcpy(buf, "0:/IR/TV");
	ir_path_go_up(buf);
	TEST_ASSERT_EQUAL_STRING("0:/IR", buf);
}

void test_path_go_up_trailing_slash(void)
{
	char buf[64];
	strcpy(buf, "0:/IR/TV/");
	ir_path_go_up(buf);
	TEST_ASSERT_EQUAL_STRING("0:/IR", buf);
}

void test_path_go_up_three_levels(void)
{
	char buf[64];
	strcpy(buf, "0:/IR/TV/Samsung");
	ir_path_go_up(buf);
	TEST_ASSERT_EQUAL_STRING("0:/IR/TV", buf);
	ir_path_go_up(buf);
	TEST_ASSERT_EQUAL_STRING("0:/IR", buf);
}

void test_path_go_up_no_slash_clears(void)
{
	char buf[64];
	strcpy(buf, "rootonly");
	ir_path_go_up(buf);
	TEST_ASSERT_EQUAL_STRING("", buf);
}

void test_path_go_up_empty(void)
{
	char buf[64];
	buf[0] = '\0';
	ir_path_go_up(buf);
	TEST_ASSERT_EQUAL_STRING("", buf);
}

void test_path_go_up_null_safe(void)
{
	ir_path_go_up(NULL);  /* must not crash */
}

/* =========================================================================
 * ir_str_contains_icase() — case-insensitive substring search
 * (full coverage is in test_ir_search.c; these cases cover the API contract)
 * =========================================================================*/

void test_str_icase_basic_match(void)
{
	TEST_ASSERT_TRUE(ir_str_contains_icase("Sony_KD75.ir", "Sony"));
}

void test_str_icase_case_fold(void)
{
	TEST_ASSERT_TRUE(ir_str_contains_icase("SAMSUNG_TV.ir", "samsung"));
}

void test_str_icase_no_match(void)
{
	TEST_ASSERT_FALSE(ir_str_contains_icase("LG_Soundbar.ir", "Sony"));
}

void test_str_icase_null_needle(void)
{
	TEST_ASSERT_TRUE(ir_str_contains_icase("anything", NULL));
}

void test_str_icase_null_haystack(void)
{
	TEST_ASSERT_FALSE(ir_str_contains_icase(NULL, "Sony"));
}

void test_str_icase_both_null(void)
{
	TEST_ASSERT_TRUE(ir_str_contains_icase(NULL, NULL));
}

/* =========================================================================
 * Runner
 * =========================================================================*/

int main(void)
{
	UNITY_BEGIN();

	/* ir_map_flipper_protocol */
	RUN_TEST(test_protocol_nec);
	RUN_TEST(test_protocol_necext);
	RUN_TEST(test_protocol_samsung48);
	RUN_TEST(test_protocol_samsung32);
	RUN_TEST(test_protocol_rc5);
	RUN_TEST(test_protocol_rc6);
	RUN_TEST(test_protocol_sony_variants);
	RUN_TEST(test_protocol_kaseikyo);
	RUN_TEST(test_protocol_nec42);
	RUN_TEST(test_protocol_denon);
	RUN_TEST(test_protocol_jvc);
	RUN_TEST(test_protocol_lg);
	RUN_TEST(test_protocol_pioneer_maps_to_nec);
	RUN_TEST(test_protocol_apple);
	RUN_TEST(test_protocol_bose);
	RUN_TEST(test_protocol_nokia);
	RUN_TEST(test_protocol_rca);
	RUN_TEST(test_protocol_rcmm);
	RUN_TEST(test_protocol_unknown_returns_zero);
	RUN_TEST(test_protocol_null_returns_zero);
	RUN_TEST(test_protocol_case_sensitive);

	/* ir_is_ir_file */
	RUN_TEST(test_is_ir_file_typical);
	RUN_TEST(test_is_ir_file_no_extension);
	RUN_TEST(test_is_ir_file_wrong_extension);
	RUN_TEST(test_is_ir_file_short_name);
	RUN_TEST(test_is_ir_file_null);

	/* ir_path_append */
	RUN_TEST(test_path_append_basic);
	RUN_TEST(test_path_append_trailing_slash);
	RUN_TEST(test_path_append_empty_base);
	RUN_TEST(test_path_append_null_safe);
	RUN_TEST(test_path_append_buffer_boundary);
	RUN_TEST(test_path_append_does_not_overflow);

	/* ir_path_go_up */
	RUN_TEST(test_path_go_up_simple);
	RUN_TEST(test_path_go_up_trailing_slash);
	RUN_TEST(test_path_go_up_three_levels);
	RUN_TEST(test_path_go_up_no_slash_clears);
	RUN_TEST(test_path_go_up_empty);
	RUN_TEST(test_path_go_up_null_safe);

	/* ir_str_contains_icase */
	RUN_TEST(test_str_icase_basic_match);
	RUN_TEST(test_str_icase_case_fold);
	RUN_TEST(test_str_icase_no_match);
	RUN_TEST(test_str_icase_null_needle);
	RUN_TEST(test_str_icase_null_haystack);
	RUN_TEST(test_str_icase_both_null);

	return UNITY_END();
}
