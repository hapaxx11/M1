/* See COPYING.txt for license details. */

/**
 * @file   test_subghz_submenu_model.c
 * @brief  Host-side unit tests for the reusable scrollable-list model.
 */

#include "unity.h"
#include "subghz_submenu_model.h"
#include <string.h>

void setUp(void) { }
void tearDown(void) { }

/*============================================================================*/
/* Init                                                                       */
/*============================================================================*/

static void test_init_null_is_safe(void)
{
    subghz_submenu_model_init(NULL, 10, 4);  /* must not crash */
}

static void test_init_sets_defaults(void)
{
    subghz_submenu_model_t m;
    memset(&m, 0xAB, sizeof(m));
    subghz_submenu_model_init(&m, 13, 4);
    TEST_ASSERT_EQUAL_UINT8(13, m.item_count);
    TEST_ASSERT_EQUAL_UINT8(4,  m.visible_count);
    TEST_ASSERT_EQUAL_UINT8(0,  m.selected);
    TEST_ASSERT_EQUAL_UINT8(0,  m.scroll_offset);
}

static void test_init_clamps_visible_zero_to_one(void)
{
    subghz_submenu_model_t m;
    subghz_submenu_model_init(&m, 5, 0);
    TEST_ASSERT_EQUAL_UINT8(1, m.visible_count);
}

static void test_init_empty_list_keeps_defaults(void)
{
    subghz_submenu_model_t m;
    subghz_submenu_model_init(&m, 0, 4);
    TEST_ASSERT_EQUAL_UINT8(0, m.item_count);
    TEST_ASSERT_EQUAL_UINT8(0, m.selected);
    TEST_ASSERT_EQUAL_UINT8(0, m.scroll_offset);
}

/*============================================================================*/
/* Down navigation                                                            */
/*============================================================================*/

static void test_down_advances_selection_within_window(void)
{
    subghz_submenu_model_t m;
    subghz_submenu_model_init(&m, 13, 4);
    subghz_submenu_model_down(&m);
    TEST_ASSERT_EQUAL_UINT8(1, m.selected);
    TEST_ASSERT_EQUAL_UINT8(0, m.scroll_offset);
    subghz_submenu_model_down(&m);
    subghz_submenu_model_down(&m);
    TEST_ASSERT_EQUAL_UINT8(3, m.selected);
    TEST_ASSERT_EQUAL_UINT8(0, m.scroll_offset);
}

static void test_down_scrolls_window_when_leaving_visible_area(void)
{
    subghz_submenu_model_t m;
    subghz_submenu_model_init(&m, 13, 4);
    /* Advance 4 times: 0→1→2→3→4. At idx=4 the window must scroll. */
    for (int i = 0; i < 4; ++i) subghz_submenu_model_down(&m);
    TEST_ASSERT_EQUAL_UINT8(4, m.selected);
    TEST_ASSERT_EQUAL_UINT8(1, m.scroll_offset);
}

static void test_down_wraps_at_end_and_resets_scroll(void)
{
    subghz_submenu_model_t m;
    subghz_submenu_model_init(&m, 13, 4);
    for (int i = 0; i < 12; ++i) subghz_submenu_model_down(&m);
    TEST_ASSERT_EQUAL_UINT8(12, m.selected);
    /* Window must have scrolled so the last item is visible. */
    TEST_ASSERT_TRUE(m.scroll_offset + m.visible_count > m.selected);
    /* One more press: wrap to 0. */
    subghz_submenu_model_down(&m);
    TEST_ASSERT_EQUAL_UINT8(0, m.selected);
    TEST_ASSERT_EQUAL_UINT8(0, m.scroll_offset);
}

static void test_down_on_empty_list_is_noop(void)
{
    subghz_submenu_model_t m;
    subghz_submenu_model_init(&m, 0, 4);
    subghz_submenu_model_down(&m);
    TEST_ASSERT_EQUAL_UINT8(0, m.selected);
    TEST_ASSERT_EQUAL_UINT8(0, m.scroll_offset);
}

/*============================================================================*/
/* Up navigation                                                              */
/*============================================================================*/

static void test_up_from_zero_wraps_to_last(void)
{
    subghz_submenu_model_t m;
    subghz_submenu_model_init(&m, 13, 4);
    subghz_submenu_model_up(&m);
    TEST_ASSERT_EQUAL_UINT8(12, m.selected);
    /* Window must include the last item. */
    TEST_ASSERT_TRUE(subghz_submenu_model_is_visible(&m, 12));
}

static void test_up_decrements_within_window(void)
{
    subghz_submenu_model_t m;
    subghz_submenu_model_init(&m, 13, 4);
    subghz_submenu_model_set_selected(&m, 3);
    subghz_submenu_model_up(&m);
    TEST_ASSERT_EQUAL_UINT8(2, m.selected);
    TEST_ASSERT_EQUAL_UINT8(0, m.scroll_offset);
}

static void test_up_scrolls_window_when_leaving_top(void)
{
    subghz_submenu_model_t m;
    subghz_submenu_model_init(&m, 13, 4);
    /* Jump to item 8 — scroll = 5, sel = 8 visible at row 3. */
    subghz_submenu_model_set_selected(&m, 8);
    TEST_ASSERT_EQUAL_UINT8(5, m.scroll_offset);
    /* Up to 7 — still visible at row 2, no scroll. */
    subghz_submenu_model_up(&m);
    TEST_ASSERT_EQUAL_UINT8(7, m.selected);
    TEST_ASSERT_EQUAL_UINT8(5, m.scroll_offset);
    /* Up past the top of the window — scroll must follow. */
    subghz_submenu_model_up(&m);
    subghz_submenu_model_up(&m);
    subghz_submenu_model_up(&m);
    TEST_ASSERT_EQUAL_UINT8(4, m.selected);
    TEST_ASSERT_EQUAL_UINT8(4, m.scroll_offset);
}

static void test_up_on_empty_list_is_noop(void)
{
    subghz_submenu_model_t m;
    subghz_submenu_model_init(&m, 0, 4);
    subghz_submenu_model_up(&m);
    TEST_ASSERT_EQUAL_UINT8(0, m.selected);
}

/*============================================================================*/
/* set_selected                                                               */
/*============================================================================*/

static void test_set_selected_clamps_overflow(void)
{
    subghz_submenu_model_t m;
    subghz_submenu_model_init(&m, 13, 4);
    subghz_submenu_model_set_selected(&m, 99);
    TEST_ASSERT_EQUAL_UINT8(12, m.selected);
    TEST_ASSERT_TRUE(subghz_submenu_model_is_visible(&m, 12));
}

static void test_set_selected_snaps_window(void)
{
    subghz_submenu_model_t m;
    subghz_submenu_model_init(&m, 13, 4);
    subghz_submenu_model_set_selected(&m, 7);
    TEST_ASSERT_EQUAL_UINT8(7, m.selected);
    TEST_ASSERT_TRUE(subghz_submenu_model_is_visible(&m, 7));
    /* Jump back to top. */
    subghz_submenu_model_set_selected(&m, 0);
    TEST_ASSERT_EQUAL_UINT8(0, m.scroll_offset);
}

static void test_set_selected_on_empty_list_is_noop(void)
{
    subghz_submenu_model_t m;
    subghz_submenu_model_init(&m, 0, 4);
    subghz_submenu_model_set_selected(&m, 5);
    TEST_ASSERT_EQUAL_UINT8(0, m.selected);
    TEST_ASSERT_EQUAL_UINT8(0, m.scroll_offset);
}

/*============================================================================*/
/* set_item_count                                                             */
/*============================================================================*/

static void test_set_item_count_clamps_selection_down(void)
{
    subghz_submenu_model_t m;
    subghz_submenu_model_init(&m, 13, 4);
    subghz_submenu_model_set_selected(&m, 12);
    subghz_submenu_model_set_item_count(&m, 5);
    TEST_ASSERT_EQUAL_UINT8(5, m.item_count);
    TEST_ASSERT_EQUAL_UINT8(4, m.selected);
    TEST_ASSERT_TRUE(subghz_submenu_model_is_visible(&m, 4));
}

static void test_set_item_count_zero_resets(void)
{
    subghz_submenu_model_t m;
    subghz_submenu_model_init(&m, 13, 4);
    subghz_submenu_model_set_selected(&m, 7);
    subghz_submenu_model_set_item_count(&m, 0);
    TEST_ASSERT_EQUAL_UINT8(0, m.item_count);
    TEST_ASSERT_EQUAL_UINT8(0, m.selected);
    TEST_ASSERT_EQUAL_UINT8(0, m.scroll_offset);
}

static void test_set_item_count_growing_preserves_selection(void)
{
    subghz_submenu_model_t m;
    subghz_submenu_model_init(&m, 5, 4);
    subghz_submenu_model_set_selected(&m, 3);
    subghz_submenu_model_set_item_count(&m, 20);
    TEST_ASSERT_EQUAL_UINT8(20, m.item_count);
    TEST_ASSERT_EQUAL_UINT8(3,  m.selected);
}

/*============================================================================*/
/* set_visible_count                                                          */
/*============================================================================*/

static void test_set_visible_count_recomputes_window(void)
{
    subghz_submenu_model_t m;
    subghz_submenu_model_init(&m, 13, 4);
    subghz_submenu_model_set_selected(&m, 7);
    /* Shrink visible to 2 — sel=7 must remain visible. */
    subghz_submenu_model_set_visible_count(&m, 2);
    TEST_ASSERT_EQUAL_UINT8(2, m.visible_count);
    TEST_ASSERT_TRUE(subghz_submenu_model_is_visible(&m, 7));
    /* Grow visible to 13 — entire list fits, scroll snaps to 0. */
    subghz_submenu_model_set_visible_count(&m, 13);
    TEST_ASSERT_EQUAL_UINT8(0, m.scroll_offset);
}

static void test_set_visible_count_zero_clamps_to_one(void)
{
    subghz_submenu_model_t m;
    subghz_submenu_model_init(&m, 13, 4);
    subghz_submenu_model_set_visible_count(&m, 0);
    TEST_ASSERT_EQUAL_UINT8(1, m.visible_count);
}

/*============================================================================*/
/* is_visible / visible_row                                                   */
/*============================================================================*/

static void test_is_visible_window_boundaries(void)
{
    subghz_submenu_model_t m;
    subghz_submenu_model_init(&m, 13, 4);
    subghz_submenu_model_set_selected(&m, 8);
    /* scroll_offset = 5, window = {5,6,7,8} */
    TEST_ASSERT_FALSE(subghz_submenu_model_is_visible(&m, 4));
    TEST_ASSERT_TRUE (subghz_submenu_model_is_visible(&m, 5));
    TEST_ASSERT_TRUE (subghz_submenu_model_is_visible(&m, 8));
    TEST_ASSERT_FALSE(subghz_submenu_model_is_visible(&m, 9));
    TEST_ASSERT_FALSE(subghz_submenu_model_is_visible(&m, 99));
}

static void test_is_visible_empty_list(void)
{
    subghz_submenu_model_t m;
    subghz_submenu_model_init(&m, 0, 4);
    TEST_ASSERT_FALSE(subghz_submenu_model_is_visible(&m, 0));
}

static void test_visible_row_tracks_selection(void)
{
    subghz_submenu_model_t m;
    subghz_submenu_model_init(&m, 13, 4);
    TEST_ASSERT_EQUAL_UINT8(0, subghz_submenu_model_visible_row(&m));
    subghz_submenu_model_set_selected(&m, 8);
    /* scroll_offset = 5, sel = 8 → row 3 */
    TEST_ASSERT_EQUAL_UINT8(3, subghz_submenu_model_visible_row(&m));
}

/*============================================================================*/
/* Invariants under random-ish walks                                          */
/*============================================================================*/

static void test_invariants_hold_under_random_walk(void)
{
    subghz_submenu_model_t m;
    subghz_submenu_model_init(&m, 13, 4);
    /* Deterministic walk that exercises both directions, wrap, and jumps. */
    static const char *script = "ddduuusssddddddduuuuuuuu";
    for (const char *p = script; *p; ++p) {
        if (*p == 'd') subghz_submenu_model_down(&m);
        else if (*p == 'u') subghz_submenu_model_up(&m);
        else if (*p == 's') subghz_submenu_model_set_selected(&m, 9);

        /* Invariants. */
        TEST_ASSERT_TRUE (m.selected      < m.item_count);
        TEST_ASSERT_TRUE (m.scroll_offset <= m.selected);
        TEST_ASSERT_TRUE (m.selected      < (uint16_t)m.scroll_offset + m.visible_count);
        TEST_ASSERT_TRUE (m.visible_count >= 1);
    }
}

static void test_size_is_compact(void)
{
    /* Model must fit in 4 bytes to pack into a 32-bit per-scene state slot. */
    TEST_ASSERT_EQUAL_size_t(4u, sizeof(subghz_submenu_model_t));
}

/*============================================================================*/

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_null_is_safe);
    RUN_TEST(test_init_sets_defaults);
    RUN_TEST(test_init_clamps_visible_zero_to_one);
    RUN_TEST(test_init_empty_list_keeps_defaults);

    RUN_TEST(test_down_advances_selection_within_window);
    RUN_TEST(test_down_scrolls_window_when_leaving_visible_area);
    RUN_TEST(test_down_wraps_at_end_and_resets_scroll);
    RUN_TEST(test_down_on_empty_list_is_noop);

    RUN_TEST(test_up_from_zero_wraps_to_last);
    RUN_TEST(test_up_decrements_within_window);
    RUN_TEST(test_up_scrolls_window_when_leaving_top);
    RUN_TEST(test_up_on_empty_list_is_noop);

    RUN_TEST(test_set_selected_clamps_overflow);
    RUN_TEST(test_set_selected_snaps_window);
    RUN_TEST(test_set_selected_on_empty_list_is_noop);

    RUN_TEST(test_set_item_count_clamps_selection_down);
    RUN_TEST(test_set_item_count_zero_resets);
    RUN_TEST(test_set_item_count_growing_preserves_selection);

    RUN_TEST(test_set_visible_count_recomputes_window);
    RUN_TEST(test_set_visible_count_zero_clamps_to_one);

    RUN_TEST(test_is_visible_window_boundaries);
    RUN_TEST(test_is_visible_empty_list);
    RUN_TEST(test_visible_row_tracks_selection);

    RUN_TEST(test_invariants_hold_under_random_walk);
    RUN_TEST(test_size_is_compact);
    return UNITY_END();
}
