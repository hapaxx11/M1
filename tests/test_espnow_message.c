/* See COPYING.txt for license details. */

/**
 * @file   test_espnow_message.c
 * @brief  Host-side unit tests for ESP-NOW short-text messaging + inbox ring.
 */

#include "unity.h"
#include "espnow_message.h"

#include <string.h>

void setUp(void)  {}
void tearDown(void) {}

static const uint8_t MAC_A[6] = { 1, 2, 3, 4, 5, 6 };
static const uint8_t MAC_B[6] = { 9, 9, 9, 9, 9, 9 };

/* =========================================================================
 * Framing
 * =========================================================================*/

void test_build_parse_roundtrip(void)
{
    uint8_t frame[ESPNOW_MSG_TEXT_MAX + ESPNOW_MSG_HDR_LEN];
    size_t  flen = 0;
    TEST_ASSERT_TRUE(espnow_msg_build(0x2A, "hello peer", frame, sizeof(frame), &flen));
    TEST_ASSERT_EQUAL_UINT(ESPNOW_MSG_HDR_LEN + 10u, flen);
    TEST_ASSERT_EQUAL_UINT8(ESPNOW_MSG_TYPE_TEXT, frame[0]);

    char text[ESPNOW_MSG_TEXT_MAX + 1];
    uint8_t seq = 0;
    size_t tlen = 0;
    TEST_ASSERT_TRUE(espnow_msg_parse(frame, flen, &seq, text, sizeof(text), &tlen));
    TEST_ASSERT_EQUAL_UINT8(0x2A, seq);
    TEST_ASSERT_EQUAL_STRING("hello peer", text);
    TEST_ASSERT_EQUAL_UINT(10, tlen);
}

void test_build_rejects_empty(void)
{
    uint8_t frame[16];
    size_t flen;
    TEST_ASSERT_FALSE(espnow_msg_build(1, "", frame, sizeof(frame), &flen));
}

void test_build_rejects_too_long(void)
{
    char big[ESPNOW_MSG_TEXT_MAX + 2];
    memset(big, 'x', sizeof(big) - 1);
    big[sizeof(big) - 1] = '\0';
    uint8_t frame[ESPNOW_MSG_TEXT_MAX + ESPNOW_MSG_HDR_LEN + 4];
    size_t flen;
    TEST_ASSERT_FALSE(espnow_msg_build(1, big, frame, sizeof(frame), &flen));
}

void test_build_rejects_small_buffer(void)
{
    uint8_t frame[3];   /* header (2) + 1 byte only */
    size_t flen;
    TEST_ASSERT_FALSE(espnow_msg_build(1, "hello", frame, sizeof(frame), &flen));
}

void test_build_max_length_ok(void)
{
    char big[ESPNOW_MSG_TEXT_MAX + 1];
    memset(big, 'a', ESPNOW_MSG_TEXT_MAX);
    big[ESPNOW_MSG_TEXT_MAX] = '\0';
    uint8_t frame[ESPNOW_MSG_TEXT_MAX + ESPNOW_MSG_HDR_LEN];
    size_t flen;
    TEST_ASSERT_TRUE(espnow_msg_build(7, big, frame, sizeof(frame), &flen));
    TEST_ASSERT_EQUAL_UINT(sizeof(frame), flen);
}

void test_parse_rejects_wrong_type(void)
{
    uint8_t frame[4] = { 0x10 /* file transfer type */, 1, 'h', 'i' };
    char text[16];
    TEST_ASSERT_FALSE(espnow_msg_parse(frame, sizeof(frame), NULL, text, sizeof(text), NULL));
}

void test_parse_rejects_header_only(void)
{
    uint8_t frame[2] = { ESPNOW_MSG_TYPE_TEXT, 5 };
    char text[16];
    TEST_ASSERT_FALSE(espnow_msg_parse(frame, sizeof(frame), NULL, text, sizeof(text), NULL));
}

void test_parse_rejects_embedded_nul(void)
{
    uint8_t frame[5] = { ESPNOW_MSG_TYPE_TEXT, 1, 'a', 0, 'b' };
    char text[16];
    TEST_ASSERT_FALSE(espnow_msg_parse(frame, sizeof(frame), NULL, text, sizeof(text), NULL));
}

void test_parse_rejects_small_out(void)
{
    uint8_t frame[5] = { ESPNOW_MSG_TYPE_TEXT, 1, 'a', 'b', 'c' };
    char text[3];   /* needs 3 chars + NUL = 4 */
    TEST_ASSERT_FALSE(espnow_msg_parse(frame, sizeof(frame), NULL, text, sizeof(text), NULL));
}

/* =========================================================================
 * Inbox ring
 * =========================================================================*/

void test_inbox_push_and_get(void)
{
    espnow_inbox_t ib;
    espnow_inbox_init(&ib);
    TEST_ASSERT_TRUE(espnow_inbox_push(&ib, MAC_A, 1, false, "first"));
    TEST_ASSERT_TRUE(espnow_inbox_push(&ib, MAC_A, 2, true,  "reply"));
    TEST_ASSERT_EQUAL_UINT8(2, ib.count);

    const espnow_msg_entry_t *e0 = espnow_inbox_get(&ib, 0);
    const espnow_msg_entry_t *e1 = espnow_inbox_get(&ib, 1);
    TEST_ASSERT_NOT_NULL(e0);
    TEST_ASSERT_NOT_NULL(e1);
    TEST_ASSERT_EQUAL_STRING("first", e0->text);
    TEST_ASSERT_FALSE(e0->outgoing);
    TEST_ASSERT_EQUAL_STRING("reply", e1->text);
    TEST_ASSERT_TRUE(e1->outgoing);
    TEST_ASSERT_NULL(espnow_inbox_get(&ib, 2));
}

void test_inbox_evicts_oldest_when_full(void)
{
    espnow_inbox_t ib;
    espnow_inbox_init(&ib);
    char buf[8];
    for (int i = 0; i < (int)ESPNOW_INBOX_CAP + 3; ++i) {
        snprintf(buf, sizeof(buf), "m%d", i);
        TEST_ASSERT_TRUE(espnow_inbox_push(&ib, MAC_A, (uint8_t)i, false, buf));
    }
    TEST_ASSERT_EQUAL_UINT8(ESPNOW_INBOX_CAP, ib.count);
    TEST_ASSERT_EQUAL_UINT32(ESPNOW_INBOX_CAP + 3, ib.total);

    /* Oldest retained is message index 3 (0..2 evicted). */
    const espnow_msg_entry_t *oldest = espnow_inbox_get(&ib, 0);
    TEST_ASSERT_EQUAL_STRING("m3", oldest->text);
    /* Newest is the last pushed. */
    const espnow_msg_entry_t *newest = espnow_inbox_get(&ib, (uint8_t)(ib.count - 1));
    char expect[8];
    snprintf(expect, sizeof(expect), "m%d", ESPNOW_INBOX_CAP + 2);
    TEST_ASSERT_EQUAL_STRING(expect, newest->text);
}

void test_inbox_truncates_long_text(void)
{
    espnow_inbox_t ib;
    espnow_inbox_init(&ib);
    char big[ESPNOW_MSG_TEXT_MAX + 10];
    memset(big, 'z', sizeof(big) - 1);
    big[sizeof(big) - 1] = '\0';
    TEST_ASSERT_TRUE(espnow_inbox_push(&ib, MAC_A, 1, false, big));
    const espnow_msg_entry_t *e = espnow_inbox_get(&ib, 0);
    TEST_ASSERT_EQUAL_UINT(ESPNOW_MSG_TEXT_MAX, strlen(e->text));
}

void test_inbox_duplicate_detection(void)
{
    espnow_inbox_t ib;
    espnow_inbox_init(&ib);
    espnow_inbox_push(&ib, MAC_A, 5, false, "hi");
    TEST_ASSERT_TRUE(espnow_inbox_is_duplicate(&ib, MAC_A, 5));
    TEST_ASSERT_FALSE(espnow_inbox_is_duplicate(&ib, MAC_A, 6));
    TEST_ASSERT_FALSE(espnow_inbox_is_duplicate(&ib, MAC_B, 5));
}

void test_inbox_duplicate_ignores_outgoing(void)
{
    espnow_inbox_t ib;
    espnow_inbox_init(&ib);
    espnow_inbox_push(&ib, MAC_A, 5, false, "in");
    espnow_inbox_push(&ib, MAC_A, 9, true,  "out");   /* our own reply, seq 9 */
    /* Duplicate check must ignore the outgoing entry and still see seq 5. */
    TEST_ASSERT_TRUE(espnow_inbox_is_duplicate(&ib, MAC_A, 5));
    TEST_ASSERT_FALSE(espnow_inbox_is_duplicate(&ib, MAC_A, 9));
}

/* =========================================================================
 * Runner
 * =========================================================================*/

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_build_parse_roundtrip);
    RUN_TEST(test_build_rejects_empty);
    RUN_TEST(test_build_rejects_too_long);
    RUN_TEST(test_build_rejects_small_buffer);
    RUN_TEST(test_build_max_length_ok);
    RUN_TEST(test_parse_rejects_wrong_type);
    RUN_TEST(test_parse_rejects_header_only);
    RUN_TEST(test_parse_rejects_embedded_nul);
    RUN_TEST(test_parse_rejects_small_out);
    RUN_TEST(test_inbox_push_and_get);
    RUN_TEST(test_inbox_evicts_oldest_when_full);
    RUN_TEST(test_inbox_truncates_long_text);
    RUN_TEST(test_inbox_duplicate_detection);
    RUN_TEST(test_inbox_duplicate_ignores_outgoing);
    return UNITY_END();
}
