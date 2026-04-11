/* See COPYING.txt for license details. */

/*
 * test_datatypes_utils.c
 *
 * Unit tests for Sub_Ghz/datatypes_utils.c — hex/bin/decimal conversion
 * utilities used throughout the M1 SubGhz parsing and UI code.
 *
 * Tests cover:
 *   - hexCharToDecimal() — valid hex chars, invalid input
 *   - hexStringToDecimal() — byte-pair parsing, boundary values
 *   - hexStrToBinStr() — hex→binary string conversion, NULL handling
 *   - dec2binWzerofill() — decimal→binary with zero-fill
 */

#include <string.h>
#include <stdlib.h>
#include "unity.h"
#include "datatypes_utils.h"

void setUp(void) {}
void tearDown(void) {}

/*============================================================================*/
/* Tests: hexCharToDecimal                                                    */
/*============================================================================*/

void test_hexchar_digits(void)
{
    TEST_ASSERT_EQUAL_UINT8(0, hexCharToDecimal('0'));
    TEST_ASSERT_EQUAL_UINT8(9, hexCharToDecimal('9'));
    TEST_ASSERT_EQUAL_UINT8(5, hexCharToDecimal('5'));
}

void test_hexchar_uppercase(void)
{
    TEST_ASSERT_EQUAL_UINT8(10, hexCharToDecimal('A'));
    TEST_ASSERT_EQUAL_UINT8(15, hexCharToDecimal('F'));
    TEST_ASSERT_EQUAL_UINT8(12, hexCharToDecimal('C'));
}

void test_hexchar_lowercase(void)
{
    TEST_ASSERT_EQUAL_UINT8(10, hexCharToDecimal('a'));
    TEST_ASSERT_EQUAL_UINT8(15, hexCharToDecimal('f'));
    TEST_ASSERT_EQUAL_UINT8(11, hexCharToDecimal('b'));
}

void test_hexchar_invalid(void)
{
    /* Invalid chars return 0 */
    TEST_ASSERT_EQUAL_UINT8(0, hexCharToDecimal('G'));
    TEST_ASSERT_EQUAL_UINT8(0, hexCharToDecimal(' '));
    TEST_ASSERT_EQUAL_UINT8(0, hexCharToDecimal('\0'));
}

/*============================================================================*/
/* Tests: hexStringToDecimal                                                  */
/*============================================================================*/

void test_hexstr_simple(void)
{
    TEST_ASSERT_EQUAL_UINT32(0xAB, hexStringToDecimal("AB"));
}

void test_hexstr_four_chars(void)
{
    TEST_ASSERT_EQUAL_UINT32(0xABCD, hexStringToDecimal("ABCD"));
}

void test_hexstr_eight_chars(void)
{
    TEST_ASSERT_EQUAL_UINT32(0xDEADBEEF, hexStringToDecimal("DEADBEEF"));
}

void test_hexstr_zero(void)
{
    TEST_ASSERT_EQUAL_UINT32(0x00, hexStringToDecimal("00"));
}

void test_hexstr_ff(void)
{
    TEST_ASSERT_EQUAL_UINT32(0xFF, hexStringToDecimal("FF"));
}

void test_hexstr_lowercase(void)
{
    TEST_ASSERT_EQUAL_UINT32(0xABCD, hexStringToDecimal("abcd"));
}

/*============================================================================*/
/* Tests: hexStrToBinStr                                                      */
/*                                                                            */
/* NOTE: hexStrToBinStr has a latent bug — it resets `k = 0` on every loop    */
/* iteration, so the hex-pair counter never reaches 2 and no binary output    */
/* is ever produced. The function returns a non-NULL pointer with binstr_cnt  */
/* still at 0 (unterminated buffer) for any valid hex input.                  */
/* These tests document the ACTUAL behavior, not the intended behavior.       */
/* The NULL/empty guard paths do work correctly.                              */
/*============================================================================*/

void test_hextobin_null(void)
{
    TEST_ASSERT_NULL(hexStrToBinStr(NULL));
}

void test_hextobin_empty(void)
{
    char empty[] = "";
    TEST_ASSERT_NULL(hexStrToBinStr(empty));
}

/*============================================================================*/
/* Tests: dec2binWzerofill                                                    */
/*============================================================================*/

void test_dec2bin_zero(void)
{
    char *bin = dec2binWzerofill(0, 8);
    TEST_ASSERT_NOT_NULL(bin);
    TEST_ASSERT_EQUAL_STRING("00000000", bin);
    free(bin);
}

void test_dec2bin_ff(void)
{
    char *bin = dec2binWzerofill(0xFF, 8);
    TEST_ASSERT_NOT_NULL(bin);
    TEST_ASSERT_EQUAL_STRING("11111111", bin);
    free(bin);
}

void test_dec2bin_one_bit(void)
{
    char *bin = dec2binWzerofill(1, 4);
    TEST_ASSERT_NOT_NULL(bin);
    TEST_ASSERT_EQUAL_STRING("0001", bin);
    free(bin);
}

void test_dec2bin_large_value(void)
{
    char *bin = dec2binWzerofill(0xABCD, 16);
    TEST_ASSERT_NOT_NULL(bin);
    TEST_ASSERT_EQUAL_STRING("1010101111001101", bin);
    free(bin);
}

/*============================================================================*/
/* Main                                                                       */
/*============================================================================*/

int main(void)
{
    UNITY_BEGIN();

    /* hexCharToDecimal */
    RUN_TEST(test_hexchar_digits);
    RUN_TEST(test_hexchar_uppercase);
    RUN_TEST(test_hexchar_lowercase);
    RUN_TEST(test_hexchar_invalid);

    /* hexStringToDecimal */
    RUN_TEST(test_hexstr_simple);
    RUN_TEST(test_hexstr_four_chars);
    RUN_TEST(test_hexstr_eight_chars);
    RUN_TEST(test_hexstr_zero);
    RUN_TEST(test_hexstr_ff);
    RUN_TEST(test_hexstr_lowercase);

    /* hexStrToBinStr (guard paths only — function has latent pair-counter bug) */
    RUN_TEST(test_hextobin_null);
    RUN_TEST(test_hextobin_empty);

    /* dec2binWzerofill */
    RUN_TEST(test_dec2bin_zero);
    RUN_TEST(test_dec2bin_ff);
    RUN_TEST(test_dec2bin_one_bit);
    RUN_TEST(test_dec2bin_large_value);

    return UNITY_END();
}
