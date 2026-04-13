/* See COPYING.txt for license details. */

/*
 * test_file_browser.c
 *
 * Unit tests for file browser pure-logic functions:
 *   - fb_char_lower() — case conversion
 *   - fb_entry_cmp() — qsort comparator (directories first, case-insensitive)
 *   - fb_find_ext() — file extension matching
 *   - fb_get_file_type() — file type classification
 *   - fb_dyn_strcat() — dynamic path concatenation
 *
 * Build with the host-side CMake:
 *   cmake -B build-tests -S tests && cmake --build build-tests
 *   ctest --test-dir build-tests --output-on-failure
 */

#include "unity.h"
#include "file_browser_impl.h"
#include <string.h>
#include <stdlib.h>

/* Unity setup / teardown (required stubs) */
void setUp(void) {}
void tearDown(void) {}

/* ===================================================================
 * fb_char_lower
 * =================================================================== */

void test_char_lower_uppercase(void)
{
    TEST_ASSERT_EQUAL_INT('a', fb_char_lower('A'));
    TEST_ASSERT_EQUAL_INT('z', fb_char_lower('Z'));
    TEST_ASSERT_EQUAL_INT('m', fb_char_lower('M'));
}

void test_char_lower_already_lowercase(void)
{
    TEST_ASSERT_EQUAL_INT('a', fb_char_lower('a'));
    TEST_ASSERT_EQUAL_INT('z', fb_char_lower('z'));
}

void test_char_lower_non_alpha(void)
{
    TEST_ASSERT_EQUAL_INT('0', fb_char_lower('0'));
    TEST_ASSERT_EQUAL_INT('.', fb_char_lower('.'));
    TEST_ASSERT_EQUAL_INT('_', fb_char_lower('_'));
    TEST_ASSERT_EQUAL_INT(' ', fb_char_lower(' '));
}

/* ===================================================================
 * fb_entry_cmp — directories-first sort
 * =================================================================== */

static fb_sorted_entry_t make_entry(const char *name, uint8_t is_dir)
{
    fb_sorted_entry_t e;
    memset(&e, 0, sizeof(e));
    strncpy(e.fname, name, FB_FNAME_MAX - 1);
    e.fname[FB_FNAME_MAX - 1] = '\0';
    e.fattrib = is_dir ? FB_AM_DIR : 0;
    return e;
}

void test_entry_cmp_dir_before_file(void)
{
    fb_sorted_entry_t dir = make_entry("beta", 1);
    fb_sorted_entry_t file = make_entry("alpha", 0);
    /* Directory should sort before file regardless of name */
    TEST_ASSERT_TRUE(fb_entry_cmp(&dir, &file) < 0);
    TEST_ASSERT_TRUE(fb_entry_cmp(&file, &dir) > 0);
}

void test_entry_cmp_files_alphabetical(void)
{
    fb_sorted_entry_t a = make_entry("apple.sub", 0);
    fb_sorted_entry_t b = make_entry("banana.sub", 0);
    TEST_ASSERT_TRUE(fb_entry_cmp(&a, &b) < 0);
    TEST_ASSERT_TRUE(fb_entry_cmp(&b, &a) > 0);
}

void test_entry_cmp_case_insensitive(void)
{
    fb_sorted_entry_t upper = make_entry("Alpha.sub", 0);
    fb_sorted_entry_t lower = make_entry("alpha.sub", 0);
    TEST_ASSERT_EQUAL_INT(0, fb_entry_cmp(&upper, &lower));
}

void test_entry_cmp_dirs_alphabetical(void)
{
    fb_sorted_entry_t d1 = make_entry("IR", 1);
    fb_sorted_entry_t d2 = make_entry("SUBGHZ", 1);
    TEST_ASSERT_TRUE(fb_entry_cmp(&d1, &d2) < 0);
}

void test_entry_cmp_equal_entries(void)
{
    fb_sorted_entry_t a = make_entry("test.sub", 0);
    fb_sorted_entry_t b = make_entry("test.sub", 0);
    TEST_ASSERT_EQUAL_INT(0, fb_entry_cmp(&a, &b));
}

void test_entry_cmp_prefix(void)
{
    /* "test" should sort before "testing" */
    fb_sorted_entry_t shorter = make_entry("test", 0);
    fb_sorted_entry_t longer = make_entry("testing", 0);
    TEST_ASSERT_TRUE(fb_entry_cmp(&shorter, &longer) < 0);
    TEST_ASSERT_TRUE(fb_entry_cmp(&longer, &shorter) > 0);
}

void test_entry_cmp_qsort_integration(void)
{
    /* Sort a realistic file listing and verify order */
    fb_sorted_entry_t entries[] = {
        make_entry("zebra.sub", 0),
        make_entry("NFC", 1),
        make_entry("alpha.sub", 0),
        make_entry("IR", 1),
        make_entry("Beta.sub", 0),
        make_entry("SUBGHZ", 1),
    };
    qsort(entries, 6, sizeof(fb_sorted_entry_t), fb_entry_cmp);

    /* Directories first (alphabetical): IR, NFC, SUBGHZ */
    TEST_ASSERT_EQUAL_STRING("IR", entries[0].fname);
    TEST_ASSERT_EQUAL_STRING("NFC", entries[1].fname);
    TEST_ASSERT_EQUAL_STRING("SUBGHZ", entries[2].fname);
    /* Files next (alphabetical): alpha, Beta, zebra */
    TEST_ASSERT_EQUAL_STRING("alpha.sub", entries[3].fname);
    TEST_ASSERT_EQUAL_STRING("Beta.sub", entries[4].fname);
    TEST_ASSERT_EQUAL_STRING("zebra.sub", entries[5].fname);
}

/* ===================================================================
 * fb_find_ext — extension matching
 * =================================================================== */

void test_find_ext_match(void)
{
    char name[] = "data.log";
    TEST_ASSERT_EQUAL_UINT8(1, fb_find_ext(name, ".log.LOG.txt.TXT"));
}

void test_find_ext_uppercase_match(void)
{
    char name[] = "DATA.LOG";
    TEST_ASSERT_EQUAL_UINT8(1, fb_find_ext(name, ".log.LOG.txt.TXT"));
}

void test_find_ext_no_match(void)
{
    char name[] = "firmware.bin";
    TEST_ASSERT_EQUAL_UINT8(0, fb_find_ext(name, ".log.LOG.txt.TXT"));
}

void test_find_ext_no_dot(void)
{
    char name[] = "README";
    TEST_ASSERT_EQUAL_UINT8(0, fb_find_ext(name, ".log.LOG.txt.TXT"));
}

void test_find_ext_dot_only(void)
{
    /* Files with a dot at position 0 (like ".a") are rejected because
     * the extension search finds k==1, filtering out hidden/dotfiles
     * and malformed filenames with no base name before the dot. */
    char name[] = ".a";
    TEST_ASSERT_EQUAL_UINT8(0, fb_find_ext(name, ".a"));
}

void test_find_ext_subghz(void)
{
    char name[] = "garage_door.sub";
    TEST_ASSERT_EQUAL_UINT8(1, fb_find_ext(name, ".sub.SUB.sgh.SGH"));
}

void test_find_ext_multiple_dots(void)
{
    char name[] = "backup.2024.log";
    TEST_ASSERT_EQUAL_UINT8(1, fb_find_ext(name, ".log.LOG"));
}

/* ===================================================================
 * fb_get_file_type — file classification
 * =================================================================== */

void test_get_file_type_data(void)
{
    char name[] = "output.log";
    TEST_ASSERT_EQUAL_INT(FB_F_EXT_DATA, fb_get_file_type(name));
}

void test_get_file_type_data_txt(void)
{
    char name[] = "notes.txt";
    TEST_ASSERT_EQUAL_INT(FB_F_EXT_DATA, fb_get_file_type(name));
}

void test_get_file_type_other(void)
{
    char name[] = "firmware.bin";
    TEST_ASSERT_EQUAL_INT(FB_F_EXT_OTHER, fb_get_file_type(name));
}

void test_get_file_type_sub(void)
{
    char name[] = "signal.sub";
    TEST_ASSERT_EQUAL_INT(FB_F_EXT_OTHER, fb_get_file_type(name));
}

/* ===================================================================
 * fb_dyn_strcat — path concatenation
 * =================================================================== */

void test_dyn_strcat_single(void)
{
    char buffer[128];
    uint8_t len = fb_dyn_strcat(buffer, 1, "%s", "hello");
    TEST_ASSERT_EQUAL_STRING("hello", buffer);
    TEST_ASSERT_EQUAL_UINT8(5, len);
}

void test_dyn_strcat_two_parts(void)
{
    char buffer[128];
    uint8_t len = fb_dyn_strcat(buffer, 2, "%s%s", "0:", "SUBGHZ");
    TEST_ASSERT_EQUAL_STRING("0:/SUBGHZ", buffer);
    TEST_ASSERT_EQUAL_UINT8(9, len);
}

void test_dyn_strcat_three_parts(void)
{
    char buffer[128];
    uint8_t len = fb_dyn_strcat(buffer, 3, "%s%s%s", "0:", "SUBGHZ", "test.sub");
    TEST_ASSERT_EQUAL_STRING("0:/SUBGHZ/test.sub", buffer);
    TEST_ASSERT_EQUAL_UINT8(18, len);
}

void test_dyn_strcat_empty_middle(void)
{
    /* Empty string in the middle should not produce double separator */
    char buffer[128];
    uint8_t len = fb_dyn_strcat(buffer, 3, "%s%s%s", "0:", "", "test.sub");
    /* The empty string is skipped, so we get "0:/test.sub" */
    TEST_ASSERT_EQUAL_STRING("0:/test.sub", buffer);
    TEST_ASSERT_EQUAL_UINT8(11, len);
}

/* ===================================================================
 * Runner
 * =================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* fb_char_lower */
    RUN_TEST(test_char_lower_uppercase);
    RUN_TEST(test_char_lower_already_lowercase);
    RUN_TEST(test_char_lower_non_alpha);

    /* fb_entry_cmp */
    RUN_TEST(test_entry_cmp_dir_before_file);
    RUN_TEST(test_entry_cmp_files_alphabetical);
    RUN_TEST(test_entry_cmp_case_insensitive);
    RUN_TEST(test_entry_cmp_dirs_alphabetical);
    RUN_TEST(test_entry_cmp_equal_entries);
    RUN_TEST(test_entry_cmp_prefix);
    RUN_TEST(test_entry_cmp_qsort_integration);

    /* fb_find_ext */
    RUN_TEST(test_find_ext_match);
    RUN_TEST(test_find_ext_uppercase_match);
    RUN_TEST(test_find_ext_no_match);
    RUN_TEST(test_find_ext_no_dot);
    RUN_TEST(test_find_ext_dot_only);
    RUN_TEST(test_find_ext_subghz);
    RUN_TEST(test_find_ext_multiple_dots);

    /* fb_get_file_type */
    RUN_TEST(test_get_file_type_data);
    RUN_TEST(test_get_file_type_data_txt);
    RUN_TEST(test_get_file_type_other);
    RUN_TEST(test_get_file_type_sub);

    /* fb_dyn_strcat */
    RUN_TEST(test_dyn_strcat_single);
    RUN_TEST(test_dyn_strcat_two_parts);
    RUN_TEST(test_dyn_strcat_three_parts);
    RUN_TEST(test_dyn_strcat_empty_middle);

    return UNITY_END();
}
