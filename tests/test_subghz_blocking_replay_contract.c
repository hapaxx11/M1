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
    int written;
    FILE *fp;
    long size;
    char *buf;

    TEST_ASSERT_NOT_NULL(relpath);
    TEST_ASSERT_NOT_EQUAL('/', relpath[0]);
    TEST_ASSERT_NULL_MESSAGE(strstr(relpath, ".."),
                             "test helper only accepts repo-relative source paths");
    written = snprintf(path, sizeof(path), "%s/%s", M1_ROOT, relpath);
    TEST_ASSERT_TRUE_MESSAGE(written > 0 && (size_t)written < sizeof(path),
                             "source path exceeded fixed test buffer");
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

    /* This suite is intentionally source-level: the production contract is
     * "async abort preserves main_q_hdl, blocking wrapper flushes it".  The
     * TX path is hardware-coupled, so we pin the relevant function regions in
     * source instead of attempting to execute the STM32-only code on host. */
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
        int ch = (unsigned char)input[i];
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
    /* Keep the end marker aligned with the helper order in m1_sub_ghz.c.
     * If these functions move during refactoring, update both markers rather
     * than weakening the source-level contract this test is pinning. */
    char *abort_fn = slice_between(content,
                                   "void sub_ghz_replay_abort(void)",
                                   "bool sub_ghz_replay_async_is_active(void)");
    char *abort_norm = normalize_ws(abort_fn);

    TEST_ASSERT_NULL_MESSAGE(strstr(abort_norm, "xQueueReset(main_q_hdl);"),
                             "async abort must not reset the shared main queue");
    TEST_ASSERT_NULL_MESSAGE(strstr(abort_norm, "sub_ghz_tx_raw_deinit();"),
                             "async abort must not call the resetting raw TX deinit wrapper");
    TEST_ASSERT_NULL_MESSAGE(strstr(abort_norm, "sub_ghz_tx_raw_deinit_impl(true);"),
                             "async abort must not invoke raw TX deinit with queue reset enabled");

    free(abort_norm);
    free(abort_fn);
    free(content);
}

void test_blocking_wrapper_flushes_main_queue_before_returning(void)
{
    char *content = read_file("m1_csrc/m1_sub_ghz.c");
    /* The end marker intentionally brackets the whole blocking-wrapper region.
     * If helper ordering changes, update these markers so the test keeps
     * checking the wrapper contract instead of silently widening its scope. */
    char *helper = slice_between(content,
                                 "static void subghz_replay_blocking_reset_queue(void)",
                                 "static uint8_t subghz_replay_run_blocking(void)");
    char *wrapper = slice_between(content,
                                  "static uint8_t subghz_replay_run_blocking(void)",
                                  "uint8_t sub_ghz_replay_datafile(");
    char *content_norm = normalize_ws(content);
    char *helper_norm = normalize_ws(helper);
    char *wrapper_norm = normalize_ws(wrapper);

    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(content_norm,
                                        "static void subghz_replay_blocking_reset_queue(void)"),
                                 "missing blocking replay queue-reset helper definition");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(helper_norm, "xQueueReset(main_q_hdl);"),
                                 "blocking replay queue-reset helper must flush main_q_hdl");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(wrapper_norm, "subghz_replay_blocking_reset_queue();"),
                                 "blocking replay wrapper must flush queued events");

    free(wrapper_norm);
    free(helper_norm);
    free(content_norm);
    free(wrapper);
    free(helper);
    free(content);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_async_abort_does_not_reset_main_queue);
    RUN_TEST(test_blocking_wrapper_flushes_main_queue_before_returning);
    return UNITY_END();
}
