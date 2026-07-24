/* See COPYING.txt for license details. */

/*
 * test_badusb_typing_speed.c
 *
 * Source-level regression checks for the Bad-USB typing-speed fix
 * (cherry-picked from bedge117/M1 commit 7ae4c565).
 *
 * Correctness of key-down/key-up ordering is guaranteed by
 * badusb_wait_tx_idle(), which blocks until the host has actually polled
 * (read) each HID report before the next report is queued. The old fixed
 * per-key/per-char osDelay() calls were therefore redundant padding on top
 * of that poll-gating and only slowed typing down (~4x). This test verifies:
 *
 *   - BADUSB_KEY_PRESS_MS / BADUSB_KEY_RELEASE_MS / BADUSB_INTER_CHAR_MS are
 *     all defined as 0 (poll-gated, no extra fixed delay)
 *   - Each of the three osDelay() call sites is compiled out via
 *     "#if BADUSB_*_MS > 0" so a raise of the constant re-enables padding
 *     without needing further source changes
 *   - badusb_wait_tx_idle() is still called from badusb_send_key() /
 *     badusb_release_all(), i.e. the poll-gating that makes the above safe
 *     has not been removed alongside the fixed delays
 */

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

static void assert_contains(const char *content, const char *needle)
{
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(content, needle), needle);
}

/* ------------------------------------------------------------------ */
/* Timing constants are poll-gated (0), not fixed padding              */
/* ------------------------------------------------------------------ */

void test_key_press_ms_is_poll_gated(void)
{
    char *c = read_file("m1_csrc/m1_badusb.c");
    assert_contains(c, "#define BADUSB_KEY_PRESS_MS       0");
    free(c);
}

void test_key_release_ms_is_poll_gated(void)
{
    char *c = read_file("m1_csrc/m1_badusb.c");
    assert_contains(c, "#define BADUSB_KEY_RELEASE_MS     0");
    free(c);
}

void test_inter_char_ms_is_poll_gated(void)
{
    char *c = read_file("m1_csrc/m1_badusb.c");
    assert_contains(c, "#define BADUSB_INTER_CHAR_MS      0");
    free(c);
}

/* ------------------------------------------------------------------ */
/* Delay call sites are compiled out, not unconditional osDelay() calls */
/* ------------------------------------------------------------------ */

void test_key_press_delay_is_conditionally_compiled(void)
{
    char *c = read_file("m1_csrc/m1_badusb.c");
    assert_contains(c, "#if BADUSB_KEY_PRESS_MS > 0\n    osDelay(BADUSB_KEY_PRESS_MS);\n#endif");
    free(c);
}

void test_key_release_delay_is_conditionally_compiled(void)
{
    char *c = read_file("m1_csrc/m1_badusb.c");
    assert_contains(c, "#if BADUSB_KEY_RELEASE_MS > 0\n    osDelay(BADUSB_KEY_RELEASE_MS);\n#endif");
    free(c);
}

void test_inter_char_delay_is_conditionally_compiled(void)
{
    char *c = read_file("m1_csrc/m1_badusb.c");
    /* Appears twice: badusb_type_string() and badusb_type_string_forced() */
    const char *needle = "#if BADUSB_INTER_CHAR_MS > 0\n        osDelay(BADUSB_INTER_CHAR_MS);\n#endif";
    const char *first = strstr(c, needle);
    TEST_ASSERT_NOT_NULL(first);
    const char *second = strstr(first + 1, needle);
    TEST_ASSERT_NOT_NULL(second);
    free(c);
}

/* ------------------------------------------------------------------ */
/* Poll-gating that makes the zero delays safe is still present        */
/* ------------------------------------------------------------------ */

void test_send_key_still_waits_for_tx_idle(void)
{
    char *c = read_file("m1_csrc/m1_badusb.c");
    assert_contains(c, "void badusb_send_key(uint8_t modifier, uint8_t keycode)\n{\n    badusb_wait_tx_idle();");
    free(c);
}

void test_release_all_still_waits_for_tx_idle(void)
{
    char *c = read_file("m1_csrc/m1_badusb.c");
    assert_contains(c, "static void badusb_release_all(void)\n{\n    badusb_wait_tx_idle();");
    free(c);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_key_press_ms_is_poll_gated);
    RUN_TEST(test_key_release_ms_is_poll_gated);
    RUN_TEST(test_inter_char_ms_is_poll_gated);
    RUN_TEST(test_key_press_delay_is_conditionally_compiled);
    RUN_TEST(test_key_release_delay_is_conditionally_compiled);
    RUN_TEST(test_inter_char_delay_is_conditionally_compiled);
    RUN_TEST(test_send_key_still_waits_for_tx_idle);
    RUN_TEST(test_release_all_still_waits_for_tx_idle);
    return UNITY_END();
}
