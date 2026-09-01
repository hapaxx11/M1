/* See COPYING.txt for license details. */

/**
 * @file   test_espnow_shareable.c
 * @brief  Host-side unit tests for the ESP-NOW capture-sharing helpers.
 */

#include "unity.h"
#include "espnow_shareable.h"

#include <string.h>

void setUp(void)  {}
void tearDown(void) {}

/* =========================================================================
 * classify / is_shareable
 * =========================================================================*/

void test_classify_known_kinds(void)
{
    TEST_ASSERT_EQUAL_INT(ESPNOW_SHARE_KIND_SUBGHZ, espnow_share_classify("foo.sub"));
    TEST_ASSERT_EQUAL_INT(ESPNOW_SHARE_KIND_NFC,    espnow_share_classify("card.nfc"));
    TEST_ASSERT_EQUAL_INT(ESPNOW_SHARE_KIND_RFID,   espnow_share_classify("tag.rfid"));
    TEST_ASSERT_EQUAL_INT(ESPNOW_SHARE_KIND_IR,     espnow_share_classify("tv.ir"));
}

void test_classify_case_insensitive(void)
{
    TEST_ASSERT_EQUAL_INT(ESPNOW_SHARE_KIND_SUBGHZ, espnow_share_classify("FOO.SUB"));
    TEST_ASSERT_EQUAL_INT(ESPNOW_SHARE_KIND_NFC,    espnow_share_classify("Card.Nfc"));
}

void test_classify_with_full_path(void)
{
    TEST_ASSERT_EQUAL_INT(ESPNOW_SHARE_KIND_SUBGHZ,
                          espnow_share_classify("/SUBGHZ/garage/door.sub"));
}

void test_classify_unknown(void)
{
    TEST_ASSERT_EQUAL_INT(ESPNOW_SHARE_KIND_UNKNOWN, espnow_share_classify("notes.txt"));
    TEST_ASSERT_EQUAL_INT(ESPNOW_SHARE_KIND_UNKNOWN, espnow_share_classify("noext"));
    TEST_ASSERT_EQUAL_INT(ESPNOW_SHARE_KIND_UNKNOWN, espnow_share_classify(""));
    TEST_ASSERT_EQUAL_INT(ESPNOW_SHARE_KIND_UNKNOWN, espnow_share_classify(NULL));
}

void test_classify_directory_dot_not_extension(void)
{
    /* A dot in a directory name must not be treated as the file extension. */
    TEST_ASSERT_EQUAL_INT(ESPNOW_SHARE_KIND_UNKNOWN,
                          espnow_share_classify("/my.dir/file"));
}

void test_classify_hidden_file_no_extension(void)
{
    TEST_ASSERT_EQUAL_INT(ESPNOW_SHARE_KIND_UNKNOWN, espnow_share_classify("/dir/.sub"));
}

void test_is_shareable(void)
{
    TEST_ASSERT_TRUE(espnow_share_is_shareable("a.ir"));
    TEST_ASSERT_FALSE(espnow_share_is_shareable("a.exe"));
}

/* =========================================================================
 * basename
 * =========================================================================*/

void test_basename_strips_path(void)
{
    char out[32];
    TEST_ASSERT_TRUE(espnow_share_basename("/SUBGHZ/x/foo.sub", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("foo.sub", out);
}

void test_basename_no_separator(void)
{
    char out[32];
    TEST_ASSERT_TRUE(espnow_share_basename("bare.nfc", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("bare.nfc", out);
}

void test_basename_truncates(void)
{
    char out[4];
    TEST_ASSERT_TRUE(espnow_share_basename("/d/abcdef.sub", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("abc", out);   /* 3 chars + NUL */
}

void test_basename_null_args(void)
{
    char out[4];
    TEST_ASSERT_FALSE(espnow_share_basename(NULL, out, sizeof(out)));
    TEST_ASSERT_FALSE(espnow_share_basename("x", NULL, sizeof(out)));
    TEST_ASSERT_FALSE(espnow_share_basename("x", out, 0));
}

/* =========================================================================
 * name safety
 * =========================================================================*/

void test_name_safe_accepts_plain(void)
{
    TEST_ASSERT_TRUE(espnow_share_name_is_safe("garage.sub", 32));
}

void test_name_safe_rejects_empty_and_long(void)
{
    TEST_ASSERT_FALSE(espnow_share_name_is_safe("", 32));
    TEST_ASSERT_FALSE(espnow_share_name_is_safe("abcdef", 3));
}

void test_name_safe_rejects_traversal(void)
{
    TEST_ASSERT_FALSE(espnow_share_name_is_safe("../etc/passwd", 64));
    TEST_ASSERT_FALSE(espnow_share_name_is_safe("a/b.sub", 64));
    TEST_ASSERT_FALSE(espnow_share_name_is_safe("a\\b.sub", 64));
    TEST_ASSERT_FALSE(espnow_share_name_is_safe("..", 64));
}

void test_name_safe_rejects_hidden(void)
{
    TEST_ASSERT_FALSE(espnow_share_name_is_safe(".hidden", 64));
}

/* =========================================================================
 * recv path
 * =========================================================================*/

void test_recv_path_builds(void)
{
    char out[64];
    TEST_ASSERT_TRUE(espnow_share_recv_path("foo.sub", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("/ESPNOW/foo.sub", out);
}

void test_recv_path_rejects_overflow(void)
{
    char out[8];   /* too small for "/ESPNOW/" + name */
    TEST_ASSERT_FALSE(espnow_share_recv_path("foo.sub", out, sizeof(out)));
}

/* =========================================================================
 * Runner
 * =========================================================================*/

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_classify_known_kinds);
    RUN_TEST(test_classify_case_insensitive);
    RUN_TEST(test_classify_with_full_path);
    RUN_TEST(test_classify_unknown);
    RUN_TEST(test_classify_directory_dot_not_extension);
    RUN_TEST(test_classify_hidden_file_no_extension);
    RUN_TEST(test_is_shareable);
    RUN_TEST(test_basename_strips_path);
    RUN_TEST(test_basename_no_separator);
    RUN_TEST(test_basename_truncates);
    RUN_TEST(test_basename_null_args);
    RUN_TEST(test_name_safe_accepts_plain);
    RUN_TEST(test_name_safe_rejects_empty_and_long);
    RUN_TEST(test_name_safe_rejects_traversal);
    RUN_TEST(test_name_safe_rejects_hidden);
    RUN_TEST(test_recv_path_builds);
    RUN_TEST(test_recv_path_rejects_overflow);
    return UNITY_END();
}
