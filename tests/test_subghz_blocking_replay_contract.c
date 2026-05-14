/* See COPYING.txt for license details. */

/*
 * test_subghz_blocking_replay_contract.c
 *
 * Source-level regression checks for the split async-vs-blocking replay
 * contract in m1_sub_ghz.c.
 *
 * Read Raw async TX must not reset main_q_hdl in sub_ghz_replay_abort(),
 * but the legacy blocking wrappers (Saved / Playlist / Remote / Bind Wizard)
 * still own a private event loop and must flush queued keypad/TX-complete
 * events before returning to their caller scene.
 */

#include "unity.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M1_ROOT
#error "M1_ROOT must be defined by CMake"
#endif

static char *read_file(const char *relpath)
{
    char path[512];
    FILE *fp;
    long size;
    char *buf;

    snprintf(path, sizeof(path), "%s/%s", M1_ROOT, relpath);
    fp = fopen(path, "rb");
    TEST_ASSERT_NOT_NULL_MESSAGE(fp, path);

    TEST_ASSERT_EQUAL_INT(0, fseek(fp, 0, SEEK_END));
    size = ftell(fp);
    TEST_ASSERT_GREATER_THAN_INT(0, size);
    TEST_ASSERT_EQUAL_INT(0, fseek(fp, 0, SEEK_SET));

    buf = (char *)malloc((size_t)size + 1U);
    TEST_ASSERT_NOT_NULL(buf);
    TEST_ASSERT_EQUAL_size_t((size_t)size, fread(buf, 1U, (size_t)size, fp));
    buf[size] = '\0';
    fclose(fp);
    return buf;
}

static char *slice_between(const char *content, const char *start, const char *end)
{
    const char *p_start = strstr(content, start);
    const char *p_end;
    size_t len;
    char *slice;

    TEST_ASSERT_NOT_NULL_MESSAGE(p_start, start);
    p_end = strstr(p_start, end);
    TEST_ASSERT_NOT_NULL_MESSAGE(p_end, end);
    TEST_ASSERT_TRUE_MESSAGE(p_end > p_start, end);

    len = (size_t)(p_end - p_start);
    slice = (char *)malloc(len + 1U);
    TEST_ASSERT_NOT_NULL(slice);
    memcpy(slice, p_start, len);
    slice[len] = '\0';
    return slice;
}

static char *normalize_ws(const char *input)
{
    size_t len = strlen(input);
    char *out = (char *)malloc(len + 1U);
    size_t j = 0;
    int prev_space = 0;

    TEST_ASSERT_NOT_NULL(out);

    for (size_t i = 0; i < len; i++)
    {
        unsigned char ch = (unsigned char)input[i];
        if (isspace(ch))
        {
            if (!prev_space)
                out[j++] = ' ';
            prev_space = 1;
        }
        else
        {
            out[j++] = (char)ch;
            prev_space = 0;
        }
    }

    if (j > 0 && out[j - 1] == ' ')
        j--;
    out[j] = '\0';
    return out;
}

void setUp(void)
{
}

void tearDown(void)
{
}

void test_async_abort_does_not_reset_main_queue(void)
{
    char *content = read_file("m1_csrc/m1_sub_ghz.c");
    char *abort_fn = slice_between(content,
                                   "void sub_ghz_replay_abort(void)",
                                   "bool sub_ghz_replay_async_is_active(void)");
    char *abort_norm = normalize_ws(abort_fn);

    TEST_ASSERT_NULL_MESSAGE(strstr(abort_norm, "xQueueReset(main_q_hdl);"),
                             "async abort must not reset the shared main queue");

    free(abort_norm);
    free(abort_fn);
    free(content);
}

void test_blocking_wrapper_flushes_main_queue_before_returning(void)
{
    char *content = read_file("m1_csrc/m1_sub_ghz.c");
    char *wrapper = slice_between(content,
                                  "static uint8_t subghz_replay_run_blocking(void)",
                                  "uint8_t sub_ghz_replay_datafile(");
    char *content_norm = normalize_ws(content);
    char *wrapper_norm = normalize_ws(wrapper);

    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(content_norm,
                                        "static void subghz_replay_blocking_reset_queue(void)"),
                                 "missing blocking replay queue-reset helper definition");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(content_norm, "xQueueReset(main_q_hdl);"),
                                 "blocking replay queue-reset helper must flush main_q_hdl");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(wrapper_norm, "subghz_replay_blocking_reset_queue();"),
                                 "blocking replay wrapper must flush queued events");

    free(wrapper_norm);
    free(content_norm);
    free(wrapper);
    free(content);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_async_abort_does_not_reset_main_queue);
    RUN_TEST(test_blocking_wrapper_flushes_main_queue_before_returning);
    return UNITY_END();
}
