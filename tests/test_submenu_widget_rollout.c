/* See COPYING.txt for license details. */

/*
 * test_submenu_widget_rollout.c
 *
 * Source-level regression checks for Phase E: shared submenu-widget rollout.
 * Verifies that all BT, Settings, NFC, and WiFi scene files that were
 * migrated off raw sel/scroll byte pairs now use m1_submenu_event() +
 * m1_submenu_draw() and no longer call m1_scene_menu_event().
 */

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

    buf = (char *)malloc((size_t)size + 1U);
    TEST_ASSERT_NOT_NULL(buf);
    TEST_ASSERT_EQUAL_size_t((size_t)size, fread(buf, 1U, (size_t)size, fp));
    buf[size] = '\0';
    fclose(fp);
    return buf;
}

static void assert_contains(const char *content, const char *needle)
{
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(content, needle), needle);
}

static void assert_absent(const char *content, const char *needle)
{
    TEST_ASSERT_NULL_MESSAGE(strstr(content, needle), needle);
}

/* ------------------------------------------------------------------ */
/* m1_submenu widget API surface                                       */
/* ------------------------------------------------------------------ */

void test_m1_submenu_declares_event_and_draw(void)
{
    char *h = read_file("m1_csrc/m1_submenu.h");
    assert_contains(h, "m1_submenu_event");
    assert_contains(h, "m1_submenu_draw");
    free(h);
}

void test_m1_submenu_implements_event_and_draw(void)
{
    char *c = read_file("m1_csrc/m1_submenu.c");
    assert_contains(c, "m1_submenu_event");
    assert_contains(c, "m1_submenu_draw");
    free(c);
}

/* ------------------------------------------------------------------ */
/* BT scene files                                                      */
/* ------------------------------------------------------------------ */

void test_bt_scene_menu_uses_widget(void)
{
    char *c = read_file("m1_csrc/m1_bt_scene_menu.c");
    assert_contains(c, "m1_submenu.h");
    assert_contains(c, "m1_submenu_event");
    assert_contains(c, "m1_submenu_draw");
    assert_absent(c, "m1_scene_menu_event");
    free(c);
}

void test_bt_scene_sniff_uses_widget(void)
{
    char *c = read_file("m1_csrc/m1_bt_scene_sniff.c");
    assert_contains(c, "m1_submenu.h");
    assert_contains(c, "m1_submenu_event");
    assert_contains(c, "m1_submenu_draw");
    assert_absent(c, "m1_scene_menu_event");
    free(c);
}

void test_bt_scene_spam_uses_widget(void)
{
    char *c = read_file("m1_csrc/m1_bt_scene_spam.c");
    assert_contains(c, "m1_submenu.h");
    assert_contains(c, "m1_submenu_event");
    assert_contains(c, "m1_submenu_draw");
    assert_absent(c, "m1_scene_menu_event");
    free(c);
}

/* ------------------------------------------------------------------ */
/* Settings scene files                                                */
/* ------------------------------------------------------------------ */

void test_settings_scene_menu_uses_widget(void)
{
    char *c = read_file("m1_csrc/m1_settings_scene_menu.c");
    assert_contains(c, "m1_submenu.h");
    assert_contains(c, "m1_submenu_event");
    assert_contains(c, "m1_submenu_draw");
    assert_absent(c, "m1_scene_menu_event");
    free(c);
}

void test_settings_scene_storage_uses_widget(void)
{
    char *c = read_file("m1_csrc/m1_settings_scene_storage.c");
    assert_contains(c, "m1_submenu.h");
    assert_contains(c, "m1_submenu_event");
    assert_contains(c, "m1_submenu_draw");
    assert_absent(c, "m1_scene_menu_event");
    free(c);
}

void test_settings_scene_power_uses_widget(void)
{
    char *c = read_file("m1_csrc/m1_settings_scene_power.c");
    assert_contains(c, "m1_submenu.h");
    assert_contains(c, "m1_submenu_event");
    assert_contains(c, "m1_submenu_draw");
    assert_absent(c, "m1_scene_menu_event");
    free(c);
}

void test_settings_scene_fw_uses_widget(void)
{
    char *c = read_file("m1_csrc/m1_settings_scene_fw.c");
    assert_contains(c, "m1_submenu.h");
    assert_contains(c, "m1_submenu_event");
    assert_contains(c, "m1_submenu_draw");
    assert_absent(c, "m1_scene_menu_event");
    free(c);
}

void test_settings_scene_esp32_uses_widget(void)
{
    char *c = read_file("m1_csrc/m1_settings_scene_esp32.c");
    assert_contains(c, "m1_submenu.h");
    assert_contains(c, "m1_submenu_event");
    assert_contains(c, "m1_submenu_draw");
    assert_absent(c, "m1_scene_menu_event");
    free(c);
}

/* ------------------------------------------------------------------ */
/* NFC scene                                                           */
/* ------------------------------------------------------------------ */

void test_nfc_scene_uses_widget(void)
{
    char *c = read_file("m1_csrc/m1_nfc_scene.c");
    assert_contains(c, "m1_submenu.h");
    assert_contains(c, "m1_submenu_event");
    assert_contains(c, "m1_submenu_draw");
    assert_absent(c, "m1_scene_menu_event");
    free(c);
}

/* ------------------------------------------------------------------ */
/* WiFi scene                                                          */
/* ------------------------------------------------------------------ */

void test_wifi_scene_uses_widget(void)
{
    /* After Phase D the menu + submenu widget live in the per-group split
     * file; m1_wifi_scene_menu.c is the canonical file for the top menu. */
    char *c = read_file("m1_csrc/m1_wifi_scene_menu.c");
    assert_contains(c, "m1_submenu.h");
    assert_contains(c, "m1_submenu_event");
    assert_contains(c, "m1_submenu_draw");
    assert_absent(c, "m1_scene_menu_event");
    free(c);
}

/* ------------------------------------------------------------------ */
/* Regression: SubGHz scenes must still use the widget too            */
/* ------------------------------------------------------------------ */

void test_subghz_scene_menu_uses_widget(void)
{
    char *c = read_file("m1_csrc/m1_subghz_scene_menu.c");
    assert_contains(c, "m1_submenu_draw");
    assert_absent(c, "m1_scene_menu_event");
    free(c);
}

/* ------------------------------------------------------------------ */
/* Regression: on_enter must NOT unconditionally reset model           */
/*                                                                     */
/* PR #547 introduced a bug where every on_enter called                */
/* subghz_submenu_model_init() unconditionally, resetting the user's   */
/* menu selection when returning from a child scene via scene_pop().   */
/* The fix guards the init with `if (model.item_count == 0)` so that   */
/* the model is only initialised on first entry.                       */
/* ------------------------------------------------------------------ */

static void assert_guarded_init(const char *relpath)
{
    char *c = read_file(relpath);

    const char *init = strstr(c, "subghz_submenu_model_init");
    TEST_ASSERT_NOT_NULL_MESSAGE(init, relpath);

    /* Look for the guard within a small window before the init call to avoid false positives. */
    const char *win_start = (init - c > 200) ? (init - 200) : c;
    const char *guard = strstr(win_start, "item_count");
    TEST_ASSERT_NOT_NULL_MESSAGE(guard, relpath);
    TEST_ASSERT_TRUE(guard < init);

    const char *eq = strstr(win_start, "== 0");
    if (!eq) eq = strstr(win_start, "==0");
    TEST_ASSERT_NOT_NULL_MESSAGE(eq, relpath);
    TEST_ASSERT_TRUE(guard < eq);
    TEST_ASSERT_TRUE(eq < init);

    free(c);
}

void test_bt_menu_init_guarded(void)
{
    assert_guarded_init("m1_csrc/m1_bt_scene_menu.c");
}

void test_bt_sniff_init_guarded(void)
{
    assert_guarded_init("m1_csrc/m1_bt_scene_sniff.c");
}

void test_bt_spam_init_guarded(void)
{
    assert_guarded_init("m1_csrc/m1_bt_scene_spam.c");
}

void test_settings_menu_init_guarded(void)
{
    assert_guarded_init("m1_csrc/m1_settings_scene_menu.c");
}

void test_settings_storage_init_guarded(void)
{
    assert_guarded_init("m1_csrc/m1_settings_scene_storage.c");
}

void test_settings_power_init_guarded(void)
{
    assert_guarded_init("m1_csrc/m1_settings_scene_power.c");
}

void test_settings_fw_init_guarded(void)
{
    assert_guarded_init("m1_csrc/m1_settings_scene_fw.c");
}

void test_settings_esp32_init_guarded(void)
{
    assert_guarded_init("m1_csrc/m1_settings_scene_esp32.c");
}

void test_nfc_menu_init_guarded(void)
{
    assert_guarded_init("m1_csrc/m1_nfc_scene.c");
}

void test_wifi_menu_init_guarded(void)
{
    assert_guarded_init("m1_csrc/m1_wifi_scene_menu.c");
}

void test_wifi_sniff_init_guarded(void)
{
    assert_guarded_init("m1_csrc/m1_wifi_scene_sniff.c");
}

void test_wifi_attack_init_guarded(void)
{
    assert_guarded_init("m1_csrc/m1_wifi_scene_attack.c");
}

void test_wifi_general_init_guarded(void)
{
    assert_guarded_init("m1_csrc/m1_wifi_scene_general.c");
}

void test_wifi_net_init_guarded(void)
{
    assert_guarded_init("m1_csrc/m1_wifi_scene_net.c");
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_m1_submenu_declares_event_and_draw);
    RUN_TEST(test_m1_submenu_implements_event_and_draw);
    RUN_TEST(test_bt_scene_menu_uses_widget);
    RUN_TEST(test_bt_scene_sniff_uses_widget);
    RUN_TEST(test_bt_scene_spam_uses_widget);
    RUN_TEST(test_settings_scene_menu_uses_widget);
    RUN_TEST(test_settings_scene_storage_uses_widget);
    RUN_TEST(test_settings_scene_power_uses_widget);
    RUN_TEST(test_settings_scene_fw_uses_widget);
    RUN_TEST(test_settings_scene_esp32_uses_widget);
    RUN_TEST(test_nfc_scene_uses_widget);
    RUN_TEST(test_wifi_scene_uses_widget);
    RUN_TEST(test_subghz_scene_menu_uses_widget);
    RUN_TEST(test_bt_menu_init_guarded);
    RUN_TEST(test_bt_sniff_init_guarded);
    RUN_TEST(test_bt_spam_init_guarded);
    RUN_TEST(test_settings_menu_init_guarded);
    RUN_TEST(test_settings_storage_init_guarded);
    RUN_TEST(test_settings_power_init_guarded);
    RUN_TEST(test_settings_fw_init_guarded);
    RUN_TEST(test_settings_esp32_init_guarded);
    RUN_TEST(test_nfc_menu_init_guarded);
    RUN_TEST(test_wifi_menu_init_guarded);
    RUN_TEST(test_wifi_sniff_init_guarded);
    RUN_TEST(test_wifi_attack_init_guarded);
    RUN_TEST(test_wifi_general_init_guarded);
    RUN_TEST(test_wifi_net_init_guarded);
    return UNITY_END();
}
