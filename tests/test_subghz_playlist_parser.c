/* See COPYING.txt for license details. */

/*
 * test_subghz_playlist_parser.c
 *
 * Unit tests for subghz_playlist_parser — the Flipper→M1 path remapping
 * utility extracted from m1_subghz_scene_playlist.c, plus the new
 * subghz_playlist_parse_delay() function.
 *
 * Tests cover:
 *   - Standard Flipper path remapping (/ext/subghz/... → /SUBGHZ/...)
 *   - Flipper path without /ext/ prefix (/subghz/... → /SUBGHZ/...)
 *   - Non-matching paths passed through unchanged
 *   - Empty and NULL input handling
 *   - Buffer size boundary conditions
 *   - Nested subdirectories preserved
 *   - Case sensitivity
 *   - Delay directive parsing ("# delay: N")
 *
 * Build:
 *   cmake -B build-tests -S tests && cmake --build build-tests
 *   ctest --test-dir build-tests --output-on-failure
 */

#include <string.h>
#include "unity.h"
#include "subghz_playlist_parser.h"

void setUp(void) {}
void tearDown(void) {}

/*============================================================================*/
/* Tests: Standard Flipper path                                               */
/*============================================================================*/

void test_ext_subghz_remapped(void)
{
    char dst[64];
    subghz_remap_flipper_path("/ext/subghz/Tesla_charge.sub", dst, sizeof(dst));
    TEST_ASSERT_EQUAL_STRING("/SUBGHZ/Tesla_charge.sub", dst);
}

void test_ext_subghz_nested(void)
{
    char dst[64];
    subghz_remap_flipper_path("/ext/subghz/Cars/Toyota.sub", dst, sizeof(dst));
    TEST_ASSERT_EQUAL_STRING("/SUBGHZ/Cars/Toyota.sub", dst);
}

/*============================================================================*/
/* Tests: Without /ext/ prefix                                                */
/*============================================================================*/

void test_subghz_without_ext_remapped(void)
{
    char dst[64];
    subghz_remap_flipper_path("/subghz/garage.sub", dst, sizeof(dst));
    TEST_ASSERT_EQUAL_STRING("/SUBGHZ/garage.sub", dst);
}

/*============================================================================*/
/* Tests: Non-matching paths                                                  */
/*============================================================================*/

void test_nonmatching_path_passthrough(void)
{
    char dst[64];
    subghz_remap_flipper_path("/SUBGHZ/already_correct.sub", dst, sizeof(dst));
    TEST_ASSERT_EQUAL_STRING("/SUBGHZ/already_correct.sub", dst);
}

void test_root_path_passthrough(void)
{
    char dst[64];
    subghz_remap_flipper_path("/some/other/path.sub", dst, sizeof(dst));
    TEST_ASSERT_EQUAL_STRING("/some/other/path.sub", dst);
}

void test_ext_non_subghz_passthrough(void)
{
    /* /ext/infrared/ should NOT be remapped — only /ext/subghz/ */
    char dst[64];
    subghz_remap_flipper_path("/ext/infrared/tv.ir", dst, sizeof(dst));
    TEST_ASSERT_EQUAL_STRING("/infrared/tv.ir", dst);
}

/*============================================================================*/
/* Tests: Case sensitivity                                                    */
/*============================================================================*/

void test_uppercase_subghz_not_remapped(void)
{
    /* /ext/SUBGHZ/ (already uppercase) — /ext/ stripped but /SUBGHZ/ kept */
    char dst[64];
    subghz_remap_flipper_path("/ext/SUBGHZ/file.sub", dst, sizeof(dst));
    /* After stripping /ext/, becomes /SUBGHZ/file.sub which doesn't match
     * /subghz/ (lowercase), so it passes through */
    TEST_ASSERT_EQUAL_STRING("/SUBGHZ/file.sub", dst);
}

/*============================================================================*/
/* Tests: Buffer boundary                                                     */
/*============================================================================*/

void test_small_buffer_truncated(void)
{
    char dst[10];
    subghz_remap_flipper_path("/ext/subghz/long_name.sub", dst, sizeof(dst));
    /* snprintf truncates to 9 chars + NUL */
    TEST_ASSERT_EQUAL_UINT8('\0', dst[9]);
    TEST_ASSERT_TRUE(strlen(dst) < 10);
}

void test_exact_fit(void)
{
    /* "/SUBGHZ/x" = 9 chars + NUL = 10 */
    char dst[10];
    subghz_remap_flipper_path("/ext/subghz/x", dst, sizeof(dst));
    TEST_ASSERT_EQUAL_STRING("/SUBGHZ/x", dst);
}

/*============================================================================*/
/* Tests: Empty string                                                        */
/*============================================================================*/

void test_empty_string(void)
{
    char dst[64] = "garbage";
    subghz_remap_flipper_path("", dst, sizeof(dst));
    TEST_ASSERT_EQUAL_STRING("", dst);
}

/*============================================================================*/
/* Tests: Delay directive parsing                                             */
/*============================================================================*/

void test_delay_basic(void)
{
    uint16_t ms = 9999;
    TEST_ASSERT_TRUE(subghz_playlist_parse_delay("# delay: 500", &ms));
    TEST_ASSERT_EQUAL_UINT16(500, ms);
}

void test_delay_no_space_after_colon(void)
{
    uint16_t ms = 9999;
    TEST_ASSERT_TRUE(subghz_playlist_parse_delay("# delay:1000", &ms));
    TEST_ASSERT_EQUAL_UINT16(1000, ms);
}

void test_delay_uppercase_D(void)
{
    uint16_t ms = 9999;
    TEST_ASSERT_TRUE(subghz_playlist_parse_delay("# Delay: 250", &ms));
    TEST_ASSERT_EQUAL_UINT16(250, ms);
}

void test_delay_zero(void)
{
    uint16_t ms = 9999;
    TEST_ASSERT_TRUE(subghz_playlist_parse_delay("# delay: 0", &ms));
    TEST_ASSERT_EQUAL_UINT16(0, ms);
}

void test_delay_max_clamped(void)
{
    uint16_t ms = 0;
    TEST_ASSERT_TRUE(subghz_playlist_parse_delay("# delay: 99999", &ms));
    TEST_ASSERT_EQUAL_UINT16(60000, ms);
}

void test_delay_not_a_comment(void)
{
    uint16_t ms = 9999;
    TEST_ASSERT_FALSE(subghz_playlist_parse_delay("delay: 500", &ms));
    TEST_ASSERT_EQUAL_UINT16(9999, ms);  /* unchanged */
}

void test_delay_non_delay_comment(void)
{
    uint16_t ms = 9999;
    TEST_ASSERT_FALSE(subghz_playlist_parse_delay("# UberGuidoZ playlist v1", &ms));
    TEST_ASSERT_EQUAL_UINT16(9999, ms);
}

void test_delay_sub_line_not_delay(void)
{
    uint16_t ms = 9999;
    TEST_ASSERT_FALSE(subghz_playlist_parse_delay("sub: /SUBGHZ/foo.sub", &ms));
    TEST_ASSERT_EQUAL_UINT16(9999, ms);
}

void test_delay_null_output(void)
{
    /* Should not crash with NULL output pointer */
    TEST_ASSERT_FALSE(subghz_playlist_parse_delay("# delay: 500", NULL));
}

void test_delay_null_line(void)
{
    uint16_t ms = 9999;
    TEST_ASSERT_FALSE(subghz_playlist_parse_delay(NULL, &ms));
}

void test_delay_whitespace_before_hash(void)
{
    /* Whitespace before '#' means it's not a comment line */
    uint16_t ms = 9999;
    TEST_ASSERT_FALSE(subghz_playlist_parse_delay("  # delay: 300", &ms));
}

/*============================================================================*/
/* Main                                                                       */
/*============================================================================*/

int main(void)
{
    UNITY_BEGIN();

    /* Standard Flipper paths */
    RUN_TEST(test_ext_subghz_remapped);
    RUN_TEST(test_ext_subghz_nested);

    /* Without /ext/ prefix */
    RUN_TEST(test_subghz_without_ext_remapped);

    /* Non-matching */
    RUN_TEST(test_nonmatching_path_passthrough);
    RUN_TEST(test_root_path_passthrough);
    RUN_TEST(test_ext_non_subghz_passthrough);

    /* Case sensitivity */
    RUN_TEST(test_uppercase_subghz_not_remapped);

    /* Buffer boundaries */
    RUN_TEST(test_small_buffer_truncated);
    RUN_TEST(test_exact_fit);

    /* Empty string */
    RUN_TEST(test_empty_string);

    /* Delay directive parsing */
    RUN_TEST(test_delay_basic);
    RUN_TEST(test_delay_no_space_after_colon);
    RUN_TEST(test_delay_uppercase_D);
    RUN_TEST(test_delay_zero);
    RUN_TEST(test_delay_max_clamped);
    RUN_TEST(test_delay_not_a_comment);
    RUN_TEST(test_delay_non_delay_comment);
    RUN_TEST(test_delay_sub_line_not_delay);
    RUN_TEST(test_delay_null_output);
    RUN_TEST(test_delay_null_line);
    RUN_TEST(test_delay_whitespace_before_hash);

    return UNITY_END();
}
