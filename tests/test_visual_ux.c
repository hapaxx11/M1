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
    return UNITY_END();
}
