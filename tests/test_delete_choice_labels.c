/* See COPYING.txt for license details. */

#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M1_ROOT
#error "M1_ROOT must be defined by CMake"
#endif

void setUp(void) {}
void tearDown(void) {}

static char *read_file(const char *relpath)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", M1_ROOT, relpath);
    FILE *fp = fopen(path, "rb");
    TEST_ASSERT_NOT_NULL_MESSAGE(fp, path);
    TEST_ASSERT_EQUAL_INT(0, fseek(fp, 0, SEEK_END));
    long size = ftell(fp);
    TEST_ASSERT_GREATER_THAN_INT(0, size);
    TEST_ASSERT_EQUAL_INT(0, fseek(fp, 0, SEEK_SET));
    char *buf = malloc((size_t)size + 1U);
    TEST_ASSERT_NOT_NULL(buf);
    TEST_ASSERT_EQUAL_size_t((size_t)size, fread(buf, 1U, (size_t)size, fp));
    buf[size] = '\0';
    fclose(fp);
    return buf;
}

static void assert_separate_cancel(const char *relpath, unsigned expected)
{
    char *content = read_file(relpath);
    TEST_ASSERT_NULL_MESSAGE(strstr(content, "\"OK  /  Cancel\""), relpath);
    unsigned count = 0;
    const char *cursor = content;
    while ((cursor = strstr(cursor, "\"OK\\nCancel\"")) != NULL) {
        ++count;
        ++cursor;
    }
    TEST_ASSERT_EQUAL_UINT_MESSAGE(expected, count, relpath);
    free(content);
}

void test_ir_custom_delete_choices_have_separate_cancel(void)
{
    assert_separate_cancel("m1_csrc/m1_ir_custom.c", 2);
}

void test_ir_saved_delete_choice_has_separate_cancel(void)
{
    assert_separate_cancel("m1_csrc/m1_ir_universal.c", 1);
}

void test_subghz_raw_delete_choice_has_separate_cancel(void)
{
    assert_separate_cancel("m1_csrc/m1_subghz_scene_more_raw.c", 1);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_ir_custom_delete_choices_have_separate_cancel);
    RUN_TEST(test_ir_saved_delete_choice_has_separate_cancel);
    RUN_TEST(test_subghz_raw_delete_choice_has_separate_cancel);
    return UNITY_END();
}
