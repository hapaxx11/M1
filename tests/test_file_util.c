/* See COPYING.txt for license details. */

/**
 * @file  test_file_util.c
 * @brief Unit tests for pure-logic path manipulation in m1_file_util.c.
 *
 * Tests fu_get_file_extension(), fu_get_filename(), fu_get_filename_without_ext(),
 * fu_get_directory_path(), and fu_path_combine().
 *
 * The FatFS-dependent functions (fs_file_exists, fs_directory_exists, etc.)
 * are NOT tested here — they require a real filesystem.
 */

#include "unity.h"
#include <string.h>

/* We include the file_util header — the FatFS-dependent functions won't be
 * linked because we only compile the source functions we need via a stub. */
#include "ff.h"
#include "m1_file_util.h"

void setUp(void) { }
void tearDown(void) { }

/*============================================================================*/
/* fu_get_file_extension                                                      */
/*============================================================================*/

void test_extension_normal(void)
{
    TEST_ASSERT_EQUAL_STRING("txt", fu_get_file_extension("readme.txt"));
}

void test_extension_dotfile(void)
{
    /* Leading dot with no other dot → no extension */
    TEST_ASSERT_NULL(fu_get_file_extension(".gitignore"));
}

void test_extension_double(void)
{
    TEST_ASSERT_EQUAL_STRING("gz", fu_get_file_extension("archive.tar.gz"));
}

void test_extension_no_ext(void)
{
    TEST_ASSERT_NULL(fu_get_file_extension("Makefile"));
}

void test_extension_null(void)
{
    TEST_ASSERT_NULL(fu_get_file_extension(NULL));
}

void test_extension_empty(void)
{
    TEST_ASSERT_NULL(fu_get_file_extension(""));
}

void test_extension_trailing_dot(void)
{
    /* "file." → extension is empty string "" */
    const char *ext = fu_get_file_extension("file.");
    TEST_ASSERT_NOT_NULL(ext);
    TEST_ASSERT_EQUAL_STRING("", ext);
}

void test_extension_sub(void)
{
    TEST_ASSERT_EQUAL_STRING("sub", fu_get_file_extension("0:/SUBGHZ/capture.sub"));
}

/*============================================================================*/
/* fu_get_filename                                                            */
/*============================================================================*/

void test_filename_unix_path(void)
{
    TEST_ASSERT_EQUAL_STRING("test.bin", fu_get_filename("/home/user/data/test.bin"));
}

void test_filename_windows_path(void)
{
    TEST_ASSERT_EQUAL_STRING("config.ini", fu_get_filename("C:\\Windows\\config.ini"));
}

void test_filename_sd_path(void)
{
    TEST_ASSERT_EQUAL_STRING("capture.sgh", fu_get_filename("0:/SUBGHZ/capture.sgh"));
}

void test_filename_no_sep(void)
{
    TEST_ASSERT_EQUAL_STRING("readme.md", fu_get_filename("readme.md"));
}

void test_filename_null(void)
{
    TEST_ASSERT_NULL(fu_get_filename(NULL));
}

void test_filename_trailing_sep(void)
{
    /* Trailing slash → empty filename after it */
    const char *f = fu_get_filename("/data/logs/");
    TEST_ASSERT_EQUAL_STRING("", f);
}

void test_filename_mixed_sep(void)
{
    /* Mixed separators — should pick the last one */
    TEST_ASSERT_EQUAL_STRING("file.txt", fu_get_filename("path/to\\file.txt"));
}

/*============================================================================*/
/* fu_get_filename_without_ext                                                */
/*============================================================================*/

void test_fname_noext_normal(void)
{
    char buf[64];
    fu_get_filename_without_ext("/folder/test.txt", buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("test", buf);
}

void test_fname_noext_no_ext(void)
{
    char buf[64];
    fu_get_filename_without_ext("no_extension", buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("no_extension", buf);
}

void test_fname_noext_double_ext(void)
{
    char buf[64];
    fu_get_filename_without_ext("/home/archive.tar.gz", buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("archive.tar", buf);
}

void test_fname_noext_dotfile(void)
{
    char buf[64];
    fu_get_filename_without_ext("/home/.gitignore", buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING(".gitignore", buf);
}

void test_fname_noext_sd_path(void)
{
    char buf[64];
    fu_get_filename_without_ext("0:/SUBGHZ/signal.sub", buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("signal", buf);
}

void test_fname_noext_null_path(void)
{
    char buf[64] = "untouched";
    fu_get_filename_without_ext(NULL, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("untouched", buf);
}

void test_fname_noext_null_out(void)
{
    /* Should not crash */
    fu_get_filename_without_ext("test.txt", NULL, 64);
}

void test_fname_noext_zero_size(void)
{
    char buf[64] = "untouched";
    fu_get_filename_without_ext("test.txt", buf, 0);
    TEST_ASSERT_EQUAL_STRING("untouched", buf);
}

void test_fname_noext_small_buf(void)
{
    char buf[4];
    fu_get_filename_without_ext("longfilename.txt", buf, sizeof(buf));
    /* Should be truncated to fit */
    TEST_ASSERT_EQUAL(3, strlen(buf));
    TEST_ASSERT_EQUAL_STRING("lon", buf);
}

void test_fname_noext_windows(void)
{
    char buf[64];
    fu_get_filename_without_ext("C:\\folder\\test.txt", buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("test", buf);
}

/*============================================================================*/
/* fu_get_directory_path                                                      */
/*============================================================================*/

void test_dirpath_unix(void)
{
    char buf[64];
    fu_get_directory_path("/home/user/test.txt", buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("/home/user", buf);
}

void test_dirpath_sd(void)
{
    char buf[64];
    fu_get_directory_path("0:/SUBGHZ/capture.sgh", buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("0:/SUBGHZ", buf);
}

void test_dirpath_windows(void)
{
    char buf[64];
    fu_get_directory_path("C:\\data\\file.bin", buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("C:\\data", buf);
}

void test_dirpath_no_sep(void)
{
    char buf[64] = "initial";
    fu_get_directory_path("readme.md", buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("", buf);
}

void test_dirpath_null(void)
{
    char buf[64] = "unchanged";
    fu_get_directory_path(NULL, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("", buf);
}

void test_dirpath_null_out(void)
{
    /* Should not crash */
    fu_get_directory_path("/test/file.c", NULL, 64);
}

void test_dirpath_zero_size(void)
{
    char buf[64] = "unchanged";
    fu_get_directory_path("/test/file.c", buf, 0);
    /* outDir[0] is set to '\0' by the NULL guard even when dirSize is 0 */
    TEST_ASSERT_EQUAL_STRING("", buf);
}

void test_dirpath_small_buf(void)
{
    char buf[4];
    fu_get_directory_path("/very/long/path/file.txt", buf, sizeof(buf));
    /* Should be truncated but null-terminated */
    TEST_ASSERT_EQUAL(3, strlen(buf));
}

/*============================================================================*/
/* fu_path_combine                                                            */
/*============================================================================*/

void test_combine_normal(void)
{
    char buf[128];
    fu_path_combine(buf, sizeof(buf), "0:/SUBGHZ", "capture.sub");
    TEST_ASSERT_EQUAL_STRING("0:/SUBGHZ/capture.sub", buf);
}

void test_combine_trailing_sep(void)
{
    char buf[128];
    fu_path_combine(buf, sizeof(buf), "0:/SUBGHZ/", "capture.sub");
    TEST_ASSERT_EQUAL_STRING("0:/SUBGHZ/capture.sub", buf);
}

void test_combine_null_path(void)
{
    char buf[128];
    fu_path_combine(buf, sizeof(buf), NULL, "readme.md");
    TEST_ASSERT_EQUAL_STRING("readme.md", buf);
}

void test_combine_null_file(void)
{
    char buf[128];
    fu_path_combine(buf, sizeof(buf), "/usr/local/bin", NULL);
    TEST_ASSERT_EQUAL_STRING("/usr/local/bin", buf);
}

void test_combine_both_null(void)
{
    char buf[128] = "old";
    fu_path_combine(buf, sizeof(buf), NULL, NULL);
    TEST_ASSERT_EQUAL_STRING("", buf);
}

void test_combine_both_empty(void)
{
    char buf[128] = "old";
    fu_path_combine(buf, sizeof(buf), "", "");
    TEST_ASSERT_EQUAL_STRING("", buf);
}

void test_combine_empty_path(void)
{
    char buf[128];
    fu_path_combine(buf, sizeof(buf), "", "test.txt");
    TEST_ASSERT_EQUAL_STRING("test.txt", buf);
}

void test_combine_empty_file(void)
{
    char buf[128];
    fu_path_combine(buf, sizeof(buf), "/data", "");
    TEST_ASSERT_EQUAL_STRING("/data", buf);
}

void test_combine_absolute_file(void)
{
    char buf[128];
    fu_path_combine(buf, sizeof(buf), "/data", "/logs/output.txt");
    TEST_ASSERT_EQUAL_STRING("/logs/output.txt", buf);
}

void test_combine_null_out(void)
{
    /* Should not crash */
    fu_path_combine(NULL, 128, "/data", "file.txt");
}

void test_combine_zero_size(void)
{
    char buf[128] = "old";
    fu_path_combine(buf, 0, "/data", "file.txt");
    TEST_ASSERT_EQUAL_STRING("old", buf);
}

/*============================================================================*/
/* main                                                                       */
/*============================================================================*/

int main(void)
{
    UNITY_BEGIN();

    /* fu_get_file_extension */
    RUN_TEST(test_extension_normal);
    RUN_TEST(test_extension_dotfile);
    RUN_TEST(test_extension_double);
    RUN_TEST(test_extension_no_ext);
    RUN_TEST(test_extension_null);
    RUN_TEST(test_extension_empty);
    RUN_TEST(test_extension_trailing_dot);
    RUN_TEST(test_extension_sub);

    /* fu_get_filename */
    RUN_TEST(test_filename_unix_path);
    RUN_TEST(test_filename_windows_path);
    RUN_TEST(test_filename_sd_path);
    RUN_TEST(test_filename_no_sep);
    RUN_TEST(test_filename_null);
    RUN_TEST(test_filename_trailing_sep);
    RUN_TEST(test_filename_mixed_sep);

    /* fu_get_filename_without_ext */
    RUN_TEST(test_fname_noext_normal);
    RUN_TEST(test_fname_noext_no_ext);
    RUN_TEST(test_fname_noext_double_ext);
    RUN_TEST(test_fname_noext_dotfile);
    RUN_TEST(test_fname_noext_sd_path);
    RUN_TEST(test_fname_noext_null_path);
    RUN_TEST(test_fname_noext_null_out);
    RUN_TEST(test_fname_noext_zero_size);
    RUN_TEST(test_fname_noext_small_buf);
    RUN_TEST(test_fname_noext_windows);

    /* fu_get_directory_path */
    RUN_TEST(test_dirpath_unix);
    RUN_TEST(test_dirpath_sd);
    RUN_TEST(test_dirpath_windows);
    RUN_TEST(test_dirpath_no_sep);
    RUN_TEST(test_dirpath_null);
    RUN_TEST(test_dirpath_null_out);
    RUN_TEST(test_dirpath_zero_size);
    RUN_TEST(test_dirpath_small_buf);

    /* fu_path_combine */
    RUN_TEST(test_combine_normal);
    RUN_TEST(test_combine_trailing_sep);
    RUN_TEST(test_combine_null_path);
    RUN_TEST(test_combine_null_file);
    RUN_TEST(test_combine_both_null);
    RUN_TEST(test_combine_both_empty);
    RUN_TEST(test_combine_empty_path);
    RUN_TEST(test_combine_empty_file);
    RUN_TEST(test_combine_absolute_file);
    RUN_TEST(test_combine_null_out);
    RUN_TEST(test_combine_zero_size);

    return UNITY_END();
}
