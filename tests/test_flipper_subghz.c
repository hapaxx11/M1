/* See COPYING.txt for license details. */

/*
 * test_flipper_subghz.c
 *
 * Regression tests for flipper_subghz.c — preset-to-modulation and
 * frequency-to-band mapping functions.
 *
 * Bug context:
 *   1. Sub-GHz 2FSK replay at all frequencies — Emulate incorrectly used OOK
 *      for FSK presets because the preset parser was case-sensitive and did
 *      not recognize mixed-case Flipper preset strings like
 *      "FuriHalSubGhzPreset2FSKDev238Async".
 *   2. ASK presets were not recognized at all, causing modulation to fall
 *      through to MODULATION_UNKNOWN.
 *   3. Frequency-to-band mapping determines which SI4463 radio config to
 *      load; incorrect mapping causes the radio to fail to transmit.
 *
 * Build:
 *   cmake -B build-tests -S tests && cmake --build build-tests
 *   ctest --test-dir build-tests --output-on-failure
 */

#include "unity.h"
#include "m1_sub_ghz.h"

/* Declaration from flipper_subghz.h — we include the header directly */
#include "flipper_subghz.h"

void setUp(void) {}
void tearDown(void) {}

/* ===================================================================
 * flipper_subghz_preset_to_modulation — case-insensitive matching
 *
 * This was the core bug: the original parser used strcmp/strstr which
 * was case-sensitive.  Flipper .sub files use mixed-case preset names
 * like "FuriHalSubGhzPresetOok650Async".  The fix added stristr()
 * for case-insensitive substring matching.
 * =================================================================== */

void test_preset_ook_standard(void)
{
	/* Standard Flipper OOK preset — most common in .sub files */
	TEST_ASSERT_EQUAL_UINT8(MODULATION_OOK,
		flipper_subghz_preset_to_modulation("FuriHalSubGhzPresetOok650Async"));
}

void test_preset_ook_270(void)
{
	TEST_ASSERT_EQUAL_UINT8(MODULATION_OOK,
		flipper_subghz_preset_to_modulation("FuriHalSubGhzPresetOok270Async"));
}

void test_preset_ook_lowercase(void)
{
	/* Case-insensitive: all lowercase should still match */
	TEST_ASSERT_EQUAL_UINT8(MODULATION_OOK,
		flipper_subghz_preset_to_modulation("furihalsubghzpresetook650async"));
}

void test_preset_ook_uppercase(void)
{
	/* Case-insensitive: all uppercase */
	TEST_ASSERT_EQUAL_UINT8(MODULATION_OOK,
		flipper_subghz_preset_to_modulation("FURIHALSUBGHZPRESETOOK650ASYNC"));
}

void test_preset_fsk_2fsk(void)
{
	/* 2FSK preset — this was the bug: standard band configs forced OOK
	 * for all frequencies, and CUSTOM band only checked FSK >= 850 MHz */
	TEST_ASSERT_EQUAL_UINT8(MODULATION_FSK,
		flipper_subghz_preset_to_modulation("FuriHalSubGhzPreset2FSKDev238Async"));
}

void test_preset_fsk_2fsk_476(void)
{
	TEST_ASSERT_EQUAL_UINT8(MODULATION_FSK,
		flipper_subghz_preset_to_modulation("FuriHalSubGhzPreset2FSKDev476Async"));
}

void test_preset_fsk_gfsk(void)
{
	/* GFSK variant — should also match "FSK" substring */
	TEST_ASSERT_EQUAL_UINT8(MODULATION_FSK,
		flipper_subghz_preset_to_modulation("FuriHalSubGhzPresetGFSK9_99KbAsync"));
}

void test_preset_fsk_lowercase(void)
{
	TEST_ASSERT_EQUAL_UINT8(MODULATION_FSK,
		flipper_subghz_preset_to_modulation("furihalsubghzpreset2fskdev238async"));
}

void test_preset_ask(void)
{
	/* ASK preset — was completely unrecognized before the fix */
	TEST_ASSERT_EQUAL_UINT8(MODULATION_ASK,
		flipper_subghz_preset_to_modulation("FuriHalSubGhzPresetASK"));
}

void test_preset_ask_lowercase(void)
{
	TEST_ASSERT_EQUAL_UINT8(MODULATION_ASK,
		flipper_subghz_preset_to_modulation("ask_custom_preset"));
}

void test_preset_unknown(void)
{
	/* Unrecognized preset */
	TEST_ASSERT_EQUAL_UINT8(MODULATION_UNKNOWN,
		flipper_subghz_preset_to_modulation("SomeUnknownPreset"));
}

void test_preset_null(void)
{
	TEST_ASSERT_EQUAL_UINT8(MODULATION_UNKNOWN,
		flipper_subghz_preset_to_modulation(NULL));
}

void test_preset_empty(void)
{
	TEST_ASSERT_EQUAL_UINT8(MODULATION_UNKNOWN,
		flipper_subghz_preset_to_modulation(""));
}

/* ===================================================================
 * flipper_subghz_freq_to_band — frequency to SI4463 band mapping
 *
 * Correct band mapping is essential for loading the right radio config.
 * A wrong band means the radio may not transmit or may use the wrong
 * antenna path.
 * =================================================================== */

void test_freq_to_band_300mhz(void)
{
	TEST_ASSERT_EQUAL_UINT8(SUB_GHZ_BAND_300,
		flipper_subghz_freq_to_band(300000000));
}

void test_freq_to_band_310mhz(void)
{
	TEST_ASSERT_EQUAL_UINT8(SUB_GHZ_BAND_310,
		flipper_subghz_freq_to_band(310000000));
}

void test_freq_to_band_315mhz(void)
{
	TEST_ASSERT_EQUAL_UINT8(SUB_GHZ_BAND_315,
		flipper_subghz_freq_to_band(315000000));
}

void test_freq_to_band_345mhz(void)
{
	TEST_ASSERT_EQUAL_UINT8(SUB_GHZ_BAND_345,
		flipper_subghz_freq_to_band(345000000));
}

void test_freq_to_band_390mhz(void)
{
	TEST_ASSERT_EQUAL_UINT8(SUB_GHZ_BAND_390,
		flipper_subghz_freq_to_band(390000000));
}

void test_freq_to_band_433mhz(void)
{
	/* 433 MHz exactly: freq_mhz = 433, which is <= 433 → BAND_433 */
	TEST_ASSERT_EQUAL_UINT8(SUB_GHZ_BAND_433,
		flipper_subghz_freq_to_band(433000000));
}

void test_freq_to_band_433_92mhz(void)
{
	/* 433.92 MHz — the most common frequency for garage doors/remotes.
	 * freq_mhz = 433920000/1000000 = 433, which is <= 433 → BAND_433 */
	TEST_ASSERT_EQUAL_UINT8(SUB_GHZ_BAND_433,
		flipper_subghz_freq_to_band(433920000));
}

void test_freq_to_band_868mhz(void)
{
	/* 868 MHz: freq_mhz = 868, which is > 500 → BAND_915 */
	TEST_ASSERT_EQUAL_UINT8(SUB_GHZ_BAND_915,
		flipper_subghz_freq_to_band(868000000));
}

void test_freq_to_band_915mhz(void)
{
	TEST_ASSERT_EQUAL_UINT8(SUB_GHZ_BAND_915,
		flipper_subghz_freq_to_band(915000000));
}

void test_freq_to_band_boundary_305(void)
{
	/* 305 MHz: freq_mhz=305, <= 305 → BAND_300 (boundary) */
	TEST_ASSERT_EQUAL_UINT8(SUB_GHZ_BAND_300,
		flipper_subghz_freq_to_band(305000000));
}

void test_freq_to_band_boundary_306(void)
{
	/* 306 MHz: freq_mhz=306, > 305 but <= 312 → BAND_310 */
	TEST_ASSERT_EQUAL_UINT8(SUB_GHZ_BAND_310,
		flipper_subghz_freq_to_band(306000000));
}

/* ===================================================================
 * Runner
 * =================================================================== */

int main(void)
{
	UNITY_BEGIN();

	/* Preset-to-modulation — case insensitive matching regression */
	RUN_TEST(test_preset_ook_standard);
	RUN_TEST(test_preset_ook_270);
	RUN_TEST(test_preset_ook_lowercase);
	RUN_TEST(test_preset_ook_uppercase);
	RUN_TEST(test_preset_fsk_2fsk);
	RUN_TEST(test_preset_fsk_2fsk_476);
	RUN_TEST(test_preset_fsk_gfsk);
	RUN_TEST(test_preset_fsk_lowercase);
	RUN_TEST(test_preset_ask);
	RUN_TEST(test_preset_ask_lowercase);
	RUN_TEST(test_preset_unknown);
	RUN_TEST(test_preset_null);
	RUN_TEST(test_preset_empty);

	/* Frequency-to-band mapping */
	RUN_TEST(test_freq_to_band_300mhz);
	RUN_TEST(test_freq_to_band_310mhz);
	RUN_TEST(test_freq_to_band_315mhz);
	RUN_TEST(test_freq_to_band_345mhz);
	RUN_TEST(test_freq_to_band_390mhz);
	RUN_TEST(test_freq_to_band_433mhz);
	RUN_TEST(test_freq_to_band_433_92mhz);
	RUN_TEST(test_freq_to_band_868mhz);
	RUN_TEST(test_freq_to_band_915mhz);
	RUN_TEST(test_freq_to_band_boundary_305);
	RUN_TEST(test_freq_to_band_boundary_306);

	return UNITY_END();
}
