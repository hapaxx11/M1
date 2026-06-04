/* See COPYING.txt for license details. */

/*
 * test_wifi_haldelay_free.c
 *
 * Source-level regression checks for Phase A-6: HAL_Delay elimination from
 * m1_wifi.c. Verifies that:
 *
 *   - m1_wifi.c contains no HAL_Delay() calls (comments excluded)
 *   - wifi_wait_dismiss() helper is present in m1_wifi.c
 *   - beacon_message() uses wifi_wait_dismiss() instead of HAL_Delay
 *   - wifi_show_message() uses wifi_wait_dismiss() instead of HAL_Delay
 *   - ESP32 boot sequences use vTaskDelay(pdMS_TO_TICKS(...)) for 200/2000 ms
 *   - The station-scan countdown loop uses xQueueReceive with pdMS_TO_TICKS(1000)
 *     instead of HAL_Delay(1000)
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

static void assert_absent(const char *content, const char *needle)
{
    TEST_ASSERT_NULL_MESSAGE(strstr(content, needle), needle);
}

/*
 * Strip C-style line comments so that pattern checks are not confused by
 * occurrences in comment text.  Returns a newly-allocated string; caller must
 * free it.
 */
static char *strip_line_comments(const char *src)
{
    size_t len = strlen(src);
    char *out = (char *)malloc(len + 1U);
    TEST_ASSERT_NOT_NULL(out);

    size_t i = 0, j = 0;
    while (i < len)
    {
        if (src[i] == '/' && src[i + 1] == '/')
        {
            /* skip to end of line */
            while (i < len && src[i] != '\n')
                i++;
        }
        else if (src[i] == '/' && src[i + 1] == '*')
        {
            /* skip block comment */
            i += 2;
            while (i + 1 < len && !(src[i] == '*' && src[i + 1] == '/'))
                i++;
            i += 2;
        }
        else
        {
            out[j++] = src[i++];
        }
    }
    out[j] = '\0';
    return out;
}

/* ------------------------------------------------------------------ */
/* HAL_Delay absence checks                                            */
/* ------------------------------------------------------------------ */

void test_m1_wifi_no_hal_delay_calls(void)
{
    char *raw = read_file("m1_csrc/m1_wifi.c");
    char *src = strip_line_comments(raw);
    free(raw);
    assert_absent(src, "HAL_Delay(");
    free(src);
}

/* ------------------------------------------------------------------ */
/* wifi_wait_dismiss() presence                                        */
/* ------------------------------------------------------------------ */

void test_m1_wifi_wait_dismiss_defined(void)
{
    char *src = read_file("m1_csrc/m1_wifi.c");
    assert_contains(src, "wifi_wait_dismiss(void)");
    free(src);
}

void test_m1_wifi_wait_dismiss_called(void)
{
    char *src = read_file("m1_csrc/m1_wifi.c");
    assert_contains(src, "wifi_wait_dismiss()");
    free(src);
}

/* ------------------------------------------------------------------ */
/* wifi_show_message() uses wifi_wait_dismiss                         */
/* ------------------------------------------------------------------ */

void test_wifi_show_message_uses_wait_dismiss(void)
{
    char *src = read_file("m1_csrc/m1_wifi.c");
    /* The function body must contain wifi_wait_dismiss, not HAL_Delay */
    const char *fn = strstr(src, "wifi_show_message(");
    TEST_ASSERT_NOT_NULL_MESSAGE(fn, "wifi_show_message not found");
    /* Find function closing brace (simple heuristic: look forward) */
    const char *body_end = strstr(fn, "\nstatic ");
    char *body;
    if (body_end)
    {
        size_t len = (size_t)(body_end - fn);
        body = (char *)malloc(len + 1U);
        TEST_ASSERT_NOT_NULL(body);
        memcpy(body, fn, len);
        body[len] = '\0';
    }
    else
    {
        body = strdup(fn);
        TEST_ASSERT_NOT_NULL(body);
    }
    assert_contains(body, "wifi_wait_dismiss()");
    assert_absent(body, "HAL_Delay(");
    free(body);
    free(src);
}

/* ------------------------------------------------------------------ */
/* beacon_message() uses wifi_wait_dismiss                            */
/* ------------------------------------------------------------------ */

void test_beacon_message_uses_wait_dismiss(void)
{
    char *src = read_file("m1_csrc/m1_wifi.c");
    const char *fn = strstr(src, "beacon_message(");
    TEST_ASSERT_NOT_NULL_MESSAGE(fn, "beacon_message not found");
    const char *body_end = strstr(fn, "\nstatic ");
    char *body;
    if (body_end)
    {
        size_t len = (size_t)(body_end - fn);
        body = (char *)malloc(len + 1U);
        TEST_ASSERT_NOT_NULL(body);
        memcpy(body, fn, len);
        body[len] = '\0';
    }
    else
    {
        body = strdup(fn);
        TEST_ASSERT_NOT_NULL(body);
    }
    assert_contains(body, "wifi_wait_dismiss()");
    assert_absent(body, "HAL_Delay(");
    free(body);
    free(src);
}

/* ------------------------------------------------------------------ */
/* ESP32 boot sequences use vTaskDelay                                */
/* ------------------------------------------------------------------ */

void test_esp32_boot_uses_vtaskdelay_200(void)
{
    char *src = read_file("m1_csrc/m1_wifi.c");
    assert_contains(src, "vTaskDelay(pdMS_TO_TICKS(200))");
    free(src);
}

void test_esp32_boot_uses_vtaskdelay_2000(void)
{
    char *src = read_file("m1_csrc/m1_wifi.c");
    assert_contains(src, "vTaskDelay(pdMS_TO_TICKS(2000))");
    free(src);
}

/* ------------------------------------------------------------------ */
/* Countdown scan loop uses xQueueReceive with 1000 ms timeout        */
/* ------------------------------------------------------------------ */

void test_sta_scan_countdown_uses_xqueuereceive(void)
{
    char *src = read_file("m1_csrc/m1_wifi.c");
    /* The countdown loop must use a queue receive timeout of 1000 ms */
    assert_contains(src, "pdMS_TO_TICKS(1000)");
    free(src);
}

void test_sta_scan_countdown_no_hal_delay_1000(void)
{
    char *src = read_file("m1_csrc/m1_wifi.c");
    assert_absent(src, "HAL_Delay(1000)");
    free(src);
}

/* ------------------------------------------------------------------ */

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_m1_wifi_no_hal_delay_calls);
    RUN_TEST(test_m1_wifi_wait_dismiss_defined);
    RUN_TEST(test_m1_wifi_wait_dismiss_called);
    RUN_TEST(test_wifi_show_message_uses_wait_dismiss);
    RUN_TEST(test_beacon_message_uses_wait_dismiss);
    RUN_TEST(test_esp32_boot_uses_vtaskdelay_200);
    RUN_TEST(test_esp32_boot_uses_vtaskdelay_2000);
    RUN_TEST(test_sta_scan_countdown_uses_xqueuereceive);
    RUN_TEST(test_sta_scan_countdown_no_hal_delay_1000);
    return UNITY_END();
}
