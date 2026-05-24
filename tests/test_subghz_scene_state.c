/* See COPYING.txt for license details. */

/**
 * @file   test_subghz_scene_state.c
 * @brief  Host-side unit tests for per-scene 32-bit state slots.
 */

#include "unity.h"
#include "subghz_scene_state.h"
#include <string.h>

void setUp(void) { }
void tearDown(void) { }

/*----------------------------------------------------------------------------*/

static void test_capacity_is_positive(void)
{
    TEST_ASSERT_TRUE(subghz_scene_state_capacity() > 0);
}

static void test_init_zeroes_all_slots(void)
{
    subghz_scene_state_array_t arr;
    /* Fill with non-zero garbage first. */
    memset(&arr, 0xAB, sizeof(arr));
    subghz_scene_state_init(&arr);
    for (uint8_t i = 0; i < subghz_scene_state_capacity(); ++i) {
        TEST_ASSERT_EQUAL_UINT32(0u, subghz_scene_state_get(&arr, i));
    }
}

static void test_init_handles_null_safely(void)
{
    /* Must not crash. */
    subghz_scene_state_init(NULL);
}

static void test_set_then_get_round_trip(void)
{
    subghz_scene_state_array_t arr;
    subghz_scene_state_init(&arr);

    TEST_ASSERT_TRUE(subghz_scene_state_set(&arr, 0, 0xDEADBEEF));
    TEST_ASSERT_TRUE(subghz_scene_state_set(&arr, 1, 0x12345678));
    TEST_ASSERT_TRUE(subghz_scene_state_set(&arr, 7, 0xFFFFFFFF));

    TEST_ASSERT_EQUAL_HEX32(0xDEADBEEF, subghz_scene_state_get(&arr, 0));
    TEST_ASSERT_EQUAL_HEX32(0x12345678, subghz_scene_state_get(&arr, 1));
    TEST_ASSERT_EQUAL_HEX32(0xFFFFFFFF, subghz_scene_state_get(&arr, 7));
    /* Untouched slots remain zero. */
    TEST_ASSERT_EQUAL_UINT32(0u, subghz_scene_state_get(&arr, 2));
    TEST_ASSERT_EQUAL_UINT32(0u, subghz_scene_state_get(&arr, 10));
}

static void test_set_overwrites(void)
{
    subghz_scene_state_array_t arr;
    subghz_scene_state_init(&arr);

    TEST_ASSERT_TRUE(subghz_scene_state_set(&arr, 3, 0xAAAAAAAA));
    TEST_ASSERT_TRUE(subghz_scene_state_set(&arr, 3, 0x55555555));
    TEST_ASSERT_EQUAL_HEX32(0x55555555, subghz_scene_state_get(&arr, 3));
}

static void test_out_of_range_set_is_rejected(void)
{
    subghz_scene_state_array_t arr;
    subghz_scene_state_init(&arr);

    TEST_ASSERT_FALSE(subghz_scene_state_set(&arr, subghz_scene_state_capacity(), 0xCAFE));
    TEST_ASSERT_FALSE(subghz_scene_state_set(&arr, 255, 0xCAFE));
    /* No collateral damage to a valid slot. */
    TEST_ASSERT_EQUAL_UINT32(0u, subghz_scene_state_get(&arr, 0));
}

static void test_out_of_range_get_returns_zero(void)
{
    subghz_scene_state_array_t arr;
    subghz_scene_state_init(&arr);
    /* Sentinel value at a valid slot — must not leak through to OOR reads. */
    subghz_scene_state_set(&arr, 0, 0xABCDABCD);

    TEST_ASSERT_EQUAL_UINT32(0u, subghz_scene_state_get(&arr, subghz_scene_state_capacity()));
    TEST_ASSERT_EQUAL_UINT32(0u, subghz_scene_state_get(&arr, 255));
}

static void test_null_get_returns_zero(void)
{
    TEST_ASSERT_EQUAL_UINT32(0u, subghz_scene_state_get(NULL, 0));
    TEST_ASSERT_EQUAL_UINT32(0u, subghz_scene_state_get(NULL, 10));
}

static void test_null_set_returns_false(void)
{
    TEST_ASSERT_FALSE(subghz_scene_state_set(NULL, 0, 0xCAFE));
}

static void test_slots_are_independent(void)
{
    /* Writing slot N must not bleed into slot N±1 (catches off-by-one bugs). */
    subghz_scene_state_array_t arr;
    subghz_scene_state_init(&arr);

    for (uint8_t i = 0; i < subghz_scene_state_capacity(); ++i) {
        TEST_ASSERT_TRUE(subghz_scene_state_set(&arr, i, 0x1000u + i));
    }
    for (uint8_t i = 0; i < subghz_scene_state_capacity(); ++i) {
        TEST_ASSERT_EQUAL_UINT32(0x1000u + i, subghz_scene_state_get(&arr, i));
    }
}

static void test_init_after_use_clears_all_slots(void)
{
    subghz_scene_state_array_t arr;
    subghz_scene_state_init(&arr);
    for (uint8_t i = 0; i < subghz_scene_state_capacity(); ++i) {
        subghz_scene_state_set(&arr, i, 0xFFFFFFFFu);
    }
    subghz_scene_state_init(&arr);
    for (uint8_t i = 0; i < subghz_scene_state_capacity(); ++i) {
        TEST_ASSERT_EQUAL_UINT32(0u, subghz_scene_state_get(&arr, i));
    }
}

static void test_realistic_menu_scene_use_case(void)
{
    /* Simulates the migration done in m1_subghz_scene_menu.c:
     * the menu scene stores its selected item index in slot SubGhzSceneMenu (0).
     * After pushing a child scene and popping back, the value persists.
     *
     * The state survives any number of unrelated set/get on other slots
     * (push/pop chain visiting other scenes does not clobber Menu's slot). */
    subghz_scene_state_array_t arr;
    subghz_scene_state_init(&arr);

    const uint8_t MENU_SCENE_ID = 0;          /* SubGhzSceneMenu */
    const uint8_t READ_SCENE_ID = 1;          /* SubGhzSceneRead */
    const uint8_t SAVED_SCENE_ID = 8;         /* SubGhzSceneSaved */

    /* User scrolls to item 7 (Freq Scanner). */
    subghz_scene_state_set(&arr, MENU_SCENE_ID, 7);

    /* User pushes Read; Read uses its own slot. */
    subghz_scene_state_set(&arr, READ_SCENE_ID, 0xCAFEBABE);

    /* User pops back to Menu, then opens Saved, then pops back to Menu.
     * Saved scrambles its own slot but never touches Menu's. */
    subghz_scene_state_set(&arr, SAVED_SCENE_ID, 42);

    /* Menu's stored cursor position is still 7. */
    TEST_ASSERT_EQUAL_UINT32(7u, subghz_scene_state_get(&arr, MENU_SCENE_ID));
    /* Other slots are also intact. */
    TEST_ASSERT_EQUAL_HEX32(0xCAFEBABE, subghz_scene_state_get(&arr, READ_SCENE_ID));
    TEST_ASSERT_EQUAL_UINT32(42u, subghz_scene_state_get(&arr, SAVED_SCENE_ID));
}

/*----------------------------------------------------------------------------*/

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_capacity_is_positive);
    RUN_TEST(test_init_zeroes_all_slots);
    RUN_TEST(test_init_handles_null_safely);
    RUN_TEST(test_set_then_get_round_trip);
    RUN_TEST(test_set_overwrites);
    RUN_TEST(test_out_of_range_set_is_rejected);
    RUN_TEST(test_out_of_range_get_returns_zero);
    RUN_TEST(test_null_get_returns_zero);
    RUN_TEST(test_null_set_returns_false);
    RUN_TEST(test_slots_are_independent);
    RUN_TEST(test_init_after_use_clears_all_slots);
    RUN_TEST(test_realistic_menu_scene_use_case);
    return UNITY_END();
}
