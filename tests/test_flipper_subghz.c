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
#include <stdio.h>

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

void test_freq_to_band_330mhz(void)
{
	/* 330 MHz: freq_mhz=330, > 320 but <= 358 → BAND_345
	 * 330 MHz is a common Asian gate remote frequency. */
	TEST_ASSERT_EQUAL_UINT8(SUB_GHZ_BAND_345,
		flipper_subghz_freq_to_band(330000000));
}

/* ===================================================================
 * flipper_subghz_is_m1_native_header — M1 native .sgh file detection
 *
 * Bug context (GitHub issue "Emulate sgh failure"):
 *   Files saved by C3.12 (fw 0.8.x) and SiN360 (fw 0.9.x) in M1 native
 *   .sgh format could not be selected in the Saved scene — the scene popped
 *   back to the menu instead of showing the action menu (Emulate/Info/
 *   Rename/Delete).
 *
 *   Root cause: flipper_subghz_load() only accepted Flipper .sub format
 *   versions "1" and "2".  M1 native .sgh files use the firmware version
 *   as the Version: field (e.g. "0.8", "0.9"), so the version check failed
 *   and flipper_subghz_load() returned false.  open_saved_browser() treated
 *   false as "load failed" and popped the scene.
 *
 *   Fix: save the Filetype: value before reading the Version: line, then
 *   detect "M1 SubGHz" in the filetype to route to the new M1-native
 *   parsers (m1sgh_parse_raw / m1sgh_parse_packet).
 *
 * These tests exercise flipper_subghz_is_m1_native_header(), the pure-logic
 * helper that was extracted from the fixed code path for testability.
 * All test_m1_* assertions below would have failed (returned false/true
 * respectively) before the fix.
 * =================================================================== */

void test_m1_native_noise_v09(void)
{
	/* Hapax / SiN360 fw 0.9.x RAW recording — must be detected as M1 native */
	TEST_ASSERT_TRUE(
		flipper_subghz_is_m1_native_header("M1 SubGHz NOISE", "0.9"));
}

void test_m1_native_noise_v08(void)
{
	/* C3.12 fw 0.8.x RAW recording — must be detected as M1 native */
	TEST_ASSERT_TRUE(
		flipper_subghz_is_m1_native_header("M1 SubGHz NOISE", "0.8"));
}

void test_m1_native_packet_v08(void)
{
	/* M1 native PACKET file (legacy) — must be detected as M1 native */
	TEST_ASSERT_TRUE(
		flipper_subghz_is_m1_native_header("M1 SubGHz PACKET", "0.8"));
}

void test_m1_native_packet_v09(void)
{
	TEST_ASSERT_TRUE(
		flipper_subghz_is_m1_native_header("M1 SubGHz PACKET", "0.9"));
}

void test_flipper_raw_not_m1(void)
{
	/* Flipper RAW .sub file (version 1) — must NOT be detected as M1 native */
	TEST_ASSERT_FALSE(
		flipper_subghz_is_m1_native_header("Flipper SubGhz RAW File", "1"));
}

void test_flipper_key_not_m1(void)
{
	/* Flipper Key .sub file (version 2) — must NOT be detected as M1 native */
	TEST_ASSERT_FALSE(
		flipper_subghz_is_m1_native_header("Flipper SubGhz Key File", "2"));
}

void test_flipper_raw_v2_not_m1(void)
{
	/* Flipper RAW .sub file (version 2) */
	TEST_ASSERT_FALSE(
		flipper_subghz_is_m1_native_header("Flipper SubGhz RAW File", "2"));
}

void test_m1_native_null_filetype(void)
{
	TEST_ASSERT_FALSE(flipper_subghz_is_m1_native_header(NULL, "0.9"));
}

void test_m1_native_null_version(void)
{
	TEST_ASSERT_FALSE(flipper_subghz_is_m1_native_header("M1 SubGHz NOISE", NULL));
}

void test_m1_native_both_null(void)
{
	TEST_ASSERT_FALSE(flipper_subghz_is_m1_native_header(NULL, NULL));
}

void test_m1_native_unknown_filetype(void)
{
	/* Unknown format — must return false */
	TEST_ASSERT_FALSE(flipper_subghz_is_m1_native_header("SomeOtherFormat", "0.9"));
}

/* ===================================================================
 * flipper_subghz_load() — is_m1_native flag and file parsing
 *
 * These tests write temporary files to /tmp and exercise the full
 * flipper_subghz_load() path, which requires the stdio-backed ff.h stub.
 *
 * Bug context (fixed in this PR):
 *   C3.12 and SiN360 .sgh NOISE files caused "Memory error" (error 5)
 *   when Emulate was chosen in the Saved scene because they were always
 *   routed through sub_ghz_replay_flipper_file(), which wrote a temp file
 *   and then tried to parse it via sub_ghz_raw_samples_init().  The fix is
 *   to set is_m1_native = true during flipper_subghz_load() and use
 *   sub_ghz_replay_datafile() (direct path, no temp file) for these files.
 * =================================================================== */

/* Helper: write a string to a temp file */
static void write_tmp(const char *path, const char *content)
{
	FILE *f = fopen(path, "w");
	if (f) { fputs(content, f); fclose(f); }
}

void test_load_m1_native_noise_sets_flag(void)
{
	/* C3.12 / SiN360 style M1 native NOISE file with CRLF line endings */
	const char *path = "/tmp/test_m1_noise.sgh";
	write_tmp(path,
	    "Filetype: M1 SubGHz NOISE\r\n"
	    "Version: 0.9\r\n"
	    "Frequency: 433920000\r\n"
	    "Modulation: OOK\r\n"
	    "Data: 100 200 300 400\r\n");

	flipper_subghz_signal_t sig;
	memset(&sig, 0, sizeof(sig));
	bool ok = flipper_subghz_load(path, &sig);

	TEST_ASSERT_TRUE(ok);
	TEST_ASSERT_TRUE(sig.is_m1_native);
	TEST_ASSERT_EQUAL(FLIPPER_SUBGHZ_TYPE_RAW, sig.type);
	TEST_ASSERT_EQUAL_UINT32(433920000UL, sig.frequency);
	TEST_ASSERT_EQUAL_UINT16(4, sig.raw_count); /* 4 unsigned values loaded */

	remove(path);
}

void test_load_m1_native_noise_lf_only(void)
{
	/* C3.12-style file with LF-only line endings (as produced on Linux).
	 * flipper_subghz_load() must parse this correctly (the stdio ff_read_line
	 * path handles LF-only; the sub_ghz_raw_samples_init CRLF fix handles
	 * the streaming engine path). */
	const char *path = "/tmp/test_m1_noise_lf.sgh";
	write_tmp(path,
	    "Filetype: M1 SubGHz NOISE\n"
	    "Version: 0.8\n"
	    "Frequency: 315000000\n"
	    "Modulation: OOK\n"
	    "Data: 500 600 700 800 900\n");

	flipper_subghz_signal_t sig;
	memset(&sig, 0, sizeof(sig));
	bool ok = flipper_subghz_load(path, &sig);

	TEST_ASSERT_TRUE(ok);
	TEST_ASSERT_TRUE(sig.is_m1_native);
	TEST_ASSERT_EQUAL(FLIPPER_SUBGHZ_TYPE_RAW, sig.type);
	TEST_ASSERT_EQUAL_UINT32(315000000UL, sig.frequency);
	TEST_ASSERT_EQUAL_UINT16(5, sig.raw_count);

	remove(path);
}

void test_load_flipper_raw_not_m1_native(void)
{
	/* Flipper .sub RAW file — must NOT set is_m1_native */
	const char *path = "/tmp/test_flipper_raw.sub";
	write_tmp(path,
	    "Filetype: Flipper SubGhz RAW File\r\n"
	    "Version: 1\r\n"
	    "Frequency: 433920000\r\n"
	    "Preset: FuriHalSubGhzPresetOok650Async\r\n"
	    "Protocol: RAW\r\n"
	    "RAW_Data: 100 -200 300 -400\r\n");

	flipper_subghz_signal_t sig;
	memset(&sig, 0, sizeof(sig));
	bool ok = flipper_subghz_load(path, &sig);

	TEST_ASSERT_TRUE(ok);
	TEST_ASSERT_FALSE(sig.is_m1_native);
	TEST_ASSERT_EQUAL(FLIPPER_SUBGHZ_TYPE_RAW, sig.type);
	TEST_ASSERT_EQUAL_UINT32(433920000UL, sig.frequency);

	remove(path);
}

void test_load_flipper_key_packet_fields(void)
{
	/* Flipper Key .sub file — PACKET round-trip: key, bits, TE must survive */
	const char *path = "/tmp/test_flipper_key.sub";
	write_tmp(path,
	    "Filetype: Flipper SubGhz Key File\r\n"
	    "Version: 1\r\n"
	    "Frequency: 433920000\r\n"
	    "Preset: FuriHalSubGhzPresetOok650Async\r\n"
	    "Protocol: Princeton\r\n"
	    "Bit: 24\r\n"
	    "Key: 00 00 00 00 00 52 A1 2E\r\n"
	    "TE: 400\r\n");

	flipper_subghz_signal_t sig;
	memset(&sig, 0, sizeof(sig));
	bool ok = flipper_subghz_load(path, &sig);

	TEST_ASSERT_TRUE(ok);
	TEST_ASSERT_FALSE(sig.is_m1_native);
	TEST_ASSERT_EQUAL(FLIPPER_SUBGHZ_TYPE_PARSED, sig.type);
	TEST_ASSERT_EQUAL_UINT32(433920000UL, sig.frequency);
	TEST_ASSERT_EQUAL_UINT32(24, sig.bit_count);
	TEST_ASSERT_EQUAL_UINT32(400, sig.te);
	TEST_ASSERT_EQUAL_UINT64(0x52A12EULL, sig.key);

	remove(path);
}

void test_load_m1_native_packet_not_noise(void)
{
	/* M1 native PACKET file — is_m1_native true, but is NOT a NOISE file.
	 * The dispatch in the saved scene must not use sub_ghz_replay_datafile()
	 * for PACKET files (the key encoder path handles them). */
	const char *path = "/tmp/test_m1_packet.sgh";
	write_tmp(path,
	    "Filetype: M1 SubGHz PACKET\r\n"
	    "Version: 0.9\r\n"
	    "Frequency: 433920000\r\n"
	    "Modulation: OOK\r\n"
	    "Protocol: Princeton\r\n"
	    "Bits: 24\r\n"
	    "Payload: 0x000000000052A12E\r\n"
	    "BT: 400\r\n"
	    "IT: 0\r\n");

	flipper_subghz_signal_t sig;
	memset(&sig, 0, sizeof(sig));
	bool ok = flipper_subghz_load(path, &sig);

	TEST_ASSERT_TRUE(ok);
	TEST_ASSERT_TRUE(sig.is_m1_native);            /* is native */
	TEST_ASSERT_EQUAL(FLIPPER_SUBGHZ_TYPE_PARSED, sig.type); /* but NOT RAW */
	TEST_ASSERT_EQUAL_UINT32(433920000UL, sig.frequency);
	TEST_ASSERT_EQUAL_UINT32(24, sig.bit_count);
	TEST_ASSERT_EQUAL_UINT32(400, sig.te);

	remove(path);
}

/* ===================================================================
 * flipper_subghz_probe() — lightweight header probe
 *
 * Verifies that probe() extracts is_m1_native, is_noise, frequency, and
 * modulation without loading any Data: samples.
 * =================================================================== */

void test_probe_m1_native_noise(void)
{
	const char *path = "/tmp/test_probe_noise.sgh";
	write_tmp(path,
	    "Filetype: M1 SubGHz NOISE\r\n"
	    "Version: 0.9\r\n"
	    "Frequency: 433920000\r\n"
	    "Modulation: OOK\r\n"
	    "Data: 100 200 300\r\n");

	flipper_subghz_probe_t probe;
	bool ok = flipper_subghz_probe(path, &probe);

	TEST_ASSERT_TRUE(ok);
	TEST_ASSERT_TRUE(probe.is_m1_native);
	TEST_ASSERT_TRUE(probe.is_noise);
	TEST_ASSERT_EQUAL_UINT32(433920000UL, probe.frequency);
	TEST_ASSERT_EQUAL_UINT8(MODULATION_OOK, probe.modulation);

	remove(path);
}

void test_probe_flipper_raw_not_m1(void)
{
	const char *path = "/tmp/test_probe_flipper.sub";
	write_tmp(path,
	    "Filetype: Flipper SubGhz RAW File\r\n"
	    "Version: 1\r\n"
	    "Frequency: 868350000\r\n"
	    "Preset: FuriHalSubGhzPreset2FSKDev238Async\r\n"
	    "Protocol: RAW\r\n"
	    "RAW_Data: 100 -200\r\n");

	flipper_subghz_probe_t probe;
	bool ok = flipper_subghz_probe(path, &probe);

	TEST_ASSERT_TRUE(ok);
	TEST_ASSERT_FALSE(probe.is_m1_native);
	TEST_ASSERT_TRUE(probe.is_noise); /* Flipper RAW = noise */
	TEST_ASSERT_EQUAL_UINT32(868350000UL, probe.frequency);
	TEST_ASSERT_EQUAL_UINT8(MODULATION_FSK, probe.modulation);

	remove(path);
}

void test_probe_nonexistent_file(void)
{
	flipper_subghz_probe_t probe;
	bool ok = flipper_subghz_probe("/tmp/does_not_exist_xyz.sgh", &probe);
	TEST_ASSERT_FALSE(ok);
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
	RUN_TEST(test_freq_to_band_330mhz);

	/* M1 native .sgh format header detection — "Emulate sgh failure" regression */
	RUN_TEST(test_m1_native_noise_v09);
	RUN_TEST(test_m1_native_noise_v08);
	RUN_TEST(test_m1_native_packet_v08);
	RUN_TEST(test_m1_native_packet_v09);
	RUN_TEST(test_flipper_raw_not_m1);
	RUN_TEST(test_flipper_key_not_m1);
	RUN_TEST(test_flipper_raw_v2_not_m1);
	RUN_TEST(test_m1_native_null_filetype);
	RUN_TEST(test_m1_native_null_version);
	RUN_TEST(test_m1_native_both_null);
	RUN_TEST(test_m1_native_unknown_filetype);

	/* flipper_subghz_load() with actual files — is_m1_native flag and CRLF/LF */
	RUN_TEST(test_load_m1_native_noise_sets_flag);
	RUN_TEST(test_load_m1_native_noise_lf_only);
	RUN_TEST(test_load_flipper_raw_not_m1_native);
	RUN_TEST(test_load_flipper_key_packet_fields);
	RUN_TEST(test_load_m1_native_packet_not_noise);

	/* flipper_subghz_probe() — lightweight header probe */
	RUN_TEST(test_probe_m1_native_noise);
	RUN_TEST(test_probe_flipper_raw_not_m1);
	RUN_TEST(test_probe_nonexistent_file);

	return UNITY_END();
}
