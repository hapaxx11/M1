/* See COPYING.txt for license details. */

/*
 * test_ir_ux_polish.c
 *
 * Source-level regression checks for IR UX polish:
 *   1. Quick-remote forces M1_ORIENT_REMOTE (portrait) on entry; restores on exit.
 *   2. Long-press LEFT/RIGHT handlers removed (replaced by OK long-press menu).
 *   3. Dashboard "Remote Mode" / "Toggle Remote Mode" item removed.
 *   4. Brute-force error path uses async dismiss instead of vTaskDelay.
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
/* Quick-remote: portrait orientation forced (IR must face forward)    */
/* ------------------------------------------------------------------ */

void test_quick_remote_forced_orientation_and_restore(void)
{
    char *c = read_file("m1_csrc/m1_ir_quick_remote.c");
    /* Must force portrait orientation on entry */
    assert_contains(c, "settings_apply_orientation(M1_ORIENT_REMOTE)");
    /* Must save and restore the caller's orientation */
    assert_contains(c, "saved_orient");
    assert_contains(c, "settings_apply_orientation(saved_orient)");
    /* Must include m1_settings.h for settings_apply_orientation() */
    assert_contains(c, "m1_settings.h");
    free(c);
}

/* ------------------------------------------------------------------ */
/* Long-press LEFT/RIGHT removed from quick-remote                     */
/* ------------------------------------------------------------------ */

void test_quick_remote_no_long_press_left_right(void)
{
    char *c = read_file("m1_csrc/m1_ir_quick_remote.c");
    /* Old long-press hints should be gone */
    assert_absent(c, "H<:Scan");
    assert_absent(c, "H>:File");
    /* New bottom bar hints */
    assert_contains(c, "OK:Send");
    assert_contains(c, "Hold:Menu");
    free(c);
}

/* ------------------------------------------------------------------ */
/* Long-press OK popup menu present                                    */
/* ------------------------------------------------------------------ */

void test_quick_remote_has_ok_long_press_menu(void)
{
    char *c = read_file("m1_csrc/m1_ir_quick_remote.c");
    assert_contains(c, "ir_grid_action_menu");
    assert_contains(c, "IR_GRID_ACT_SCAN");
    assert_contains(c, "IR_GRID_ACT_CHANGE");
    assert_contains(c, "BUTTON_EVENT_LCLICK");
    assert_contains(c, "Power Scan");
    assert_contains(c, "Change Device");
    free(c);
}

/* ------------------------------------------------------------------ */
/* Dashboard: "Remote Mode" toggle removed                             */
/* ------------------------------------------------------------------ */

void test_dashboard_no_remote_mode_toggle(void)
{
    char *c = read_file("m1_csrc/m1_ir_universal.c");
    /* The literal "Remote Mode" string used as a dashboard menu item is gone */
    assert_absent(c, "\"Remote Mode\"");
    assert_absent(c, "\"Normal Mode\"");
    /* The old DASHBOARD_ITEM_COUNT of 13 is now 12 */
    assert_contains(c, "DASHBOARD_ITEM_COUNT  12");
    assert_absent(c, "DASHBOARD_ITEM_COUNT  13");
    free(c);
}

void test_dashboard_no_orientation_save_restore(void)
{
    char *c = read_file("m1_csrc/m1_ir_universal.c");
    /* dashboard_screen() no longer saves/restores orientation */
    assert_absent(c, "saved_orient");
    assert_absent(c, "browse_saved_orient");
    assert_absent(c, "build_saved_orient");
    free(c);
}

/* ------------------------------------------------------------------ */
/* Brute-force scan: async dismiss instead of vTaskDelay               */
/* ------------------------------------------------------------------ */

void test_brute_force_no_vtaskdelay_error(void)
{
    char *c = read_file("m1_csrc/m1_ir_quick_remote.c");
    /* The old vTaskDelay(pdMS_TO_TICKS(1500)) error dismissal is gone */
    assert_absent(c, "vTaskDelay(pdMS_TO_TICKS(1500))");
    free(c);
}

/* ------------------------------------------------------------------ */
/* Grid: sidebar layout replaces off-screen title bar                  */
/* ------------------------------------------------------------------ */

void test_quick_remote_sidebar_layout(void)
{
    char *c = read_file("m1_csrc/m1_ir_quick_remote.c");

    /* New sidebar constant must be defined */
    assert_contains(c, "GRID_SIDEBAR_W");

    /* Old top-title-bar constant must be gone */
    assert_absent(c, "GRID_TOP_Y");

    /* short_label field present in struct and layouts */
    assert_contains(c, "short_label");

    /* Category abbreviations for each layout entry */
    assert_contains(c, "\"TV\"");
    assert_contains(c, "\"AC\"");
    assert_contains(c, "\"AUD\"");
    assert_contains(c, "\"PRJ\"");
    assert_contains(c, "\"FAN\"");
    assert_contains(c, "\"LED\"");

    /* Old truncated device-name title-bar rendering is gone */
    assert_absent(c, "GRID_TOP_Y - 1");
    assert_absent(c, "TEXT_ALIGN_CENTER");

    free(c);
}

/* ------------------------------------------------------------------ */
/* Grid: unsigned-wrap centering guard present                         */
/* ------------------------------------------------------------------ */

void test_grid_label_no_unsigned_wrap_centering(void)
{
    char *c = read_file("m1_csrc/m1_ir_quick_remote.c");
    /* The tw >= cell_w guard must be present so centering never wraps */
    assert_contains(c, "tw >= (u8g2_uint_t)cell_w");
    /* Left-align branch must assign x + 1u (not centred) */
    assert_contains(c, "x + 1u");
    /* Centering formula must only run when tw < cell_w */
    assert_contains(c, "(cell_w - tw) / 2u");
    free(c);
}

/* ------------------------------------------------------------------ */
/* Unity entry point                                                   */
/* ------------------------------------------------------------------ */

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_quick_remote_forced_orientation_and_restore);
    RUN_TEST(test_quick_remote_no_long_press_left_right);
    RUN_TEST(test_quick_remote_has_ok_long_press_menu);
    RUN_TEST(test_dashboard_no_remote_mode_toggle);
    RUN_TEST(test_dashboard_no_orientation_save_restore);
    RUN_TEST(test_brute_force_no_vtaskdelay_error);
    RUN_TEST(test_quick_remote_sidebar_layout);
    RUN_TEST(test_grid_label_no_unsigned_wrap_centering);
    return UNITY_END();
}
