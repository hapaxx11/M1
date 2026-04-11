/**
 * @file   test_menu_layout.c
 * @brief  Host-side unit tests for menu layout helper functions.
 *
 * Tests m1_menu_item_h(), m1_menu_max_visible(), and the M1_MENU_VIS() macro
 * in Small, Medium, and Large modes.
 *
 * Constants and the M1_MENU_VIS() macro come from the production m1_scene.h
 * header so the tests stay in sync with any constant changes.  The three
 * helper functions are defined locally because m1_scene.c has heavy
 * HAL/RTOS/display dependencies that cannot be linked on the host.
 */

#include "unity.h"
#include <stdint.h>

/* ---------- Minimal stubs so m1_scene.h can be included ---------- */

/* Dummy font arrays — stand in for the u8g2 font pointers */
static const uint8_t dummy_font_small[]  = { 0 };
static const uint8_t dummy_font_medium[] = { 0 };
static const uint8_t dummy_font_large[]  = { 0 };
#define u8g2_font_NokiaSmallPlain_tf  dummy_font_small
#define u8g2_font_spleen5x8_mf        dummy_font_medium
#define u8g2_font_nine_by_five_nbp_tf  dummy_font_large

/* Display macros referenced by m1_menu_font() */
#define M1_DISP_SUB_MENU_FONT_N    u8g2_font_NokiaSmallPlain_tf
#define M1_DISP_FUNC_MENU_FONT_N   u8g2_font_spleen5x8_mf
#define M1_DISP_FUNC_MENU_FONT_N2  u8g2_font_nine_by_five_nbp_tf

/* Include the production header — provides all layout constants,
 * function declarations, and the M1_MENU_VIS() macro.              */
#include "m1_scene.h"

/* Global setting — defined in m1_system.c on target */
uint8_t m1_menu_style = 0;

/* --- Function implementations (matching m1_scene.c logic) ---
 * We cannot link m1_scene.c on the host, so we replicate the three
 * pure-logic helpers here.  The constants they reference come from
 * the real m1_scene.h above.                                       */

uint8_t m1_menu_item_h(void)
{
    switch (m1_menu_style)
    {
    case 1:  return M1_MENU_ITEM_H_MEDIUM;
    case 2:  return M1_MENU_ITEM_H_LARGE;
    default: return M1_MENU_ITEM_H_SMALL;
    }
}

uint8_t m1_menu_max_visible(void)
{
    return M1_MENU_AREA_H / m1_menu_item_h();
}

const uint8_t *m1_menu_font(void)
{
    switch (m1_menu_style)
    {
    case 1:  return M1_DISP_FUNC_MENU_FONT_N;
    case 2:  return M1_DISP_FUNC_MENU_FONT_N2;
    default: return M1_DISP_SUB_MENU_FONT_N;
    }
}

/* ================================================================ */
/* Tests                                                            */
/* ================================================================ */

void setUp(void) {}
void tearDown(void) {}

/* --- m1_menu_item_h() --- */

void test_item_h_small(void)
{
    m1_menu_style = 0;
    TEST_ASSERT_EQUAL_UINT8(8, m1_menu_item_h());
}

void test_item_h_medium(void)
{
    m1_menu_style = 1;
    TEST_ASSERT_EQUAL_UINT8(10, m1_menu_item_h());
}

void test_item_h_large(void)
{
    m1_menu_style = 2;
    TEST_ASSERT_EQUAL_UINT8(13, m1_menu_item_h());
}

/* --- m1_menu_max_visible() --- */

void test_max_visible_small(void)
{
    m1_menu_style = 0;
    /* 52 / 8 = 6 */
    TEST_ASSERT_EQUAL_UINT8(6, m1_menu_max_visible());
}

void test_max_visible_medium(void)
{
    m1_menu_style = 1;
    /* 52 / 10 = 5 */
    TEST_ASSERT_EQUAL_UINT8(5, m1_menu_max_visible());
}

void test_max_visible_large(void)
{
    m1_menu_style = 2;
    /* 52 / 13 = 4 */
    TEST_ASSERT_EQUAL_UINT8(4, m1_menu_max_visible());
}

/* --- M1_MENU_VIS() macro --- */

void test_vis_fewer_than_max_small(void)
{
    m1_menu_style = 0;
    /* 3 items < 6 max → 3 */
    TEST_ASSERT_EQUAL_UINT8(3, M1_MENU_VIS(3));
}

void test_vis_equal_to_max_small(void)
{
    m1_menu_style = 0;
    /* 6 items == 6 max → 6 */
    TEST_ASSERT_EQUAL_UINT8(6, M1_MENU_VIS(6));
}

void test_vis_more_than_max_small(void)
{
    m1_menu_style = 0;
    /* 11 items > 6 max → 6 */
    TEST_ASSERT_EQUAL_UINT8(6, M1_MENU_VIS(11));
}

void test_vis_fewer_than_max_medium(void)
{
    m1_menu_style = 1;
    /* 3 items < 5 max → 3 */
    TEST_ASSERT_EQUAL_UINT8(3, M1_MENU_VIS(3));
}

void test_vis_equal_to_max_medium(void)
{
    m1_menu_style = 1;
    /* 5 items == 5 max → 5 */
    TEST_ASSERT_EQUAL_UINT8(5, M1_MENU_VIS(5));
}

void test_vis_more_than_max_medium(void)
{
    m1_menu_style = 1;
    /* 11 items > 5 max → 5 */
    TEST_ASSERT_EQUAL_UINT8(5, M1_MENU_VIS(11));
}

void test_vis_fewer_than_max_large(void)
{
    m1_menu_style = 2;
    /* 3 items < 4 max → 3 */
    TEST_ASSERT_EQUAL_UINT8(3, M1_MENU_VIS(3));
}

void test_vis_equal_to_max_large(void)
{
    m1_menu_style = 2;
    /* 4 items == 4 max → 4 */
    TEST_ASSERT_EQUAL_UINT8(4, M1_MENU_VIS(4));
}

void test_vis_more_than_max_large(void)
{
    m1_menu_style = 2;
    /* 11 items > 4 max → 4 */
    TEST_ASSERT_EQUAL_UINT8(4, M1_MENU_VIS(11));
}

/* --- m1_menu_font() --- */

void test_font_small(void)
{
    m1_menu_style = 0;
    TEST_ASSERT_EQUAL_PTR(dummy_font_small, m1_menu_font());
}

void test_font_medium(void)
{
    m1_menu_style = 1;
    TEST_ASSERT_EQUAL_PTR(dummy_font_medium, m1_menu_font());
}

void test_font_large(void)
{
    m1_menu_style = 2;
    TEST_ASSERT_EQUAL_PTR(dummy_font_large, m1_menu_font());
}

/* --- Edge case: unknown style value defaults to Small --- */

void test_unknown_style_gives_small(void)
{
    m1_menu_style = 255;
    TEST_ASSERT_EQUAL_UINT8(8, m1_menu_item_h());
    TEST_ASSERT_EQUAL_UINT8(6, m1_menu_max_visible());
    TEST_ASSERT_EQUAL_PTR(dummy_font_small, m1_menu_font());
}

/* --- Area calculations --- */

void test_area_fits_all_items_small(void)
{
    m1_menu_style = 0;
    /* 6 items × 8px = 48px ≤ 52px available */
    uint8_t vis = M1_MENU_VIS(6);
    TEST_ASSERT_TRUE(vis * m1_menu_item_h() <= M1_MENU_AREA_H);
}

void test_area_fits_all_items_medium(void)
{
    m1_menu_style = 1;
    /* 5 items × 10px = 50px ≤ 52px available */
    uint8_t vis = M1_MENU_VIS(5);
    TEST_ASSERT_TRUE(vis * m1_menu_item_h() <= M1_MENU_AREA_H);
}

void test_area_fits_all_items_large(void)
{
    m1_menu_style = 2;
    /* 4 items × 13px = 52px ≤ 52px available */
    uint8_t vis = M1_MENU_VIS(4);
    TEST_ASSERT_TRUE(vis * m1_menu_item_h() <= M1_MENU_AREA_H);
}

void test_area_scrolling_needed_medium(void)
{
    m1_menu_style = 1;
    /* 6 items need scrolling in medium mode (only 5 visible) */
    TEST_ASSERT_EQUAL_UINT8(5, M1_MENU_VIS(6));
    TEST_ASSERT_TRUE(M1_MENU_VIS(6) * m1_menu_item_h() <= M1_MENU_AREA_H);
}

void test_area_scrolling_needed_large(void)
{
    m1_menu_style = 2;
    /* 5 items need scrolling in large mode (only 4 visible) */
    TEST_ASSERT_EQUAL_UINT8(4, M1_MENU_VIS(5));
    TEST_ASSERT_TRUE(M1_MENU_VIS(5) * m1_menu_item_h() <= M1_MENU_AREA_H);
}

/* ================================================================ */

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_item_h_small);
    RUN_TEST(test_item_h_medium);
    RUN_TEST(test_item_h_large);
    RUN_TEST(test_max_visible_small);
    RUN_TEST(test_max_visible_medium);
    RUN_TEST(test_max_visible_large);

    RUN_TEST(test_vis_fewer_than_max_small);
    RUN_TEST(test_vis_equal_to_max_small);
    RUN_TEST(test_vis_more_than_max_small);
    RUN_TEST(test_vis_fewer_than_max_medium);
    RUN_TEST(test_vis_equal_to_max_medium);
    RUN_TEST(test_vis_more_than_max_medium);
    RUN_TEST(test_vis_fewer_than_max_large);
    RUN_TEST(test_vis_equal_to_max_large);
    RUN_TEST(test_vis_more_than_max_large);

    RUN_TEST(test_font_small);
    RUN_TEST(test_font_medium);
    RUN_TEST(test_font_large);
    RUN_TEST(test_unknown_style_gives_small);

    RUN_TEST(test_area_fits_all_items_small);
    RUN_TEST(test_area_fits_all_items_medium);
    RUN_TEST(test_area_fits_all_items_large);
    RUN_TEST(test_area_scrolling_needed_medium);
    RUN_TEST(test_area_scrolling_needed_large);

    return UNITY_END();
}
