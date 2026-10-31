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

static char *read_weather_draw_list(void)
{
    char path[512];
    FILE *fp;
    long size;
    char *source;
    char *function;
    char *end;

    snprintf(path, sizeof(path), "%s/m1_csrc/m1_sub_ghz.c", M1_ROOT);
    fp = fopen(path, "rb");
    TEST_ASSERT_NOT_NULL_MESSAGE(fp, path);
    TEST_ASSERT_EQUAL_INT(0, fseek(fp, 0, SEEK_END));
    size = ftell(fp);
    TEST_ASSERT_GREATER_THAN_INT(0, size);
    TEST_ASSERT_EQUAL_INT(0, fseek(fp, 0, SEEK_SET));

    source = malloc((size_t)size + 1U);
    TEST_ASSERT_NOT_NULL(source);
    TEST_ASSERT_EQUAL_size_t((size_t)size, fread(source, 1U, (size_t)size, fp));
    source[size] = '\0';
    fclose(fp);

    function = strstr(source, "void sub_ghz_weather_draw_list(");
    TEST_ASSERT_NOT_NULL(function);
    end = strstr(function, "\nvoid sub_ghz_weather_station(void)");
    TEST_ASSERT_NOT_NULL(end);
    size = end - function;
    memmove(source, function, (size_t)size);
    source[size] = '\0';
    return source;
}

void test_empty_state_does_not_overprint_button_bar(void)
{
    char *draw_list = read_weather_draw_list();
    const char *back_hint = strstr(draw_list, "\"BACK to exit\"");

    free(draw_list);
    TEST_ASSERT_NULL_MESSAGE(back_hint,
        "The redundant BACK hint overlaps the bottom button bar");
}

void test_standard_button_bar_is_retained(void)
{
    char *draw_list = read_weather_draw_list();
    const char *button_bar = strstr(draw_list, "subghz_button_bar_draw(");

    free(draw_list);
    TEST_ASSERT_NOT_NULL_MESSAGE(button_bar,
        "Weather Station must retain the standard three-column button bar");
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_empty_state_does_not_overprint_button_bar);
    RUN_TEST(test_standard_button_bar_is_retained);
    return UNITY_END();
}
