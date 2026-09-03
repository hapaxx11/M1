/* See COPYING.txt for license details. */

/*
 * test_badusb_hold.c
 *
 * Host-side unit tests for the BadUSB HOLD / RELEASE held-key state module
 * (badusb_hold.c/h): add/remove/release-all, duplicate suppression, table
 * overflow, and 8-byte HID boot-keyboard report generation.
 */

#include "unity.h"
#include "badusb_hold.h"
#include "badusb_parser.h"   /* BUSB_KEY_*, BUSB_MOD_* */
#include <string.h>

static busb_hold_state_t h;

void setUp(void)    { busb_hold_init(&h); }
void tearDown(void) {}

/*═══════════════ Basic add / state ═══════════════*/

void test_init_is_empty(void)
{
    TEST_ASSERT_TRUE(busb_hold_is_empty(&h));
    TEST_ASSERT_EQUAL(0, h.modifiers);
    TEST_ASSERT_EQUAL(0, h.count);
}

void test_add_modifier_only(void)
{
    TEST_ASSERT_TRUE(busb_hold_add(&h, BUSB_MOD_LSHIFT, BUSB_KEY_NONE));
    TEST_ASSERT_FALSE(busb_hold_is_empty(&h));
    TEST_ASSERT_EQUAL(BUSB_MOD_LSHIFT, h.modifiers);
    TEST_ASSERT_EQUAL(0, h.count);
}

void test_add_keycode(void)
{
    TEST_ASSERT_TRUE(busb_hold_add(&h, 0, BUSB_KEY_A));
    TEST_ASSERT_EQUAL(1, h.count);
    TEST_ASSERT_EQUAL(BUSB_KEY_A, h.keys[0]);
}

void test_add_modifier_and_key(void)
{
    TEST_ASSERT_TRUE(busb_hold_add(&h, BUSB_MOD_LCTRL, BUSB_KEY_C));
    TEST_ASSERT_EQUAL(BUSB_MOD_LCTRL, h.modifiers);
    TEST_ASSERT_EQUAL(1, h.count);
    TEST_ASSERT_EQUAL(BUSB_KEY_C, h.keys[0]);
}

void test_add_duplicate_key_is_noop(void)
{
    TEST_ASSERT_TRUE(busb_hold_add(&h, 0, BUSB_KEY_A));
    TEST_ASSERT_TRUE(busb_hold_add(&h, 0, BUSB_KEY_A));
    TEST_ASSERT_EQUAL(1, h.count);
}

void test_add_accumulates_modifiers(void)
{
    busb_hold_add(&h, BUSB_MOD_LCTRL, BUSB_KEY_NONE);
    busb_hold_add(&h, BUSB_MOD_LALT, BUSB_KEY_NONE);
    TEST_ASSERT_EQUAL(BUSB_MOD_LCTRL | BUSB_MOD_LALT, h.modifiers);
}

void test_add_overflow_returns_false(void)
{
    /* Fill all 6 slots, then a 7th must fail. */
    for (int i = 0; i < BUSB_HOLD_MAX_KEYS; i++)
        TEST_ASSERT_TRUE(busb_hold_add(&h, 0, (uint8_t)(BUSB_KEY_A + i)));
    TEST_ASSERT_EQUAL(BUSB_HOLD_MAX_KEYS, h.count);
    TEST_ASSERT_FALSE(busb_hold_add(&h, 0, BUSB_KEY_Z));
    TEST_ASSERT_EQUAL(BUSB_HOLD_MAX_KEYS, h.count);
}

/*═══════════════ Remove / release ═══════════════*/

void test_remove_key_compacts(void)
{
    busb_hold_add(&h, 0, BUSB_KEY_A);
    busb_hold_add(&h, 0, BUSB_KEY_B);
    busb_hold_add(&h, 0, BUSB_KEY_C);
    busb_hold_remove(&h, 0, BUSB_KEY_B);
    TEST_ASSERT_EQUAL(2, h.count);
    TEST_ASSERT_EQUAL(BUSB_KEY_A, h.keys[0]);
    TEST_ASSERT_EQUAL(BUSB_KEY_C, h.keys[1]);
    TEST_ASSERT_EQUAL(0, h.keys[2]);
}

void test_remove_modifier_only(void)
{
    busb_hold_add(&h, BUSB_MOD_LSHIFT | BUSB_MOD_LCTRL, BUSB_KEY_NONE);
    busb_hold_remove(&h, BUSB_MOD_LSHIFT, BUSB_KEY_NONE);
    TEST_ASSERT_EQUAL(BUSB_MOD_LCTRL, h.modifiers);
}

void test_remove_absent_key_is_noop(void)
{
    busb_hold_add(&h, 0, BUSB_KEY_A);
    busb_hold_remove(&h, 0, BUSB_KEY_Z);
    TEST_ASSERT_EQUAL(1, h.count);
}

void test_release_all(void)
{
    busb_hold_add(&h, BUSB_MOD_LGUI, BUSB_KEY_R);
    busb_hold_add(&h, 0, BUSB_KEY_A);
    busb_hold_release_all(&h);
    TEST_ASSERT_TRUE(busb_hold_is_empty(&h));
}

/*═══════════════ Report generation ═══════════════*/

void test_report_empty(void)
{
    uint8_t r[8];
    busb_hold_build_report(&h, 0, 0, r);
    for (int i = 0; i < 8; i++)
        TEST_ASSERT_EQUAL(0, r[i]);
}

void test_report_held_only(void)
{
    busb_hold_add(&h, BUSB_MOD_LCTRL, BUSB_KEY_A);
    busb_hold_add(&h, 0, BUSB_KEY_B);
    uint8_t r[8];
    busb_hold_build_report(&h, 0, 0, r);
    TEST_ASSERT_EQUAL(BUSB_MOD_LCTRL, r[0]);
    TEST_ASSERT_EQUAL(0, r[1]);
    TEST_ASSERT_EQUAL(BUSB_KEY_A, r[2]);
    TEST_ASSERT_EQUAL(BUSB_KEY_B, r[3]);
    TEST_ASSERT_EQUAL(0, r[4]);
}

void test_report_merges_transient(void)
{
    busb_hold_add(&h, BUSB_MOD_LSHIFT, BUSB_KEY_NONE);   /* hold SHIFT */
    uint8_t r[8];
    busb_hold_build_report(&h, 0, BUSB_KEY_A, r);        /* press 'a' */
    TEST_ASSERT_EQUAL(BUSB_MOD_LSHIFT, r[0]);
    TEST_ASSERT_EQUAL(BUSB_KEY_A, r[2]);
}

void test_report_transient_modifier_ORed(void)
{
    busb_hold_add(&h, BUSB_MOD_LSHIFT, BUSB_KEY_NONE);
    uint8_t r[8];
    busb_hold_build_report(&h, BUSB_MOD_LCTRL, BUSB_KEY_C, r);
    TEST_ASSERT_EQUAL(BUSB_MOD_LSHIFT | BUSB_MOD_LCTRL, r[0]);
    TEST_ASSERT_EQUAL(BUSB_KEY_C, r[2]);
}

void test_report_transient_not_duplicated(void)
{
    busb_hold_add(&h, 0, BUSB_KEY_A);
    uint8_t r[8];
    busb_hold_build_report(&h, 0, BUSB_KEY_A, r);   /* same key held + pressed */
    TEST_ASSERT_EQUAL(BUSB_KEY_A, r[2]);
    TEST_ASSERT_EQUAL(0, r[3]);   /* not duplicated into a second slot */
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_is_empty);
    RUN_TEST(test_add_modifier_only);
    RUN_TEST(test_add_keycode);
    RUN_TEST(test_add_modifier_and_key);
    RUN_TEST(test_add_duplicate_key_is_noop);
    RUN_TEST(test_add_accumulates_modifiers);
    RUN_TEST(test_add_overflow_returns_false);
    RUN_TEST(test_remove_key_compacts);
    RUN_TEST(test_remove_modifier_only);
    RUN_TEST(test_remove_absent_key_is_noop);
    RUN_TEST(test_release_all);
    RUN_TEST(test_report_empty);
    RUN_TEST(test_report_held_only);
    RUN_TEST(test_report_merges_transient);
    RUN_TEST(test_report_transient_modifier_ORed);
    RUN_TEST(test_report_transient_not_duplicated);
    return UNITY_END();
}
