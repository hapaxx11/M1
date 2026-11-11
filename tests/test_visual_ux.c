/* See COPYING.txt for license details. */

#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M1_ROOT
#error "M1_ROOT must be defined by CMake"
#endif

void setUp(void) {}
void tearDown(void) {}

static char *read_file(const char *relpath)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", M1_ROOT, relpath);
    FILE *fp = fopen(path, "rb");
    TEST_ASSERT_NOT_NULL_MESSAGE(fp, path);
    TEST_ASSERT_EQUAL_INT(0, fseek(fp, 0, SEEK_END));
    long size = ftell(fp);
    TEST_ASSERT_GREATER_THAN_INT(0, size);
    TEST_ASSERT_EQUAL_INT(0, fseek(fp, 0, SEEK_SET));
    char *content = malloc((size_t)size + 1U);
    TEST_ASSERT_NOT_NULL(content);
    TEST_ASSERT_EQUAL_size_t((size_t)size, fread(content, 1U, (size_t)size, fp));
    content[size] = '\0';
    fclose(fp);
    return content;
}

static void assert_contains(const char *content, const char *needle)
{
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(content, needle), needle);
}

static void assert_absent(const char *content, const char *needle)
{
    TEST_ASSERT_NULL_MESSAGE(strstr(content, needle), needle);
}

void test_selectors_use_shared_submenu_model(void)
{
    const char *files[] = {
        "m1_csrc/m1_esl_scene.c",
        "m1_csrc/m1_fw_download.c",
        "m1_csrc/m1_esp32_fw_download.c",
        "m1_csrc/m1_ir_custom.c",
        "m1_csrc/m1_sub_ghz.c",
    };
    for (size_t i = 0; i < sizeof(files) / sizeof(files[0]); ++i)
    {
        char *content = read_file(files[i]);
        assert_contains(content, "m1_submenu_draw");
        assert_contains(content, "m1_submenu_event");
        assert_absent(content, "m1_scene_draw_menu(");
        free(content);
    }
}

void test_headers_are_not_inverted(void)
{
    char *coin = read_file("m1_csrc/game_coin_flip.c");
    assert_absent(coin, "u8g2_DrawBox(&m1_u8g2, 0, 0, 128, 12)");
    assert_contains(coin, "u8g2_DrawHLine(&m1_u8g2, 0, 12, 128)");
    free(coin);

    char *download = read_file("m1_csrc/m1_fw_download.c");
    assert_contains(download, "u8g2_DrawHLine(&m1_u8g2, 0, 12, M1_LCD_DISPLAY_WIDTH)");
    assert_absent(download, "inverted title bar");
    free(download);
}

void test_back_hints_are_removed(void)
{
    const char *files[] = {
        "m1_csrc/m1_bt.c",
        "m1_csrc/m1_ir_custom.c",
        "m1_csrc/m1_sub_ghz.c",
        "m1_csrc/game_snake.c",
        "m1_csrc/game_tetris.c",
        "m1_csrc/game_trex.c",
        "m1_csrc/game_pong.c",
        "m1_csrc/game_2048.c",
        "m1_csrc/m1_rfid.c",
    };
    for (size_t i = 0; i < sizeof(files) / sizeof(files[0]); ++i)
    {
        char *content = read_file(files[i]);
        assert_absent(content, "\"Back\"");
        assert_absent(content, "BACK:Exit");
        free(content);
    }
    char *quick = read_file("m1_csrc/m1_ir_quick_remote.c");
    assert_absent(quick, "Back:Cancel");
    free(quick);
}

void test_ok_actions_use_center_column(void)
{
    const char *files[] = {
        "m1_csrc/m1_fw_download.c",
        "m1_csrc/m1_esp32_fw_download.c",
        "m1_csrc/m1_nfc.c",
        "m1_csrc/m1_gpio_uart.c",
        "m1_csrc/m1_rfid.c",
    };
    for (size_t i = 0; i < sizeof(files) / sizeof(files[0]); ++i)
    {
        char *content = read_file(files[i]);
        assert_absent(content, "OK:Download");
        assert_absent(content, "OK:Continue");
        assert_absent(content, "OK:Start");
        assert_absent(content, "OK:Mode");
        free(content);
    }
    char *uart = read_file("m1_csrc/m1_gpio_uart.c");
    assert_contains(uart, "ok_circle_8x8, \"Mode\"");
    free(uart);
    char *quick = read_file("m1_csrc/m1_ir_quick_remote.c");
    assert_contains(quick, "ok_circle_8x8, \"Found\"");
    free(quick);
}

void test_selection_lists_have_no_ok_bars(void)
{
    char *custom = read_file("m1_csrc/m1_ir_custom.c");
    assert_absent(custom, "ok_circle_8x8, \"OK\"");
    assert_absent(custom, "IR_CUST_VISIBLE");
    free(custom);
    char *device = read_file("m1_csrc/m1_802154.c");
    assert_absent(device, "ok_circle_8x8, \"OK\"");
    free(device);
    char *pager = read_file("m1_csrc/m1_subghz_scene_restaurant_pager.c");
    assert_absent(pager, "OK:SELECT");
    assert_absent(pager, "UP/DN BACK");
    assert_absent(pager, "BACK:CANCEL");
    assert_contains(pager, "m1_submenu_draw(&state->action_model");
    assert_contains(pager, "m1_submenu_draw(&state->settings_model");
    free(pager);
}

void test_specialized_action_screens_use_standard_bars(void)
{
    const char *games[] = {
        "m1_csrc/game_snake.c",
        "m1_csrc/game_tetris.c",
        "m1_csrc/game_trex.c",
        "m1_csrc/game_rps.c",
        "m1_csrc/game_dice.c",
        "m1_csrc/game_pong.c",
        "m1_csrc/game_2048.c",
        "m1_csrc/game_queens.c",
    };
    for (size_t i = 0; i < sizeof(games) / sizeof(games[0]); ++i)
    {
        char *content = read_file(games[i]);
        assert_contains(content, "m1_button_bar_draw");
        free(content);
    }
    char *success = read_file("m1_csrc/m1_subghz_scene_save_success.c");
    assert_absent(success, "subghz_button_bar_draw");
    free(success);
    char *analyzer = read_file("m1_csrc/m1_subghz_scene_proto_pirate_analyzer.c");
    assert_contains(analyzer, "m1_button_bar_draw");
    assert_absent(analyzer, "OK:Resume LR:Trg");
    assert_absent(analyzer, "OK:Pause LR:Trg");
    free(analyzer);
}

void test_legacy_action_hints_are_replaced(void)
{
    const char *files[] = {
        "m1_csrc/m1_field_detect.c",
        "m1_csrc/m1_flipper_integration.c",
        "m1_csrc/m1_diag.c",
        "m1_csrc/signal_gen.c",
        "m1_csrc/m1_sub_ghz.c",
        "m1_csrc/game_queens.c",
        "m1_csrc/m1_subghz_scene_more_raw.c",
    };
    for (size_t i = 0; i < sizeof(files) / sizeof(files[0]); ++i)
    {
        char *content = read_file(files[i]);
        assert_absent(content, "BACK to exit");
        assert_absent(content, "Press BACK");
        assert_absent(content, "BACK: exit");
        assert_absent(content, "BACK:Stop");
        assert_absent(content, "UD:chr LR:pos OK:save");
        assert_absent(content, "L/R:Band OK");
        assert_absent(content, "L/R:Thr OK");
        assert_absent(content, "UP/DN:Proto OK:Start");
        assert_absent(content, "\"BACK\"");
        free(content);
    }
}

void test_subghz_hex_editors_use_standard_bars(void)
{
    const char *files[] = {
        "m1_csrc/m1_subghz_scene_proto_pirate_emulate.c",
        "m1_csrc/m1_subghz_scene_set_button.c",
        "m1_csrc/m1_subghz_scene_set_counter.c",
        "m1_csrc/m1_subghz_scene_set_key.c",
        "m1_csrc/m1_subghz_scene_set_serial.c",
    };
    for (size_t i = 0; i < sizeof(files) / sizeof(files[0]); ++i)
    {
        char *content = read_file(files[i]);
        assert_contains(content, "subghz_button_bar_draw");
        assert_absent(content, "L/R:Move OK");
        free(content);
    }
}

void test_review_layout_reflow_and_feedback(void)
{
    char *subghz = read_file("m1_csrc/m1_sub_ghz.c");
    assert_contains(subghz, "#define FREQAN_BAR_Y         32");
    assert_contains(subghz, "u8g2_DrawStr(&m1_u8g2, 0, 30, str);");
    assert_contains(subghz, "u8g2_DrawStr(&m1_u8g2, 0, 51, str);");
    assert_contains(subghz, "u8g2_DrawStr(&m1_u8g2, 0, 51, info_str);");
    assert_contains(subghz, "u8g2_DrawStr(&m1_u8g2, 90, 51, info_str);");
    free(subghz);

    char *analyzer = read_file("m1_csrc/m1_subghz_scene_proto_pirate_analyzer.c");
    assert_contains(analyzer, "#define RA_BAR_Y    29");
    assert_contains(analyzer, "u8g2_DrawStr(&m1_u8g2, 2, 43, line);");
    assert_contains(analyzer, "u8g2_DrawStr(&m1_u8g2, 2, 51, line);");
    free(analyzer);

    char *download = read_file("m1_csrc/m1_esp32_fw_download.c");
    assert_contains(download, "u8g2_DrawStr(&m1_u8g2, 4, 48, \"Flash via Image File\");");
    assert_contains(download, "ok_circle_8x8, \"Next\"");
    free(download);

    char *pager = read_file("m1_csrc/m1_subghz_scene_restaurant_pager.c");
    assert_contains(pager, "state->status_pending = (status != NULL");
    assert_contains(pager, "m1_message_box(&m1_u8g2, screen, state->status");
    free(pager);
}

void test_excluded_scenes_follow_visual_standards(void)
{
    char *bind = read_file("m1_csrc/m1_subghz_scene_bind_wizard.c");
    assert_absent(bind, "BACK=Retry");
    assert_absent(bind, "BACK=Exit");
    assert_absent(bind, "OK=Send");
    assert_absent(bind, "OK=Next");
    assert_contains(bind, "subghz_button_bar_draw");
    assert_contains(bind, "u8g2_DrawFrame(&m1_u8g2, 4, 42, 120, 5);");
    free(bind);

    char *remote = read_file("m1_csrc/m1_subghz_scene_remote.c");
    assert_absent(remote, "OK = browse .rem");
    assert_absent(remote, "Press to send");
    assert_contains(remote, "ok_circle_8x8, \"Browse\"");
    free(remote);

    char *update = read_file("m1_csrc/m1_fw_update.c");
    assert_absent(update, "OK=Swap");
    assert_absent(update, "BACK=Cancel");
    assert_contains(update, "ok_circle_8x8, \"Swap\"");
    free(update);

    char *transmitter = read_file("m1_csrc/m1_subghz_scene_transmitter.c");
    assert_contains(transmitter, "u8g2_DrawHLine(&m1_u8g2, 0, 10, 128);");
    assert_contains(transmitter, "ok_circle_8x8, \"Send\"");
    free(transmitter);

    char *receiver = read_file("m1_csrc/m1_subghz_scene_receiver_info.c");
    assert_contains(receiver, "u8g2_DrawHLine(&m1_u8g2, 0, 12, M1_LCD_DISPLAY_WIDTH);");
    assert_absent(receiver, "\"Back\"");
    free(receiver);

    char *settings = read_file("m1_csrc/m1_subghz_scene_signal_settings.c");
    assert_contains(settings, "u8g2_DrawStr(&m1_u8g2, 2, 10, \"Signal Settings\");");
    assert_absent(settings, "\"Back\"");
    free(settings);

    char *nfc = read_file("m1_csrc/m1_nfc.c");
    assert_absent(nfc, "BACK=exit");
    assert_contains(nfc, "ok_circle_8x8, \"Options\"");
    free(nfc);
}

void test_caption_and_countdown_reflow(void)
{
    char *download = read_file("m1_csrc/m1_fw_download.c");
    assert_contains(download, "ok_circle_8x8, \"Fetch\"");
    assert_contains(download, "ok_circle_8x8, \"Next\"");
    free(download);
    char *esp32 = read_file("m1_csrc/m1_esp32_fw_download.c");
    assert_contains(esp32, "ok_circle_8x8, \"Fetch\"");
    free(esp32);
    char *game = read_file("m1_csrc/game_2048.c");
    assert_contains(game, "ok_circle_8x8, \"Next\"");
    free(game);
    char *bind = read_file("m1_csrc/m1_subghz_scene_bind_wizard.c");
    assert_contains(bind, "m1_draw_text_box(&m1_u8g2, 2, 11, 124");
    assert_contains(bind, "u8g2_DrawStr(&m1_u8g2, 4, 41, cd);");
    assert_contains(bind, "u8g2_DrawFrame(&m1_u8g2, 4, 42, 120, 5);");
    free(bind);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_selectors_use_shared_submenu_model);
    RUN_TEST(test_headers_are_not_inverted);
    RUN_TEST(test_back_hints_are_removed);
    RUN_TEST(test_ok_actions_use_center_column);
    RUN_TEST(test_selection_lists_have_no_ok_bars);
    RUN_TEST(test_specialized_action_screens_use_standard_bars);
    RUN_TEST(test_legacy_action_hints_are_replaced);
    RUN_TEST(test_subghz_hex_editors_use_standard_bars);
    RUN_TEST(test_review_layout_reflow_and_feedback);
    RUN_TEST(test_excluded_scenes_follow_visual_standards);
    RUN_TEST(test_caption_and_countdown_reflow);
    return UNITY_END();
}
