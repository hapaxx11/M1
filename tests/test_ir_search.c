/* See COPYING.txt for license details. */

/*
 * test_ir_search.c
 *
 * Unit tests for str_contains_icase() — case-insensitive substring matcher
 * used by the IR Universal Remote IRDB search feature.
 *
 * Build:
 *   cmake -B build-tests -S tests && cmake --build build-tests
 *   ctest --test-dir build-tests --output-on-failure
 */

#include "unity.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* Function under test (implemented in stubs/ir_search_impl.c) */
bool str_contains_icase(const char *haystack, const char *needle);

void setUp(void) {}
void tearDown(void) {}

/* ===================================================================
 * Basic matching
 * =================================================================== */

void test_exact_match(void)
{
	TEST_ASSERT_TRUE(str_contains_icase("Samsung_TV.ir", "Samsung_TV.ir"));
}

void test_substring_at_start(void)
{
	TEST_ASSERT_TRUE(str_contains_icase("Samsung_TV.ir", "Sam"));
}

void test_substring_in_middle(void)
{
	TEST_ASSERT_TRUE(str_contains_icase("Samsung_TV.ir", "sung"));
}

void test_substring_at_end(void)
{
	TEST_ASSERT_TRUE(str_contains_icase("Samsung_TV.ir", ".ir"));
}

void test_no_match(void)
{
	TEST_ASSERT_FALSE(str_contains_icase("Samsung_TV.ir", "Sony"));
}

void test_needle_longer_than_haystack(void)
{
	TEST_ASSERT_FALSE(str_contains_icase("TV.ir", "Samsung_TV.ir"));
}

/* ===================================================================
 * Case insensitivity
 * =================================================================== */

void test_case_lower_query_upper_filename(void)
{
	TEST_ASSERT_TRUE(str_contains_icase("SAMSUNG_TV.ir", "samsung"));
}

void test_case_upper_query_lower_filename(void)
{
	TEST_ASSERT_TRUE(str_contains_icase("samsung_tv.ir", "SAMSUNG"));
}

void test_case_mixed(void)
{
	TEST_ASSERT_TRUE(str_contains_icase("Pioneer_AV.ir", "pIoNeEr"));
}

/* ===================================================================
 * Edge cases
 * =================================================================== */

void test_empty_needle_matches_everything(void)
{
	TEST_ASSERT_TRUE(str_contains_icase("Samsung_TV.ir", ""));
}

void test_null_needle_matches_everything(void)
{
	TEST_ASSERT_TRUE(str_contains_icase("Samsung_TV.ir", NULL));
}

void test_null_haystack_no_match(void)
{
	TEST_ASSERT_FALSE(str_contains_icase(NULL, "Samsung"));
}

void test_null_haystack_empty_needle_matches(void)
{
	/* Empty needle always matches, even with NULL haystack */
	TEST_ASSERT_TRUE(str_contains_icase(NULL, NULL));
	TEST_ASSERT_TRUE(str_contains_icase(NULL, ""));
}

void test_single_char_match(void)
{
	TEST_ASSERT_TRUE(str_contains_icase("a", "a"));
}

void test_single_char_no_match(void)
{
	TEST_ASSERT_FALSE(str_contains_icase("a", "b"));
}

void test_needle_equals_haystack_length(void)
{
	TEST_ASSERT_TRUE(str_contains_icase("abc", "abc"));
	TEST_ASSERT_FALSE(str_contains_icase("abc", "xyz"));
}

/* ===================================================================
 * Real IRDB filename scenarios
 * =================================================================== */

void test_irdb_brand_search(void)
{
	/* Typical IRDB filename: <Brand>_<Model>.ir */
	TEST_ASSERT_TRUE(str_contains_icase("Sony_KD75.ir", "sony"));
	TEST_ASSERT_TRUE(str_contains_icase("Sony_KD75.ir", "kd75"));
	TEST_ASSERT_FALSE(str_contains_icase("Sony_KD75.ir", "samsung"));
}

void test_irdb_partial_model(void)
{
	TEST_ASSERT_TRUE(str_contains_icase("Panasonic_TX55.ir", "tx"));
	TEST_ASSERT_TRUE(str_contains_icase("Panasonic_TX55.ir", "55"));
}

void test_irdb_extension_search(void)
{
	/* Searching for ".ir" should match all .ir files */
	TEST_ASSERT_TRUE(str_contains_icase("Samsung_TV.ir", ".ir"));
	TEST_ASSERT_TRUE(str_contains_icase("LG_Soundbar.ir", ".ir"));
}

/* ===================================================================
 * Runner
 * =================================================================== */

int main(void)
{
	UNITY_BEGIN();

	/* Basic matching */
	RUN_TEST(test_exact_match);
	RUN_TEST(test_substring_at_start);
	RUN_TEST(test_substring_in_middle);
	RUN_TEST(test_substring_at_end);
	RUN_TEST(test_no_match);
	RUN_TEST(test_needle_longer_than_haystack);

	/* Case insensitivity */
	RUN_TEST(test_case_lower_query_upper_filename);
	RUN_TEST(test_case_upper_query_lower_filename);
	RUN_TEST(test_case_mixed);

	/* Edge cases */
	RUN_TEST(test_empty_needle_matches_everything);
	RUN_TEST(test_null_needle_matches_everything);
	RUN_TEST(test_null_haystack_no_match);
	RUN_TEST(test_null_haystack_empty_needle_matches);
	RUN_TEST(test_single_char_match);
	RUN_TEST(test_single_char_no_match);
	RUN_TEST(test_needle_equals_haystack_length);

	/* Real IRDB scenarios */
	RUN_TEST(test_irdb_brand_search);
	RUN_TEST(test_irdb_partial_model);
	RUN_TEST(test_irdb_extension_search);

	return UNITY_END();
}
