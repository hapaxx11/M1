/* See COPYING.txt for license details. */

/*
 * test_wifi_scan_status_nonblocking.c
 *
 * Source-level regression checks for the "Join WiFi hangs on endless AP
 * search" bug.  A transient in-progress status ("Scanning APs...",
 * "Connecting...") shown right before a blocking action must use the
 * non-blocking wifi_draw_message(), NOT wifi_show_message() (which waits for a
 * key press the user does not know to give, so the scan/connect never starts).
 *
 * Verifies that:
 *   - wifi_draw_message() is defined and is non-blocking (no wifi_wait_dismiss)
 *   - the pre-scan "Scanning APs..." statuses use wifi_draw_message
 *   - the pre-connect "Connecting..." statuses use wifi_draw_message
 *   - no transient "Scanning APs..." is shown via the blocking wifi_show_message
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

/* wifi_draw_message() exists and returns without waiting for a key press. */
void test_wifi_draw_message_is_nonblocking(void)
{
    char *src = read_file("m1_csrc/m1_wifi.c");
    const char *fn = strstr(src, "static void wifi_draw_message(");
    TEST_ASSERT_NOT_NULL_MESSAGE(fn, "wifi_draw_message not defined");

    const char *body_start = strchr(fn, '{');
    TEST_ASSERT_NOT_NULL(body_start);

    const char *body_end = NULL;
    int depth = 0;
    for (const char *p = body_start; *p != '\0'; ++p)
    {
        if (*p == '{')
            ++depth;
        else if (*p == '}')
        {
            --depth;
            if (depth == 0)
            {
                body_end = p;
                break;
            }
        }
    }
    TEST_ASSERT_NOT_NULL(body_end);
    size_t len = (size_t)((body_end + 1) - fn);
    char *body = (char *)malloc(len + 1U);
    TEST_ASSERT_NOT_NULL(body);
    memcpy(body, fn, len);
    body[len] = '\0';

    int blocks = (strstr(body, "wifi_wait_dismiss") != NULL);
    free(body);
    free(src);
    TEST_ASSERT_FALSE_MESSAGE(blocks, "wifi_draw_message must not call wifi_wait_dismiss");
}

/* The pre-scan "Scanning APs..." statuses must use the non-blocking draw. */
void test_scanning_status_uses_nonblocking_draw(void)
{
    char *src = read_file("m1_csrc/m1_wifi.c");
    int blocking = (strstr(src, "wifi_show_message(\"Join WiFi\", \"Scanning APs...\"") != NULL) ||
                   (strstr(src, "wifi_show_message(\"Wardrive\", \"Scanning APs...\"") != NULL) ||
                   (strstr(src, "wifi_show_message(\"Save APs\", \"Scanning APs...\"") != NULL);
    int nonblocking = (strstr(src, "wifi_draw_message(\"Join WiFi\", \"Scanning APs...\"") != NULL) &&
                      (strstr(src, "wifi_draw_message(\"Wardrive\", \"Scanning APs...\"") != NULL) &&
                      (strstr(src, "wifi_draw_message(\"Save APs\", \"Scanning APs...\"") != NULL);
    free(src);
    TEST_ASSERT_FALSE_MESSAGE(blocking, "Scanning status must not use blocking wifi_show_message");
    TEST_ASSERT_TRUE_MESSAGE(nonblocking, "Pre-scan statuses should use wifi_draw_message");
}

/* The pre-connect "Connecting..." statuses must use the non-blocking draw. */
void test_connecting_status_uses_nonblocking_draw(void)
{
    char *src = read_file("m1_csrc/m1_wifi.c");
    int blocking = (strstr(src, "wifi_show_message(\"Join WiFi\", \"Connecting...\"") != NULL);
    int nonblocking = (strstr(src, "wifi_draw_message(\"Join WiFi\", \"Connecting...\"") != NULL) &&
                      (strstr(src, "wifi_draw_message(\"Connect\", \"Connecting...\"") != NULL);
    free(src);
    TEST_ASSERT_FALSE_MESSAGE(blocking, "Connecting status must not use blocking wifi_show_message");
    TEST_ASSERT_TRUE_MESSAGE(nonblocking, "Pre-connect statuses should use wifi_draw_message");
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_wifi_draw_message_is_nonblocking);
    RUN_TEST(test_scanning_status_uses_nonblocking_draw);
    RUN_TEST(test_connecting_status_uses_nonblocking_draw);
    return UNITY_END();
}
