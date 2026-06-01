/* See COPYING.txt for license details. */

/**
 * test_wifi_status_msg.c
 *
 * Unit tests for the pure-logic helpers in wifi_status_msg.h / wifi_status_msg.c:
 *   wifi_status_msg_set()     — populate a timed status message
 *   wifi_status_msg_clear()   — reset to inactive
 *   wifi_status_msg_active()  — check if active
 *   wifi_status_msg_expired() — tick-comparison expiry check
 *
 * Build with the host-side CMake:
 *   cmake -B build-tests -S tests && cmake --build build-tests
 *   ctest --test-dir build-tests --output-on-failure
 */

#include "unity.h"
#include "wifi_status_msg.h"
#include <string.h>

void setUp(void)    {}
void tearDown(void) {}

/* =========================================================================
 * wifi_status_msg_set — basic population
 * =========================================================================*/

static void test_set_populates_all_fields(void)
{
    wifi_status_msg_t msg;
    memset(&msg, 0xFF, sizeof(msg));

    wifi_status_msg_set(&msg, "Title", "Line 1", "Line 2", 2000);

    TEST_ASSERT_EQUAL_STRING("Title", msg.title);
    TEST_ASSERT_EQUAL_STRING("Line 1", msg.line1);
    TEST_ASSERT_EQUAL_STRING("Line 2", msg.line2);
    TEST_ASSERT_EQUAL_UINT32(2000, msg.display_ms);
    TEST_ASSERT_TRUE(msg.active);
}

static void test_set_null_strings_become_empty(void)
{
    wifi_status_msg_t msg;
    wifi_status_msg_set(&msg, NULL, NULL, NULL, 1000);

    TEST_ASSERT_EQUAL_STRING("", msg.title);
    TEST_ASSERT_EQUAL_STRING("", msg.line1);
    TEST_ASSERT_EQUAL_STRING("", msg.line2);
    TEST_ASSERT_TRUE(msg.active);
}

static void test_set_zero_duration_uses_default(void)
{
    wifi_status_msg_t msg;
    wifi_status_msg_set(&msg, "T", NULL, NULL, 0);

    TEST_ASSERT_EQUAL_UINT32(WIFI_STATUS_MSG_DEFAULT_MS, msg.display_ms);
}

static void test_set_truncates_long_title(void)
{
    wifi_status_msg_t msg;
    /* 40 characters — exceeds WIFI_STATUS_MSG_TITLE_MAX (32) */
    wifi_status_msg_set(&msg, "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCD", NULL, NULL, 100);

    TEST_ASSERT_EQUAL(WIFI_STATUS_MSG_TITLE_MAX - 1, strlen(msg.title));
    TEST_ASSERT_EQUAL('\0', msg.title[WIFI_STATUS_MSG_TITLE_MAX - 1]);
}

static void test_set_truncates_long_line(void)
{
    wifi_status_msg_t msg;
    /* 50 characters — exceeds WIFI_STATUS_MSG_LINE_MAX (40) */
    wifi_status_msg_set(&msg, NULL,
                        "01234567890123456789012345678901234567890123456789", NULL, 100);

    TEST_ASSERT_EQUAL(WIFI_STATUS_MSG_LINE_MAX - 1, strlen(msg.line1));
}

static void test_set_null_msg_is_safe(void)
{
    /* Should not crash */
    wifi_status_msg_set(NULL, "Title", "L1", "L2", 1000);
}

/* =========================================================================
 * wifi_status_msg_clear
 * =========================================================================*/

static void test_clear_resets_all_fields(void)
{
    wifi_status_msg_t msg;
    wifi_status_msg_set(&msg, "Title", "Line 1", "Line 2", 2000);
    msg.start_ms = 12345;

    wifi_status_msg_clear(&msg);

    TEST_ASSERT_EQUAL_STRING("", msg.title);
    TEST_ASSERT_EQUAL_STRING("", msg.line1);
    TEST_ASSERT_EQUAL_STRING("", msg.line2);
    TEST_ASSERT_EQUAL_UINT32(0, msg.display_ms);
    TEST_ASSERT_EQUAL_UINT32(0, msg.start_ms);
    TEST_ASSERT_FALSE(msg.active);
}

static void test_clear_null_msg_is_safe(void)
{
    wifi_status_msg_clear(NULL);
}

/* =========================================================================
 * wifi_status_msg_active
 * =========================================================================*/

static void test_active_returns_true_after_set(void)
{
    wifi_status_msg_t msg;
    wifi_status_msg_set(&msg, "T", NULL, NULL, 1000);
    TEST_ASSERT_TRUE(wifi_status_msg_active(&msg));
}

static void test_active_returns_false_after_clear(void)
{
    wifi_status_msg_t msg;
    wifi_status_msg_set(&msg, "T", NULL, NULL, 1000);
    wifi_status_msg_clear(&msg);
    TEST_ASSERT_FALSE(wifi_status_msg_active(&msg));
}

static void test_active_null_msg_returns_false(void)
{
    TEST_ASSERT_FALSE(wifi_status_msg_active(NULL));
}

/* =========================================================================
 * wifi_status_msg_expired — tick-comparison
 * =========================================================================*/

static void test_expired_not_expired_yet(void)
{
    wifi_status_msg_t msg;
    wifi_status_msg_set(&msg, "T", NULL, NULL, 2000);
    msg.start_ms = 1000;

    /* 1500 ms elapsed (< 2000) */
    TEST_ASSERT_FALSE(wifi_status_msg_expired(&msg, 2500));
}

static void test_expired_exactly_at_boundary(void)
{
    wifi_status_msg_t msg;
    wifi_status_msg_set(&msg, "T", NULL, NULL, 2000);
    msg.start_ms = 1000;

    /* Exactly 2000 ms elapsed — should be expired */
    TEST_ASSERT_TRUE(wifi_status_msg_expired(&msg, 3000));
}

static void test_expired_past_boundary(void)
{
    wifi_status_msg_t msg;
    wifi_status_msg_set(&msg, "T", NULL, NULL, 2000);
    msg.start_ms = 1000;

    /* 3000 ms elapsed (> 2000) */
    TEST_ASSERT_TRUE(wifi_status_msg_expired(&msg, 4000));
}

static void test_expired_wraparound(void)
{
    wifi_status_msg_t msg;
    wifi_status_msg_set(&msg, "T", NULL, NULL, 2000);
    /* start near UINT32_MAX */
    msg.start_ms = 0xFFFFFF00U;

    /* now wrapped around: (0x100 - 0xFFFFFF00) = 0x200 = 512 < 2000 */
    TEST_ASSERT_FALSE(wifi_status_msg_expired(&msg, 0x100));

    /* now: (0x900 - 0xFFFFFF00) = 0xA00 = 2560 >= 2000 */
    TEST_ASSERT_TRUE(wifi_status_msg_expired(&msg, 0xFFFFFF00U + 2000));
}

static void test_expired_inactive_returns_true(void)
{
    wifi_status_msg_t msg;
    wifi_status_msg_clear(&msg);

    /* Inactive message is "expired" by convention */
    TEST_ASSERT_TRUE(wifi_status_msg_expired(&msg, 0));
}

static void test_expired_null_msg_returns_true(void)
{
    TEST_ASSERT_TRUE(wifi_status_msg_expired(NULL, 0));
}

static void test_expired_zero_elapsed(void)
{
    wifi_status_msg_t msg;
    wifi_status_msg_set(&msg, "T", NULL, NULL, 100);
    msg.start_ms = 5000;

    /* 0 ms elapsed — not expired */
    TEST_ASSERT_FALSE(wifi_status_msg_expired(&msg, 5000));
}

/* =========================================================================
 * Runner
 * =========================================================================*/

int main(void)
{
    UNITY_BEGIN();

    /* set */
    RUN_TEST(test_set_populates_all_fields);
    RUN_TEST(test_set_null_strings_become_empty);
    RUN_TEST(test_set_zero_duration_uses_default);
    RUN_TEST(test_set_truncates_long_title);
    RUN_TEST(test_set_truncates_long_line);
    RUN_TEST(test_set_null_msg_is_safe);

    /* clear */
    RUN_TEST(test_clear_resets_all_fields);
    RUN_TEST(test_clear_null_msg_is_safe);

    /* active */
    RUN_TEST(test_active_returns_true_after_set);
    RUN_TEST(test_active_returns_false_after_clear);
    RUN_TEST(test_active_null_msg_returns_false);

    /* expired */
    RUN_TEST(test_expired_not_expired_yet);
    RUN_TEST(test_expired_exactly_at_boundary);
    RUN_TEST(test_expired_past_boundary);
    RUN_TEST(test_expired_wraparound);
    RUN_TEST(test_expired_inactive_returns_true);
    RUN_TEST(test_expired_null_msg_returns_true);
    RUN_TEST(test_expired_zero_elapsed);

    return UNITY_END();
}
