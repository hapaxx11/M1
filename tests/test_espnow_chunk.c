/* See COPYING.txt for license details. */

/**
 * @file   test_espnow_chunk.c
 * @brief  Host-side unit tests for ESP-NOW fragmentation/reassembly, the app
 *         type registry, and the capability-availability decision.
 */

#include "unity.h"
#include "espnow_chunk.h"
#include "espnow_appmsg.h"
#include "espnow_caps_decide.h"

#include <string.h>

void setUp(void)  {}
void tearDown(void) {}

/* =========================================================================
 * Helpers
 * =========================================================================*/

static void fill_pattern(uint8_t *buf, size_t len)
{
    for (size_t i = 0; i < len; ++i)
        buf[i] = (uint8_t)(i * 7u + 3u);
}

/* Split a message through the splitter, feed frames to a reassembler, and
 * assert the reassembled bytes match the source exactly. */
static void roundtrip(const uint8_t *src, size_t len, uint8_t msg_id)
{
    espnow_chunk_splitter_t s;
    TEST_ASSERT_TRUE(espnow_chunk_split_init(&s, msg_id, src, len));

    espnow_chunk_reasm_t r;
    espnow_chunk_reasm_init(&r);

    uint8_t frame[ESPNOW_CHUNK_FRAME_MAX];
    size_t  flen = 0;
    espnow_chunk_status_t st = ESPNOW_CHUNK_NEED_MORE;
    uint8_t emitted = 0;

    while (espnow_chunk_split_next(&s, frame, sizeof(frame), &flen)) {
        TEST_ASSERT_LESS_OR_EQUAL_UINT(ESPNOW_CHUNK_FRAME_MAX, flen);
        TEST_ASSERT_EQUAL_UINT8(ESPNOW_APP_FRAG, frame[0]);
        st = espnow_chunk_reasm_feed(&r, frame, flen);
        emitted++;
    }

    TEST_ASSERT_EQUAL_UINT8(espnow_chunk_frag_count(len), emitted);
    TEST_ASSERT_EQUAL_INT(ESPNOW_CHUNK_COMPLETE, st);
    TEST_ASSERT_EQUAL_UINT16(len, r.msg_len);
    TEST_ASSERT_EQUAL_MEMORY(src, r.msg, len);
}

/* =========================================================================
 * frag_count
 * =========================================================================*/

void test_frag_count_zero_is_invalid(void)
{
    TEST_ASSERT_EQUAL_UINT8(0, espnow_chunk_frag_count(0));
}

void test_frag_count_single_frame(void)
{
    TEST_ASSERT_EQUAL_UINT8(1, espnow_chunk_frag_count(1));
    TEST_ASSERT_EQUAL_UINT8(1, espnow_chunk_frag_count(ESPNOW_CHUNK_DATA_MAX));
}

void test_frag_count_boundary(void)
{
    /* One byte over a full fragment needs two fragments. */
    TEST_ASSERT_EQUAL_UINT8(2, espnow_chunk_frag_count(ESPNOW_CHUNK_DATA_MAX + 1));
}

void test_frag_count_max_message(void)
{
    TEST_ASSERT_EQUAL_UINT8(ESPNOW_CHUNK_MAX_FRAGS,
                            espnow_chunk_frag_count(ESPNOW_CHUNK_MSG_MAX));
}

void test_frag_count_over_max_is_invalid(void)
{
    TEST_ASSERT_EQUAL_UINT8(0, espnow_chunk_frag_count(ESPNOW_CHUNK_MSG_MAX + 1));
}

/* =========================================================================
 * Round-trip
 * =========================================================================*/

void test_roundtrip_single_byte(void)
{
    uint8_t src[1] = { 0xAB };
    roundtrip(src, 1, 0x01);
}

void test_roundtrip_exact_one_fragment(void)
{
    uint8_t src[ESPNOW_CHUNK_DATA_MAX];
    fill_pattern(src, sizeof(src));
    roundtrip(src, sizeof(src), 0x02);
}

void test_roundtrip_two_fragments(void)
{
    uint8_t src[ESPNOW_CHUNK_DATA_MAX + 1];
    fill_pattern(src, sizeof(src));
    roundtrip(src, sizeof(src), 0x03);
}

void test_roundtrip_max_message(void)
{
    uint8_t src[ESPNOW_CHUNK_MSG_MAX];
    fill_pattern(src, sizeof(src));
    roundtrip(src, sizeof(src), 0xFF);
}

void test_roundtrip_all_lengths(void)
{
    uint8_t src[ESPNOW_CHUNK_MSG_MAX];
    fill_pattern(src, sizeof(src));
    for (size_t len = 1; len <= ESPNOW_CHUNK_MSG_MAX; ++len)
        roundtrip(src, len, (uint8_t)len);
}

/* =========================================================================
 * Reassembler edge cases
 * =========================================================================*/

void test_reasm_ignores_non_fragment(void)
{
    espnow_chunk_reasm_t r;
    espnow_chunk_reasm_init(&r);
    uint8_t frame[4] = { ESPNOW_APP_MSG_BASE, 0, 0, 0 };
    TEST_ASSERT_EQUAL_INT(ESPNOW_CHUNK_IGNORED,
                          espnow_chunk_reasm_feed(&r, frame, sizeof(frame)));
}

void test_reasm_out_of_order(void)
{
    /* Three fragments delivered in reverse order still reassemble. */
    uint8_t src[ESPNOW_CHUNK_DATA_MAX * 2 + 5];
    fill_pattern(src, sizeof(src));

    espnow_chunk_splitter_t s;
    TEST_ASSERT_TRUE(espnow_chunk_split_init(&s, 0x44, src, sizeof(src)));

    uint8_t f[3][ESPNOW_CHUNK_FRAME_MAX];
    size_t  fl[3];
    for (int i = 0; i < 3; ++i)
        TEST_ASSERT_TRUE(espnow_chunk_split_next(&s, f[i], ESPNOW_CHUNK_FRAME_MAX, &fl[i]));

    espnow_chunk_reasm_t r;
    espnow_chunk_reasm_init(&r);
    TEST_ASSERT_EQUAL_INT(ESPNOW_CHUNK_NEED_MORE, espnow_chunk_reasm_feed(&r, f[2], fl[2]));
    TEST_ASSERT_EQUAL_INT(ESPNOW_CHUNK_NEED_MORE, espnow_chunk_reasm_feed(&r, f[0], fl[0]));
    TEST_ASSERT_EQUAL_INT(ESPNOW_CHUNK_COMPLETE, espnow_chunk_reasm_feed(&r, f[1], fl[1]));
    TEST_ASSERT_EQUAL_UINT16(sizeof(src), r.msg_len);
    TEST_ASSERT_EQUAL_MEMORY(src, r.msg, sizeof(src));
}

void test_reasm_duplicate_fragment_tolerated(void)
{
    uint8_t src[ESPNOW_CHUNK_DATA_MAX + 3];
    fill_pattern(src, sizeof(src));

    espnow_chunk_splitter_t s;
    espnow_chunk_split_init(&s, 0x55, src, sizeof(src));
    uint8_t f0[ESPNOW_CHUNK_FRAME_MAX], f1[ESPNOW_CHUNK_FRAME_MAX];
    size_t l0, l1;
    espnow_chunk_split_next(&s, f0, sizeof(f0), &l0);
    espnow_chunk_split_next(&s, f1, sizeof(f1), &l1);

    espnow_chunk_reasm_t r;
    espnow_chunk_reasm_init(&r);
    TEST_ASSERT_EQUAL_INT(ESPNOW_CHUNK_NEED_MORE, espnow_chunk_reasm_feed(&r, f0, l0));
    /* Duplicate of fragment 0 must not complete or corrupt. */
    TEST_ASSERT_EQUAL_INT(ESPNOW_CHUNK_NEED_MORE, espnow_chunk_reasm_feed(&r, f0, l0));
    TEST_ASSERT_EQUAL_INT(ESPNOW_CHUNK_COMPLETE, espnow_chunk_reasm_feed(&r, f1, l1));
    TEST_ASSERT_EQUAL_MEMORY(src, r.msg, sizeof(src));
}

void test_reasm_rejects_bad_index(void)
{
    espnow_chunk_reasm_t r;
    espnow_chunk_reasm_init(&r);
    /* idx (2) >= cnt (2) is out of range. */
    uint8_t frame[ESPNOW_CHUNK_HDR_LEN + 1] = { ESPNOW_APP_FRAG, 1, 2, 2, 0x99 };
    TEST_ASSERT_EQUAL_INT(ESPNOW_CHUNK_ERROR,
                          espnow_chunk_reasm_feed(&r, frame, sizeof(frame)));
}

void test_reasm_rejects_short_nonfinal_fragment(void)
{
    espnow_chunk_reasm_t r;
    espnow_chunk_reasm_init(&r);
    /* Fragment 0 of 2 must be full-size; a short one is malformed. */
    uint8_t frame[ESPNOW_CHUNK_HDR_LEN + 1] = { ESPNOW_APP_FRAG, 1, 0, 2, 0x99 };
    TEST_ASSERT_EQUAL_INT(ESPNOW_CHUNK_ERROR,
                          espnow_chunk_reasm_feed(&r, frame, sizeof(frame)));
}

void test_reasm_new_msgid_restarts(void)
{
    uint8_t src[ESPNOW_CHUNK_DATA_MAX + 2];
    fill_pattern(src, sizeof(src));

    espnow_chunk_splitter_t s;
    espnow_chunk_split_init(&s, 0x10, src, sizeof(src));
    uint8_t f0[ESPNOW_CHUNK_FRAME_MAX];
    size_t l0;
    espnow_chunk_split_next(&s, f0, sizeof(f0), &l0);   /* msg 0x10 frag 0 */

    espnow_chunk_reasm_t r;
    espnow_chunk_reasm_init(&r);
    TEST_ASSERT_EQUAL_INT(ESPNOW_CHUNK_NEED_MORE, espnow_chunk_reasm_feed(&r, f0, l0));

    /* A different, single-fragment message arrives before the first finished. */
    uint8_t other[3] = { 1, 2, 3 };
    espnow_chunk_splitter_t s2;
    espnow_chunk_split_init(&s2, 0x11, other, sizeof(other));
    uint8_t g0[ESPNOW_CHUNK_FRAME_MAX];
    size_t  gl0;
    espnow_chunk_split_next(&s2, g0, sizeof(g0), &gl0);
    TEST_ASSERT_EQUAL_INT(ESPNOW_CHUNK_COMPLETE, espnow_chunk_reasm_feed(&r, g0, gl0));
    TEST_ASSERT_EQUAL_UINT16(sizeof(other), r.msg_len);
    TEST_ASSERT_EQUAL_MEMORY(other, r.msg, sizeof(other));
}

/* =========================================================================
 * App type registry
 * =========================================================================*/

void test_appmsg_classify_blocks(void)
{
    TEST_ASSERT_EQUAL_INT(ESPNOW_APP_CLASS_PAIR,          espnow_app_classify(0x01));
    TEST_ASSERT_EQUAL_INT(ESPNOW_APP_CLASS_PAIR,          espnow_app_classify(0x0F));
    TEST_ASSERT_EQUAL_INT(ESPNOW_APP_CLASS_FILE_TRANSFER, espnow_app_classify(0x10));
    TEST_ASSERT_EQUAL_INT(ESPNOW_APP_CLASS_FILE_TRANSFER, espnow_app_classify(0x16));
    TEST_ASSERT_EQUAL_INT(ESPNOW_APP_CLASS_MESSAGE,       espnow_app_classify(0x20));
    TEST_ASSERT_EQUAL_INT(ESPNOW_APP_CLASS_TRIGGER,       espnow_app_classify(0x30));
    TEST_ASSERT_EQUAL_INT(ESPNOW_APP_CLASS_GAME,          espnow_app_classify(0x40));
    TEST_ASSERT_EQUAL_INT(ESPNOW_APP_CLASS_CRYPTO,        espnow_app_classify(0xE0));
}

void test_appmsg_classify_unknown(void)
{
    TEST_ASSERT_EQUAL_INT(ESPNOW_APP_CLASS_UNKNOWN, espnow_app_classify(0x00));
    TEST_ASSERT_EQUAL_INT(ESPNOW_APP_CLASS_UNKNOWN, espnow_app_classify(0x60));
    TEST_ASSERT_EQUAL_INT(ESPNOW_APP_CLASS_UNKNOWN, espnow_app_classify(0xFF));
}

void test_appmsg_frag_is_own_block(void)
{
    /* The fragmentation type sits in its own (currently unclassified) block so
     * it never masquerades as a real app message. */
    TEST_ASSERT_EQUAL_INT(ESPNOW_APP_CLASS_UNKNOWN, espnow_app_classify(ESPNOW_APP_FRAG));
}

/* =========================================================================
 * Capability decision
 * =========================================================================*/

void test_caps_self_report_always_available(void)
{
    TEST_ASSERT_TRUE(espnow_caps_available(true, ESPNOW_DECIDE_TRANSPORT_AT, false));
    TEST_ASSERT_TRUE(espnow_caps_available(true, ESPNOW_DECIDE_TRANSPORT_NONE, false));
}

void test_caps_rpc_probe_fallback(void)
{
    TEST_ASSERT_TRUE(espnow_caps_available(false, ESPNOW_DECIDE_TRANSPORT_RPC, true));
    TEST_ASSERT_FALSE(espnow_caps_available(false, ESPNOW_DECIDE_TRANSPORT_RPC, false));
}

void test_caps_non_rpc_fails_closed(void)
{
    TEST_ASSERT_FALSE(espnow_caps_available(false, ESPNOW_DECIDE_TRANSPORT_AT, true));
    TEST_ASSERT_FALSE(espnow_caps_available(false, ESPNOW_DECIDE_TRANSPORT_BINARY, true));
    TEST_ASSERT_FALSE(espnow_caps_available(false, ESPNOW_DECIDE_TRANSPORT_NONE, true));
}

/* =========================================================================
 * Runner
 * =========================================================================*/

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_frag_count_zero_is_invalid);
    RUN_TEST(test_frag_count_single_frame);
    RUN_TEST(test_frag_count_boundary);
    RUN_TEST(test_frag_count_max_message);
    RUN_TEST(test_frag_count_over_max_is_invalid);
    RUN_TEST(test_roundtrip_single_byte);
    RUN_TEST(test_roundtrip_exact_one_fragment);
    RUN_TEST(test_roundtrip_two_fragments);
    RUN_TEST(test_roundtrip_max_message);
    RUN_TEST(test_roundtrip_all_lengths);
    RUN_TEST(test_reasm_ignores_non_fragment);
    RUN_TEST(test_reasm_out_of_order);
    RUN_TEST(test_reasm_duplicate_fragment_tolerated);
    RUN_TEST(test_reasm_rejects_bad_index);
    RUN_TEST(test_reasm_rejects_short_nonfinal_fragment);
    RUN_TEST(test_reasm_new_msgid_restarts);
    RUN_TEST(test_appmsg_classify_blocks);
    RUN_TEST(test_appmsg_classify_unknown);
    RUN_TEST(test_appmsg_frag_is_own_block);
    RUN_TEST(test_caps_self_report_always_available);
    RUN_TEST(test_caps_rpc_probe_fallback);
    RUN_TEST(test_caps_non_rpc_fails_closed);
    return UNITY_END();
}
