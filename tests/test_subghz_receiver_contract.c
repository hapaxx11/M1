/* See COPYING.txt for license details. */

#include "unity.h"
#include "subghz_radio_config.h"
#include "subghz_rx_edge_filter.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M1_ROOT
#error "M1_ROOT must be defined by CMake"
#endif

static char *read_file(const char *relpath)
{
    char path[512];
    FILE *fp;
    long size;
    char *buf;

    snprintf(path, sizeof(path), "%s/%s", M1_ROOT, relpath);
    fp = fopen(path, "rb");
    TEST_ASSERT_NOT_NULL_MESSAGE(fp, path);
    TEST_ASSERT_EQUAL_INT(0, fseek(fp, 0, SEEK_END));
    size = ftell(fp);
    TEST_ASSERT_GREATER_THAN_INT(0, size);
    TEST_ASSERT_EQUAL_INT(0, fseek(fp, 0, SEEK_SET));
    buf = malloc((size_t)size + 1U);
    TEST_ASSERT_NOT_NULL(buf);
    TEST_ASSERT_EQUAL_size_t((size_t)size, fread(buf, 1U, (size_t)size, fp));
    buf[size] = '\0';
    fclose(fp);

    char *src = buf;
    char *dst = buf;
    while (*src != '\0')
    {
        if (*src != '\r')
            *dst++ = *src;
        src++;
    }
    *dst = '\0';
    return buf;
}

static char *slice_between(const char *content, const char *start, const char *end)
{
    const char *first = strstr(content, start);
    const char *last;
    char *slice;
    size_t len;

    TEST_ASSERT_NOT_NULL_MESSAGE(first, start);
    last = strstr(first, end);
    TEST_ASSERT_NOT_NULL_MESSAGE(last, end);
    len = (size_t)(last - first);
    slice = malloc(len + 1U);
    TEST_ASSERT_NOT_NULL(slice);
    memcpy(slice, first, len);
    slice[len] = '\0';
    return slice;
}

void setUp(void) { }
void tearDown(void) { }

void test_initial_hopper_frequency_is_applied_before_rx_start(void)
{
    char *content = read_file("m1_csrc/m1_subghz_scene_read.c");
    char *start = slice_between(content, "static void start_rx(SubGhzApp *app)\n{",
                                "static void resume_rx(SubGhzApp *app)\n{");

    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(start, "apply_initial_hopper(app);"),
                                 "fresh RX must initialize the first hopper channel");
    free(start);

    char *apply = slice_between(content, "static void apply_initial_hopper(SubGhzApp *app)",
                                "/*============================================================================*/");
    TEST_ASSERT_NOT_NULL_MESSAGE(
        strstr(apply, "subghz_apply_custom_config_ext(app->hopper_freq, app->mod_idx)"),
        "the first hopper frequency must reach the radio before RX starts");
    free(apply);
    free(content);
}

void test_named_presets_honor_requested_modulation(void)
{
    char *content = read_file("m1_csrc/m1_sub_ghz.c");
    char *opmode = slice_between(
        content,
        "static void sub_ghz_set_opmode(uint8_t opmode, uint8_t band, uint8_t channel, uint8_t tx_power)\n{",
        "static bool sub_ghz_custom_freq_entry(void)");

    TEST_ASSERT_NOT_NULL_MESSAGE(
        strstr(opmode, "subghz_radio_force_custom_config("),
        "FSK on low bands and OOK on 915 must use the modulation-aware custom path");
    free(opmode);
    free(content);
}

void test_hopper_dwell_covers_slow_protocol_frames(void)
{
    char *presets = read_file("m1_csrc/subghz_freq_presets.h");
    char *scene = read_file("m1_csrc/m1_subghz_scene.c");
    char *hopper = slice_between(scene, "if (app.hopper_active)",
                                   "prev_hopper_active = app.hopper_active;");

    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(presets, "#define SUBGHZ_HOPPER_DWELL_MS  750U"),
                                 "dwell must cover Honda V1 and the slower audited frames");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(hopper, ">= SUBGHZ_HOPPER_DWELL_MS"),
                                 "scene tick must use the shared dwell constant");
    TEST_ASSERT_NULL_MESSAGE(strstr(hopper, ">= 200U"),
                             "200 ms truncates slow preambles and weather frames");
    free(hopper);
    free(scene);
    free(presets);
}

void test_smart_id_selection_replaces_delegate_with_filtered_read(void)
{
    char *wrapper = read_file("m1_csrc/m1_subghz_scene_smart_signal_id.c");
    char *read = read_file("m1_csrc/m1_subghz_scene_read.c");

    TEST_ASSERT_NOT_NULL(strstr(wrapper,
        "subghz_scene_replace(app, SubGhzSceneRead)"));
    TEST_ASSERT_NOT_NULL(strstr(wrapper,
        "app->config_filter_mode = SubGhzConfigFilterFullRegistry"));
    TEST_ASSERT_NOT_NULL(strstr(read, "app->read_handoff_pending = false"));
    TEST_ASSERT_NOT_NULL(strstr(read,
        "subghz_freq_preset_find_hz(app->read_handoff.freq_hz)"));
    TEST_ASSERT_NOT_NULL(strstr(read,
        "subghz_set_user_custom_freq_ext(app->read_handoff.freq_hz)"));
    TEST_ASSERT_NOT_NULL(strstr(read, "app->hopping = false"));

    free(read);
    free(wrapper);
}

void test_guided_hop_uses_full_modem_reconfiguration_and_resets_decoder(void)
{
    char *content = read_file("m1_csrc/m1_subghz_scene_read.c");
    char *tick = strstr(content, "case SubGhzEventHopperTick:");

    TEST_ASSERT_NOT_NULL(tick);
    TEST_ASSERT_NOT_NULL(strstr(tick, "subghz_pulse_handler_reset()"));
    TEST_ASSERT_NOT_NULL(strstr(tick,
        "subghz_retune_custom_config_ext(entry.freq_hz, entry.mod_idx)"));
    TEST_ASSERT_NOT_NULL(strstr(tick,
        "subghz_retune_freq_hz_ext(entry.freq_hz)"));
    free(content);
}

void test_radio_init_applies_selected_flipper_profile(void)
{
    char *content = read_file("m1_csrc/m1_sub_ghz.c");
    char *opmode = slice_between(
        content,
        "static void sub_ghz_set_opmode(uint8_t opmode, uint8_t band, uint8_t channel, uint8_t tx_power)\n{",
        "static bool sub_ghz_custom_freq_entry(void)");

    TEST_ASSERT_NOT_NULL(strstr(opmode,
        "subghz_modem_profile_get(subghz_cfg.mod_idx)"));
    TEST_ASSERT_NOT_NULL(strstr(opmode, "SI446x_Set_Modem_Profile("));
    TEST_ASSERT_NOT_NULL(strstr(opmode,
        "subghz_scan_config.modulation >= MODULATION_FSK"));
    free(opmode);
    free(content);
}

void test_raw_path_heap_storage_is_zero_initialized(void)
{
    char *content = read_file("m1_csrc/m1_subghz_scene.c");
    char *run = slice_between(content, "void subghz_scene_app_run(void)",
                              "S_M1_Main_Q_t q_item;");

    TEST_ASSERT_NOT_NULL_MESSAGE(
        strstr(run, "memset(path_storage, 0, 4U * SUBGHZ_SAVED_PATH_MAX);"),
        "raw load/file/TX/saved paths must start empty, not as heap garbage");
    free(run);
    free(content);
}

void test_fsk_decode_edges_use_raw_noise_filter_without_changing_ook(void)
{
    TEST_ASSERT_FALSE(subghz_rx_edge_accept(19, false, true));
    TEST_ASSERT_TRUE(subghz_rx_edge_accept(20, false, true));
    TEST_ASSERT_TRUE(subghz_rx_edge_accept(50, false, true));
    TEST_ASSERT_TRUE(subghz_rx_edge_accept(79, false, true));
    TEST_ASSERT_TRUE(subghz_rx_edge_accept(80, false, true));
    TEST_ASSERT_TRUE(subghz_rx_edge_accept(250, false, true));
    TEST_ASSERT_TRUE(subghz_rx_edge_accept(1, false, false));
    TEST_ASSERT_FALSE(subghz_rx_edge_accept(79, true, false));
    TEST_ASSERT_TRUE(subghz_rx_edge_accept(80, true, false));
}

void test_fsk_raw_output_and_decode_filter_are_wired(void)
{
    char *profile = read_file(
        "Sub_Ghz/radio_config/m1_sub_ghz_915_rxtx_spc25_bwauto_baud106_dev200_e1_10_fsk.h");
    char *isr = read_file("m1_csrc/m1_int_hdl.c");

    TEST_ASSERT_NOT_NULL(strstr(profile, "#define RF_915_GPIO_PIN_CFG 0x13, 0x14,"));
    TEST_ASSERT_NOT_NULL(strstr(isr, "subghz_rx_edge_accept(cap_val, subghz_record_mode_flag,"));
    free(isr);
    free(profile);

    char *api = read_file("Sub_Ghz/m1_sub_ghz_api.c");
    TEST_ASSERT_NOT_NULL(strstr(api,
        "void SI446x_Set_Modem_Profile(const SubGhzModemProfile *profile)"));
    TEST_ASSERT_NOT_NULL(strstr(api, "profile->commands_size"));
    TEST_ASSERT_NOT_NULL(strstr(api, "&profile->commands[offset]"));
    TEST_ASSERT_NOT_NULL(strstr(api, "SI446x_Send_Cmd(command_size, si446x_cmd_buffer)"));
    TEST_ASSERT_NULL(strstr(api, "if (decimation >= 10U)"));
    free(api);
}

void test_345mhz_header_name_matches_its_wds_bandwidth(void)
{
    char *content = read_file(
        "Sub_Ghz/radio_config/si4463_revc2_345_rxtx_spacing250_bw200_baud10_error1_10_OOK.h");
    TEST_ASSERT_NOT_NULL(strstr(content, "RXBW(Hz): 200000"));
    free(content);
}

void test_raw_partial_flush_reaches_displayed_sample_count(void)
{
    char *recording = read_file("m1_csrc/m1_sub_ghz.c");
    char *scene = read_file("m1_csrc/m1_subghz_scene_read_raw.c");
    TEST_ASSERT_NOT_NULL(strstr(recording,
        "uint32_t remaining = ringbuffer_get_data_slots(&subghz_rx_rawdata_rb);"));
    TEST_ASSERT_NOT_NULL(strstr(recording,
        "subghz_record_total_samples, remaining"));
    TEST_ASSERT_NOT_NULL(strstr(scene,
        "app->raw_sample_count = subghz_read_raw_display_count_update("));
    TEST_ASSERT_NULL(strstr(scene, "s_signal_seen_in_chunk"));
    free(scene);
    free(recording);
}

void test_modulation_custom_path_decision_covers_all_profiles(void)
{
    TEST_ASSERT_FALSE(subghz_radio_force_custom_config(false, false));
    TEST_ASSERT_TRUE(subghz_radio_force_custom_config(false, true));
    TEST_ASSERT_TRUE(subghz_radio_force_custom_config(true, false));
    TEST_ASSERT_FALSE(subghz_radio_force_custom_config(true, true));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_modulation_custom_path_decision_covers_all_profiles);
    RUN_TEST(test_345mhz_header_name_matches_its_wds_bandwidth);
    RUN_TEST(test_raw_partial_flush_reaches_displayed_sample_count);
    RUN_TEST(test_fsk_decode_edges_use_raw_noise_filter_without_changing_ook);
    RUN_TEST(test_fsk_raw_output_and_decode_filter_are_wired);
    RUN_TEST(test_radio_init_applies_selected_flipper_profile);
    RUN_TEST(test_raw_path_heap_storage_is_zero_initialized);
    RUN_TEST(test_smart_id_selection_replaces_delegate_with_filtered_read);
    RUN_TEST(test_guided_hop_uses_full_modem_reconfiguration_and_resets_decoder);
    RUN_TEST(test_initial_hopper_frequency_is_applied_before_rx_start);
    RUN_TEST(test_named_presets_honor_requested_modulation);
    RUN_TEST(test_hopper_dwell_covers_slow_protocol_frames);
    return UNITY_END();
}
