/* See COPYING.txt for license details. */

/*
 * test_fw_source_filter.c
 *
 * Unit tests for fw_source_asset_matches_filter() — OTA asset filtering.
 *
 * Tests the pure-logic function that determines whether a GitHub release
 * asset name matches the configured include/exclude filter criteria used
 * by the firmware download module.
 *
 * Build:
 *   cmake -B build-tests -S tests && cmake --build build-tests
 *   ctest --test-dir build-tests --output-on-failure
 */

#include "unity.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "m1_fw_source.h"

void setUp(void) {}
void tearDown(void) {}

/* ===================================================================
 * Include filter (suffix matching)
 * =================================================================== */

void test_filter_include_bin(void)
{
	TEST_ASSERT_TRUE(fw_source_asset_matches_filter(
		"M1_Hapax_v0.9.0.1_wCRC.bin", "_wCRC.bin", NULL));
}

void test_filter_include_no_match(void)
{
	TEST_ASSERT_FALSE(fw_source_asset_matches_filter(
		"M1_Hapax_v0.9.0.1.elf", "_wCRC.bin", NULL));
}

void test_filter_include_exact(void)
{
	TEST_ASSERT_TRUE(fw_source_asset_matches_filter(
		"_wCRC.bin", "_wCRC.bin", NULL));
}

void test_filter_include_shorter_than_filter(void)
{
	TEST_ASSERT_FALSE(fw_source_asset_matches_filter(
		".bin", "_wCRC.bin", NULL));
}

void test_filter_include_empty(void)
{
	/* Empty include filter = match anything */
	TEST_ASSERT_TRUE(fw_source_asset_matches_filter(
		"anything.bin", "", NULL));
}

void test_filter_include_null(void)
{
	/* NULL include filter = match anything */
	TEST_ASSERT_TRUE(fw_source_asset_matches_filter(
		"anything.bin", NULL, NULL));
}

/* ===================================================================
 * Exclude filter (space-separated suffixes)
 * =================================================================== */

void test_filter_exclude_single(void)
{
	TEST_ASSERT_FALSE(fw_source_asset_matches_filter(
		"firmware.elf", NULL, ".elf"));
}

void test_filter_exclude_multiple(void)
{
	TEST_ASSERT_FALSE(fw_source_asset_matches_filter(
		"firmware.hex", NULL, ".elf .hex .map"));
}

void test_filter_exclude_no_match(void)
{
	TEST_ASSERT_TRUE(fw_source_asset_matches_filter(
		"firmware.bin", NULL, ".elf .hex .map"));
}

void test_filter_exclude_empty(void)
{
	TEST_ASSERT_TRUE(fw_source_asset_matches_filter(
		"firmware.bin", NULL, ""));
}

void test_filter_exclude_null(void)
{
	TEST_ASSERT_TRUE(fw_source_asset_matches_filter(
		"firmware.bin", NULL, NULL));
}

/* ===================================================================
 * Combined include + exclude
 * =================================================================== */

void test_filter_combined_pass(void)
{
	/* Matches include, not excluded */
	TEST_ASSERT_TRUE(fw_source_asset_matches_filter(
		"M1_Hapax_v0.9.0.1_wCRC.bin", ".bin", ".elf .hex"));
}

void test_filter_combined_include_fail(void)
{
	/* Doesn't match include */
	TEST_ASSERT_FALSE(fw_source_asset_matches_filter(
		"M1_Hapax_v0.9.0.1.elf", ".bin", ".elf .hex"));
}

void test_filter_combined_exclude_overrides(void)
{
	/* Matches include but is excluded */
	TEST_ASSERT_FALSE(fw_source_asset_matches_filter(
		"M1_Hapax_debug.bin", ".bin", "debug.bin"));
}

/* ===================================================================
 * Real-world OTA filter scenarios
 * =================================================================== */

void test_filter_hapax_release(void)
{
	/* Hapax releases use _wCRC.bin suffix, exclude ESP32 and md5 */
	TEST_ASSERT_TRUE(fw_source_asset_matches_filter(
		"M1_Hapax_v0.9.0.5_wCRC.bin", "_wCRC.bin", "ESP32 .md5"));
}

void test_filter_hapax_exclude_esp32(void)
{
	TEST_ASSERT_FALSE(fw_source_asset_matches_filter(
		"factory_ESP32C6-SPI.bin", "_wCRC.bin", "ESP32 .md5"));
}

void test_filter_esp32_category(void)
{
	/* ESP32 firmware uses .bin suffix */
	TEST_ASSERT_TRUE(fw_source_asset_matches_filter(
		"factory_ESP32C6-SPI.bin", ".bin", ".md5"));
}

void test_filter_md5_excluded(void)
{
	/* md5 file matches .md5 include filter */
	TEST_ASSERT_TRUE(fw_source_asset_matches_filter(
		"factory_ESP32C6-SPI.md5", ".md5", ""));
	/* But if we're filtering for .bin, .md5 just doesn't match include */
	TEST_ASSERT_FALSE(fw_source_asset_matches_filter(
		"factory_ESP32C6-SPI.md5", ".bin", ""));
}

static fw_source_t brain_source(void)
{
	fw_source_t source;
	memset(&source, 0, sizeof(source));
	strcpy(source.github.owner, "bedge117");
	strcpy(source.github.repo, "m1-esp32-brain");
	strcpy(source.asset_filter, "factory_m1-esp32-brain.bin");
	return source;
}

void test_filter_cd3_brain_factory_name_alias(void)
{
	fw_source_t source = brain_source();
	TEST_ASSERT_TRUE(fw_source_asset_matches_source(
		&source, "factory_ESP32C6-SPI.bin"));
	TEST_ASSERT_TRUE(fw_source_asset_matches_source(
		&source, "factory_m1-esp32-brain.bin"));
}

void test_filter_cd3_brain_alias_rejects_other_assets_and_sources(void)
{
	fw_source_t source = brain_source();
	TEST_ASSERT_FALSE(fw_source_asset_matches_source(
		&source, "factory_ESP32C6-SPI.md5"));
	TEST_ASSERT_FALSE(fw_source_asset_matches_source(
		&source, "app_m1-esp32-brain.bin"));
	strcpy(source.github.repo, "other-repo");
	TEST_ASSERT_FALSE(fw_source_asset_matches_source(
		&source, "factory_ESP32C6-SPI.bin"));
}

/* ===================================================================
 * Runner
 * =================================================================== */

int main(void)
{
	UNITY_BEGIN();

	/* Include filter */
	RUN_TEST(test_filter_include_bin);
	RUN_TEST(test_filter_include_no_match);
	RUN_TEST(test_filter_include_exact);
	RUN_TEST(test_filter_include_shorter_than_filter);
	RUN_TEST(test_filter_include_empty);
	RUN_TEST(test_filter_include_null);

	/* Exclude filter */
	RUN_TEST(test_filter_exclude_single);
	RUN_TEST(test_filter_exclude_multiple);
	RUN_TEST(test_filter_exclude_no_match);
	RUN_TEST(test_filter_exclude_empty);
	RUN_TEST(test_filter_exclude_null);

	/* Combined */
	RUN_TEST(test_filter_combined_pass);
	RUN_TEST(test_filter_combined_include_fail);
	RUN_TEST(test_filter_combined_exclude_overrides);

	/* Real-world scenarios */
	RUN_TEST(test_filter_hapax_release);
	RUN_TEST(test_filter_hapax_exclude_esp32);
	RUN_TEST(test_filter_esp32_category);
	RUN_TEST(test_filter_md5_excluded);
	RUN_TEST(test_filter_cd3_brain_factory_name_alias);
	RUN_TEST(test_filter_cd3_brain_alias_rejects_other_assets_and_sources);

	return UNITY_END();
}
