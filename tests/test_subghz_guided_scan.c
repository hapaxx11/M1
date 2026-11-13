/* See COPYING.txt for license details. */

#include "unity.h"
#include "subghz_guided_scan.h"

#include <string.h>

void setUp(void) { }
void tearDown(void) { }

static rf_fingerprint_t make_fp(uint32_t freq_hz, rf_mod_family_t mod)
{
    rf_fingerprint_t fp;
    memset(&fp, 0, sizeof(fp));
    fp.freq_hz = freq_hz;
    fp.band = RF_BAND_433;
    fp.mod = mod;
    fp.rssi_dbm = -50;
    return fp;
}

void test_honda_style_ook_hit_maps_to_am650_and_exact_frequency(void)
{
    SubGhzGuidedEntry entry = {0};
    rf_sweep_hit_t hit;
    memset(&hit, 0, sizeof(hit));
    hit.freq_hz = 314980000UL;
    hit.mod = RF_MOD_OOK;

    TEST_ASSERT_TRUE(subghz_guided_entry_from_hit(&entry, &hit));
    TEST_ASSERT_EQUAL_UINT32(314980000UL, entry.freq_hz);
    TEST_ASSERT_EQUAL_UINT8(SUBGHZ_GUIDED_MOD_AM650, entry.mod_idx);
}

void test_fsk_hit_maps_to_fm476(void)
{
    SubGhzGuidedEntry entry = {0};
    rf_sweep_hit_t hit;
    memset(&hit, 0, sizeof(hit));
    hit.freq_hz = 433920000UL;
    hit.mod = RF_MOD_FSK;

    TEST_ASSERT_TRUE(subghz_guided_entry_from_hit(&entry, &hit));
    TEST_ASSERT_EQUAL_UINT8(SUBGHZ_GUIDED_MOD_FM476, entry.mod_idx);
}

void test_unknown_hit_modulation_uses_signature_then_safe_am_default(void)
{
    rf_protocol_sig_t sig;
    rf_sweep_hit_t hit;
    SubGhzGuidedEntry entry;
    memset(&sig, 0, sizeof(sig));
    memset(&hit, 0, sizeof(hit));
    sig.mod = RF_MOD_FSK;
    hit.freq_hz = 315000000UL;
    hit.confirmed_sig = &sig;

    TEST_ASSERT_TRUE(subghz_guided_entry_from_hit(&entry, &hit));
    TEST_ASSERT_EQUAL_UINT8(SUBGHZ_GUIDED_MOD_FM476, entry.mod_idx);

    hit.confirmed_sig = NULL;
    TEST_ASSERT_TRUE(subghz_guided_entry_from_hit(&entry, &hit));
    TEST_ASSERT_EQUAL_UINT8(SUBGHZ_GUIDED_MOD_AM650, entry.mod_idx);
}

void test_invalid_frequency_and_null_inputs_are_rejected(void)
{
    SubGhzGuidedEntry entry = {0};
    rf_sweep_hit_t hit;
    memset(&hit, 0, sizeof(hit));

    TEST_ASSERT_FALSE(subghz_guided_entry_from_hit(NULL, &hit));
    TEST_ASSERT_FALSE(subghz_guided_entry_from_hit(&entry, NULL));
    TEST_ASSERT_FALSE(subghz_guided_entry_from_hit(&entry, &hit));
    TEST_ASSERT_EQUAL_UINT32(0U, entry.freq_hz);
}

void test_report_plan_deduplicates_identical_channels_and_preserves_modulation(void)
{
    rf_protocol_sig_t sig;
    rf_match_result_t match;
    rf_sweep_report_t report;
    SubGhzGuidedPlan plan;
    rf_fingerprint_t fp = make_fp(433920000UL, RF_MOD_FSK);
    memset(&sig, 0, sizeof(sig));
    memset(&match, 0, sizeof(match));
    sig.name = "Synthetic";
    sig.mod = RF_MOD_FSK;
    match.sig = &sig;
    match.confidence = 90;
    rf_sweep_report_reset(&report);

    TEST_ASSERT_TRUE(rf_sweep_report_add(&report, &fp, &match));
    TEST_ASSERT_TRUE(rf_sweep_report_add(&report, &fp, &match));
    subghz_guided_plan_from_report(&plan, &report);

    TEST_ASSERT_EQUAL_UINT8(1, plan.count);
    TEST_ASSERT_EQUAL_UINT32(433920000UL, plan.entries[0].freq_hz);
    TEST_ASSERT_EQUAL_UINT8(SUBGHZ_GUIDED_MOD_FM476, plan.entries[0].mod_idx);
    TEST_ASSERT_EQUAL(RF_MOD_FSK, report.hits[0].mod);
}

void test_decode_confirmed_modulation_is_available_for_handoff(void)
{
    rf_sweep_report_t report;
    SubGhzGuidedPlan plan;
    rf_sweep_report_reset(&report);

    TEST_ASSERT_TRUE(rf_sweep_report_add_decoded_with_mod(
        &report, 315000000UL, RF_BAND_315, -55, "Honda V1", NULL, RF_MOD_OOK));
    subghz_guided_plan_from_report(&plan, &report);

    TEST_ASSERT_EQUAL_UINT8(1, plan.count);
    TEST_ASSERT_EQUAL_UINT8(SUBGHZ_GUIDED_MOD_AM650, plan.entries[0].mod_idx);
}

void test_guided_mode_requires_setting_hopping_and_cached_channels(void)
{
    SubGhzGuidedPlan plan;
    subghz_guided_plan_reset(&plan);

    TEST_ASSERT_FALSE(subghz_guided_scan_active(true, true, NULL));
    TEST_ASSERT_FALSE(subghz_guided_scan_active(true, true, &plan));
    plan.entries[0].freq_hz = 315000000UL;
    plan.entries[0].mod_idx = SUBGHZ_GUIDED_MOD_AM650;
    plan.count = 1;
    TEST_ASSERT_FALSE(subghz_guided_scan_active(false, true, &plan));
    TEST_ASSERT_FALSE(subghz_guided_scan_active(true, false, &plan));
    TEST_ASSERT_TRUE(subghz_guided_scan_active(true, true, &plan));
}

void test_next_index_wraps_and_handles_empty_plan(void)
{
    TEST_ASSERT_EQUAL_UINT8(0, subghz_guided_next_index(7, 0));
    TEST_ASSERT_EQUAL_UINT8(1, subghz_guided_next_index(0, 6));
    TEST_ASSERT_EQUAL_UINT8(0, subghz_guided_next_index(5, 6));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_honda_style_ook_hit_maps_to_am650_and_exact_frequency);
    RUN_TEST(test_fsk_hit_maps_to_fm476);
    RUN_TEST(test_unknown_hit_modulation_uses_signature_then_safe_am_default);
    RUN_TEST(test_invalid_frequency_and_null_inputs_are_rejected);
    RUN_TEST(test_report_plan_deduplicates_identical_channels_and_preserves_modulation);
    RUN_TEST(test_decode_confirmed_modulation_is_available_for_handoff);
    RUN_TEST(test_guided_mode_requires_setting_hopping_and_cached_channels);
    RUN_TEST(test_next_index_wraps_and_handles_empty_plan);
    return UNITY_END();
}
