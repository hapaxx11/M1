/* See COPYING.txt for license details. */

/**
 * @file   test_subghz_hex_editor.c
 * @brief  Host unit tests for the pure-logic hex-digit editor module
 *         (Phase 8b-3 — SetKey scene backing model, reused by Phase 8c
 *         SetSerial / SetButton / SetCounter scenes).
 */

#include "unity.h"
#include "subghz_hex_editor.h"

#include <string.h>

void setUp(void) { }
void tearDown(void) { }

/* ----------------------- init -------------------------------------- */

static void test_init_zeroes_digits_and_cursor(void)
{
    subghz_hex_editor_t ed;
    /* Pre-poison */
    memset(&ed, 0xAB, sizeof(ed));
    subghz_hex_editor_init(&ed, 24U);

    TEST_ASSERT_EQUAL_UINT8(24U, ed.bit_count);
    TEST_ASSERT_EQUAL_UINT8(6U, ed.digit_count);
    TEST_ASSERT_EQUAL_UINT8(0U, ed.cursor);
    for (uint8_t i = 0U; i < SUBGHZ_HEX_EDITOR_MAX_DIGITS; ++i)
        TEST_ASSERT_EQUAL_UINT8(0U, ed.digits[i]);
}

static void test_init_rounds_up_to_hex_digit_boundary(void)
{
    subghz_hex_editor_t ed;

    /* 52-bit Nice FloR-S → 13 hex digits (52/4 = 13 exactly) */
    subghz_hex_editor_init(&ed, 52U);
    TEST_ASSERT_EQUAL_UINT8(13U, ed.digit_count);

    /* 54-bit DITEC GOL4 → 14 hex digits (54+3)/4 = 14 */
    subghz_hex_editor_init(&ed, 54U);
    TEST_ASSERT_EQUAL_UINT8(14U, ed.digit_count);

    /* 62-bit CAME Atomo → 16 hex digits ((62+3)/4 = 16) */
    subghz_hex_editor_init(&ed, 62U);
    TEST_ASSERT_EQUAL_UINT8(16U, ed.digit_count);

    /* 64-bit Alutech AT-4N → 16 hex digits exactly */
    subghz_hex_editor_init(&ed, 64U);
    TEST_ASSERT_EQUAL_UINT8(16U, ed.digit_count);

    /* 12-bit Holtek HT12X → 3 hex digits */
    subghz_hex_editor_init(&ed, 12U);
    TEST_ASSERT_EQUAL_UINT8(3U, ed.digit_count);

    /* 10-bit Linear → 3 hex digits ((10+3)/4 = 3) */
    subghz_hex_editor_init(&ed, 10U);
    TEST_ASSERT_EQUAL_UINT8(3U, ed.digit_count);
}

static void test_init_clamps_extreme_bit_counts(void)
{
    subghz_hex_editor_t ed;

    /* 0 bits → treated as 1 bit → 1 hex digit */
    subghz_hex_editor_init(&ed, 0U);
    TEST_ASSERT_EQUAL_UINT8(1U, ed.bit_count);
    TEST_ASSERT_EQUAL_UINT8(1U, ed.digit_count);

    /* 200 bits → clamped to 64 → 16 hex digits */
    subghz_hex_editor_init(&ed, 200U);
    TEST_ASSERT_EQUAL_UINT8(64U, ed.bit_count);
    TEST_ASSERT_EQUAL_UINT8(16U, ed.digit_count);
}

static void test_init_null_safe(void)
{
    /* Should not crash */
    subghz_hex_editor_init(NULL, 24U);
}

/* ----------------------- value get/set ---------------------------- */

static void test_set_value_then_value_roundtrip(void)
{
    subghz_hex_editor_t ed;
    subghz_hex_editor_init(&ed, 24U);

    subghz_hex_editor_set_value(&ed, 0x123456ULL);
    TEST_ASSERT_EQUAL_HEX64(0x123456ULL, subghz_hex_editor_value(&ed));

    /* Digit-by-digit check (big-endian) */
    TEST_ASSERT_EQUAL_UINT8(0x1U, ed.digits[0]);
    TEST_ASSERT_EQUAL_UINT8(0x2U, ed.digits[1]);
    TEST_ASSERT_EQUAL_UINT8(0x3U, ed.digits[2]);
    TEST_ASSERT_EQUAL_UINT8(0x4U, ed.digits[3]);
    TEST_ASSERT_EQUAL_UINT8(0x5U, ed.digits[4]);
    TEST_ASSERT_EQUAL_UINT8(0x6U, ed.digits[5]);
}

static void test_set_value_masks_high_bits(void)
{
    subghz_hex_editor_t ed;
    subghz_hex_editor_init(&ed, 24U);

    /* Top byte of 0xFF000000 is above bit_count and must be masked */
    subghz_hex_editor_set_value(&ed, 0xFFABCDEFULL);
    TEST_ASSERT_EQUAL_HEX64(0xABCDEFULL, subghz_hex_editor_value(&ed));
}

static void test_set_value_full_64_bits(void)
{
    subghz_hex_editor_t ed;
    subghz_hex_editor_init(&ed, 64U);

    subghz_hex_editor_set_value(&ed, 0xDEADBEEFCAFEBABEULL);
    TEST_ASSERT_EQUAL_HEX64(0xDEADBEEFCAFEBABEULL, subghz_hex_editor_value(&ed));

    /* Spot-check digit positions */
    TEST_ASSERT_EQUAL_UINT8(0xDU, ed.digits[0]);
    TEST_ASSERT_EQUAL_UINT8(0xEU, ed.digits[15]);
}

static void test_value_null_returns_zero(void)
{
    TEST_ASSERT_EQUAL_HEX64(0ULL, subghz_hex_editor_value(NULL));
}

/* ----------------------- cursor movement -------------------------- */

static void test_left_saturates_at_zero(void)
{
    subghz_hex_editor_t ed;
    subghz_hex_editor_init(&ed, 24U);

    /* Already at 0 */
    subghz_hex_editor_left(&ed);
    TEST_ASSERT_EQUAL_UINT8(0U, ed.cursor);

    /* Move right twice then left three times → saturate at 0 */
    subghz_hex_editor_right(&ed);
    subghz_hex_editor_right(&ed);
    TEST_ASSERT_EQUAL_UINT8(2U, ed.cursor);
    subghz_hex_editor_left(&ed);
    subghz_hex_editor_left(&ed);
    subghz_hex_editor_left(&ed);
    TEST_ASSERT_EQUAL_UINT8(0U, ed.cursor);
}

static void test_right_saturates_at_last_digit(void)
{
    subghz_hex_editor_t ed;
    subghz_hex_editor_init(&ed, 24U);  /* 6 digits */

    for (uint8_t i = 0U; i < 10U; ++i)
        subghz_hex_editor_right(&ed);

    TEST_ASSERT_EQUAL_UINT8(5U, ed.cursor);  /* digit_count - 1 */
}

static void test_cursor_movement_null_safe(void)
{
    subghz_hex_editor_left(NULL);
    subghz_hex_editor_right(NULL);
    subghz_hex_editor_up(NULL);
    subghz_hex_editor_down(NULL);
}

/* ----------------------- digit cycling ---------------------------- */

static void test_up_wraps_F_to_zero(void)
{
    subghz_hex_editor_t ed;
    subghz_hex_editor_init(&ed, 24U);

    /* Cycle digit[0] from 0 up 16 times → back to 0 */
    for (int i = 0; i < 16; ++i)
        subghz_hex_editor_up(&ed);
    TEST_ASSERT_EQUAL_UINT8(0U, ed.digits[0]);

    /* One more up → 1 */
    subghz_hex_editor_up(&ed);
    TEST_ASSERT_EQUAL_UINT8(1U, ed.digits[0]);
}

static void test_down_wraps_zero_to_F(void)
{
    subghz_hex_editor_t ed;
    subghz_hex_editor_init(&ed, 24U);

    subghz_hex_editor_down(&ed);
    TEST_ASSERT_EQUAL_UINT8(0xFU, ed.digits[0]);
}

static void test_up_down_only_affects_cursor_digit(void)
{
    subghz_hex_editor_t ed;
    subghz_hex_editor_init(&ed, 24U);

    subghz_hex_editor_right(&ed);
    subghz_hex_editor_right(&ed);   /* cursor = 2 */
    subghz_hex_editor_up(&ed);      /* digits[2] = 1 */
    subghz_hex_editor_up(&ed);      /* digits[2] = 2 */

    TEST_ASSERT_EQUAL_UINT8(0U, ed.digits[0]);
    TEST_ASSERT_EQUAL_UINT8(0U, ed.digits[1]);
    TEST_ASSERT_EQUAL_UINT8(2U, ed.digits[2]);
    TEST_ASSERT_EQUAL_UINT8(0U, ed.digits[3]);
}

/* ----------------------- end-to-end UX flow ----------------------- */

static void test_end_to_end_build_key_0xAB12(void)
{
    /* Simulate: 16-bit key, user enters 0xAB12 digit-by-digit */
    subghz_hex_editor_t ed;
    subghz_hex_editor_init(&ed, 16U);

    /* digits[0] (most significant) -> A */
    for (int i = 0; i < 0xA; ++i) subghz_hex_editor_up(&ed);
    subghz_hex_editor_right(&ed);
    /* digits[1] -> B */
    for (int i = 0; i < 0xB; ++i) subghz_hex_editor_up(&ed);
    subghz_hex_editor_right(&ed);
    /* digits[2] -> 1 */
    subghz_hex_editor_up(&ed);
    subghz_hex_editor_right(&ed);
    /* digits[3] -> 2 */
    subghz_hex_editor_up(&ed);
    subghz_hex_editor_up(&ed);

    TEST_ASSERT_EQUAL_HEX64(0xAB12ULL, subghz_hex_editor_value(&ed));
}

static void test_load_then_modify_one_digit(void)
{
    /* Use case: re-edit a previously saved key */
    subghz_hex_editor_t ed;
    subghz_hex_editor_init(&ed, 24U);
    subghz_hex_editor_set_value(&ed, 0x123456ULL);

    /* Move cursor to digit[3] (= 4) and bump it to 5 */
    subghz_hex_editor_right(&ed);
    subghz_hex_editor_right(&ed);
    subghz_hex_editor_right(&ed);
    subghz_hex_editor_up(&ed);

    TEST_ASSERT_EQUAL_HEX64(0x123556ULL, subghz_hex_editor_value(&ed));
}

static void test_value_assembly_masks_to_bit_count(void)
{
    /* 10-bit Linear protocol — 3 hex digits but only the low 10 bits
     * are significant.  If user sets all digits to F, value should be
     * masked to 10 bits = 0x3FF. */
    subghz_hex_editor_t ed;
    subghz_hex_editor_init(&ed, 10U);

    /* Manually poke all 3 digits to F */
    ed.digits[0] = 0xFU;
    ed.digits[1] = 0xFU;
    ed.digits[2] = 0xFU;

    TEST_ASSERT_EQUAL_HEX64(0x3FFULL, subghz_hex_editor_value(&ed));
}

/* ----------------------- main ------------------------------------- */

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_init_zeroes_digits_and_cursor);
    RUN_TEST(test_init_rounds_up_to_hex_digit_boundary);
    RUN_TEST(test_init_clamps_extreme_bit_counts);
    RUN_TEST(test_init_null_safe);

    RUN_TEST(test_set_value_then_value_roundtrip);
    RUN_TEST(test_set_value_masks_high_bits);
    RUN_TEST(test_set_value_full_64_bits);
    RUN_TEST(test_value_null_returns_zero);

    RUN_TEST(test_left_saturates_at_zero);
    RUN_TEST(test_right_saturates_at_last_digit);
    RUN_TEST(test_cursor_movement_null_safe);

    RUN_TEST(test_up_wraps_F_to_zero);
    RUN_TEST(test_down_wraps_zero_to_F);
    RUN_TEST(test_up_down_only_affects_cursor_digit);

    RUN_TEST(test_end_to_end_build_key_0xAB12);
    RUN_TEST(test_load_then_modify_one_digit);
    RUN_TEST(test_value_assembly_masks_to_bit_count);

    return UNITY_END();
}
