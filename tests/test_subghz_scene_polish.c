/* See COPYING.txt for license details. */

/**
 * @file   test_subghz_scene_polish.c
 * @brief  Host-side unit tests for scene-manager polish primitives.
 */

#include "unity.h"
#include "subghz_scene_polish.h"

void setUp(void) { }
void tearDown(void) { }

/*============================================================================*/
/* subghz_scene_stack_find                                                    */
/*============================================================================*/

static void test_stack_find_returns_topmost_index(void)
{
    /* Stack: [Menu=0, Read=1, ReadRaw=2, ReceiverInfo=3]  depth=4 */
    const uint8_t stack[] = {0, 1, 2, 3};
    TEST_ASSERT_EQUAL_INT(0, subghz_scene_stack_find(stack, 4, 0));
    TEST_ASSERT_EQUAL_INT(1, subghz_scene_stack_find(stack, 4, 1));
    TEST_ASSERT_EQUAL_INT(2, subghz_scene_stack_find(stack, 4, 2));
    TEST_ASSERT_EQUAL_INT(3, subghz_scene_stack_find(stack, 4, 3));
}

static void test_stack_find_returns_minus_one_when_not_present(void)
{
    const uint8_t stack[] = {0, 1, 2};
    TEST_ASSERT_EQUAL_INT(-1, subghz_scene_stack_find(stack, 3, 99));
    TEST_ASSERT_EQUAL_INT(-1, subghz_scene_stack_find(stack, 3, 3));
}

static void test_stack_find_handles_empty_stack(void)
{
    const uint8_t stack[] = {7};      /* contents ignored when depth=0 */
    TEST_ASSERT_EQUAL_INT(-1, subghz_scene_stack_find(stack, 0, 7));
}

static void test_stack_find_handles_null_pointer(void)
{
    TEST_ASSERT_EQUAL_INT(-1, subghz_scene_stack_find(NULL, 4, 0));
    /* NULL with depth=0 is also fine. */
    TEST_ASSERT_EQUAL_INT(-1, subghz_scene_stack_find(NULL, 0, 0));
}

static void test_stack_find_returns_topmost_when_duplicate_present(void)
{
    /* Same scene pushed twice — e.g. Read → Config → Read.
     * search_and_pop_to(Read) should resolve to the TOP copy so the
     * intermediate Config is popped, not preserved. */
    const uint8_t stack[] = {0, 1, 2, 1};   /* indices 0,1,2,3 */
    TEST_ASSERT_EQUAL_INT(3, subghz_scene_stack_find(stack, 4, 1));
}

static void test_stack_find_single_element(void)
{
    const uint8_t stack[] = {5};
    TEST_ASSERT_EQUAL_INT(0, subghz_scene_stack_find(stack, 1, 5));
    TEST_ASSERT_EQUAL_INT(-1, subghz_scene_stack_find(stack, 1, 6));
}

/*============================================================================*/
/* subghz_scene_stack_pop_to_depth                                            */
/*============================================================================*/

static void test_pop_to_depth_when_target_is_root(void)
{
    const uint8_t stack[] = {0, 1, 2, 3};
    /* Popping to scene 0 (root) → new depth = 1. */
    TEST_ASSERT_EQUAL_UINT8(1, subghz_scene_stack_pop_to_depth(stack, 4, 0));
}

static void test_pop_to_depth_when_target_is_already_top(void)
{
    const uint8_t stack[] = {0, 1, 2};
    /* Target already on top — depth unchanged. */
    TEST_ASSERT_EQUAL_UINT8(3, subghz_scene_stack_pop_to_depth(stack, 3, 2));
}

static void test_pop_to_depth_when_target_in_middle(void)
{
    /* [Menu, Saved, SaveName, SaveSuccess] → pop_to_depth(Saved) → 2 */
    const uint8_t stack[] = {0, 1, 2, 3};
    TEST_ASSERT_EQUAL_UINT8(2, subghz_scene_stack_pop_to_depth(stack, 4, 1));
}

static void test_pop_to_depth_when_target_missing(void)
{
    /* Not found → depth unchanged. */
    const uint8_t stack[] = {0, 1, 2};
    TEST_ASSERT_EQUAL_UINT8(3, subghz_scene_stack_pop_to_depth(stack, 3, 9));
}

static void test_pop_to_depth_empty_stack(void)
{
    TEST_ASSERT_EQUAL_UINT8(0, subghz_scene_stack_pop_to_depth(NULL, 0, 0));
}

static void test_pop_to_depth_duplicate_picks_topmost(void)
{
    /* [Read, Config, Read] — pop_to_depth(Read) → 3 (top copy). */
    const uint8_t stack[] = {1, 2, 1};
    TEST_ASSERT_EQUAL_UINT8(3, subghz_scene_stack_pop_to_depth(stack, 3, 1));
}

/*============================================================================*/
/* subghz_scene_tick_due                                                      */
/*============================================================================*/

static void test_tick_due_disabled_when_period_is_zero(void)
{
    /* period=0 is the "feature off" sentinel — never fires regardless. */
    TEST_ASSERT_FALSE(subghz_scene_tick_due(0,       0, 0));
    TEST_ASSERT_FALSE(subghz_scene_tick_due(1000,    0, 0));
    TEST_ASSERT_FALSE(subghz_scene_tick_due(123456,  0, 0));
}

static void test_tick_due_returns_false_before_period_elapsed(void)
{
    TEST_ASSERT_FALSE(subghz_scene_tick_due(100,   100, 200));   /* 0 elapsed */
    TEST_ASSERT_FALSE(subghz_scene_tick_due(150,   100, 200));   /* 50 < 200 */
    TEST_ASSERT_FALSE(subghz_scene_tick_due(299,   100, 200));   /* 199 < 200 */
}

static void test_tick_due_returns_true_at_period_boundary(void)
{
    /* `>=` semantics — fires on the exact boundary. */
    TEST_ASSERT_TRUE(subghz_scene_tick_due(300,   100, 200));    /* 200 == 200 */
    TEST_ASSERT_TRUE(subghz_scene_tick_due(301,   100, 200));    /* 201 > 200  */
    TEST_ASSERT_TRUE(subghz_scene_tick_due(10000, 100, 200));    /* far past   */
}

static void test_tick_due_handles_hal_tick_wraparound(void)
{
    /* HAL_GetTick() wraps at 2^32 ms (~49.7 days).  The subtraction must
     * yield the correct unsigned difference even when `now` has wrapped
     * past zero while `last_tick` is just below 2^32.  Concrete case:
     *   last = 0xFFFFFFF0, now = 0x00000010, period = 100
     *   elapsed = (0x00000010 - 0xFFFFFFF0) wrapped = 0x20 = 32
     * → not yet due, period=100 still wins. */
    TEST_ASSERT_FALSE(subghz_scene_tick_due(0x00000010u,
                                             0xFFFFFFF0u,
                                             100u));

    /* Same wrap, but enough elapsed to fire:
     *   last = 0xFFFFFFE0, now = 0x00000080, period = 100
     *   elapsed = 0xA0 = 160 ≥ 100 → due. */
    TEST_ASSERT_TRUE(subghz_scene_tick_due(0x00000080u,
                                            0xFFFFFFE0u,
                                            100u));
}

static void test_tick_due_period_of_one_fires_every_ms(void)
{
    /* period=1 should fire on every ms boundary. */
    TEST_ASSERT_FALSE(subghz_scene_tick_due(0, 0, 1));
    TEST_ASSERT_TRUE(subghz_scene_tick_due(1, 0, 1));
    TEST_ASSERT_TRUE(subghz_scene_tick_due(2, 0, 1));
}

static void test_tick_due_realistic_animation_cadence(void)
{
    /* Simulate a 5 fps animation tick (200 ms period) over 1 second. */
    uint32_t period = 200;
    uint32_t last_fired = 0;
    int fire_count = 0;
    for (uint32_t now = 0; now <= 1000; now += 10) {
        if (subghz_scene_tick_due(now, last_fired, period)) {
            fire_count++;
            last_fired = now;
        }
    }
    /* Should fire at 200, 400, 600, 800, 1000 — five ticks total. */
    TEST_ASSERT_EQUAL_INT(5, fire_count);
}

/*============================================================================*/
/* Custom payload typedef sanity                                              */
/*============================================================================*/

static void test_custom_payload_is_32_bit(void)
{
    /* Type is documented as a 32-bit word for the scene-manager API. */
    TEST_ASSERT_EQUAL_size_t(4, sizeof(subghz_scene_custom_payload_t));
}

static void test_custom_payload_round_trip(void)
{
    /* Trivial round-trip — caller stores then reads.  Confirms unsigned
     * 32-bit semantics (no sign-extension surprises). */
    subghz_scene_custom_payload_t v = 0xCAFEBABEu;
    TEST_ASSERT_EQUAL_HEX32(0xCAFEBABEu, v);

    /* High-bit set is preserved. */
    v = 0x80000000u;
    TEST_ASSERT_EQUAL_HEX32(0x80000000u, v);
}

/*============================================================================*/

int main(void)
{
    UNITY_BEGIN();
    /* stack_find */
    RUN_TEST(test_stack_find_returns_topmost_index);
    RUN_TEST(test_stack_find_returns_minus_one_when_not_present);
    RUN_TEST(test_stack_find_handles_empty_stack);
    RUN_TEST(test_stack_find_handles_null_pointer);
    RUN_TEST(test_stack_find_returns_topmost_when_duplicate_present);
    RUN_TEST(test_stack_find_single_element);
    /* pop_to_depth */
    RUN_TEST(test_pop_to_depth_when_target_is_root);
    RUN_TEST(test_pop_to_depth_when_target_is_already_top);
    RUN_TEST(test_pop_to_depth_when_target_in_middle);
    RUN_TEST(test_pop_to_depth_when_target_missing);
    RUN_TEST(test_pop_to_depth_empty_stack);
    RUN_TEST(test_pop_to_depth_duplicate_picks_topmost);
    /* tick_due */
    RUN_TEST(test_tick_due_disabled_when_period_is_zero);
    RUN_TEST(test_tick_due_returns_false_before_period_elapsed);
    RUN_TEST(test_tick_due_returns_true_at_period_boundary);
    RUN_TEST(test_tick_due_handles_hal_tick_wraparound);
    RUN_TEST(test_tick_due_period_of_one_fires_every_ms);
    RUN_TEST(test_tick_due_realistic_animation_cadence);
    /* payload typedef */
    RUN_TEST(test_custom_payload_is_32_bit);
    RUN_TEST(test_custom_payload_round_trip);
    return UNITY_END();
}
