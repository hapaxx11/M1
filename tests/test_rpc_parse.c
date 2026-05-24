/* See COPYING.txt for license details. */

/*
 * test_rpc_parse.c
 *
 * Unit tests for the RPC frame parser state machine.
 *
 * Tests cover:
 *   - Basic frame parsing (PING, with payload)
 *   - Frame builder round-trip
 *   - CRC mismatch rejection
 *   - Invalid length rejection
 *   - Zero-length payload
 *   - Garbage before sync byte
 *   - Multiple consecutive frames
 *   - Incremental CRC (split header + payload)
 *   - Maximum payload edge case
 *   - Parser reset after bad frame
 */

#include "unity.h"
#include "rpc_parse_impl.h"
#include <string.h>

static RPC_ParseCtx ctx;

void setUp(void)
{
    rpc_parse_init(&ctx);
}

void tearDown(void)
{
}

/* ── Helper: feed an entire buffer to the parser ── */
static void feed_bytes(const uint8_t *data, uint16_t len)
{
    for (uint16_t i = 0; i < len; i++)
    {
        rpc_parse_byte(&ctx, data[i]);
    }
}

/* ── 1. PING frame (no payload) ── */
void test_parse_ping_no_payload(void)
{
    uint8_t buf[16];
    uint16_t len = rpc_build_frame(buf, 0x01, 0x00, NULL, 0);

    TEST_ASSERT_EQUAL_UINT16(7, len);  /* SYNC + 4 header + 0 payload + 2 CRC */
    feed_bytes(buf, len);

    TEST_ASSERT_TRUE(ctx.frame_ready);
    TEST_ASSERT_EQUAL_UINT8(0x01, ctx.frame.cmd);
    TEST_ASSERT_EQUAL_UINT8(0x00, ctx.frame.seq);
    TEST_ASSERT_EQUAL_UINT16(0, ctx.frame.len);
}

/* ── 2. Frame with 4-byte payload ── */
void test_parse_with_payload(void)
{
    uint8_t payload[] = { 0xDE, 0xAD, 0xBE, 0xEF };
    uint8_t buf[32];
    uint16_t len = rpc_build_frame(buf, 0x03, 0x42, payload, 4);

    TEST_ASSERT_EQUAL_UINT16(11, len);  /* 1 + 4 + 4 + 2 */
    feed_bytes(buf, len);

    TEST_ASSERT_TRUE(ctx.frame_ready);
    TEST_ASSERT_EQUAL_UINT8(0x03, ctx.frame.cmd);
    TEST_ASSERT_EQUAL_UINT8(0x42, ctx.frame.seq);
    TEST_ASSERT_EQUAL_UINT16(4, ctx.frame.len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, ctx.frame.payload, 4);
}

/* ── 3. CRC mismatch → frame_ready stays false ── */
void test_crc_mismatch_rejected(void)
{
    uint8_t buf[16];
    uint16_t len = rpc_build_frame(buf, 0x01, 0x00, NULL, 0);

    /* Corrupt the CRC */
    buf[len - 1] ^= 0xFF;

    feed_bytes(buf, len);

    TEST_ASSERT_FALSE(ctx.frame_ready);
    TEST_ASSERT_EQUAL(RPC_STATE_IDLE, ctx.state);
}

/* ── 4. Invalid length (> RPC_MAX_PAYLOAD) → reset to idle ── */
void test_invalid_length_rejected(void)
{
    uint8_t buf[] = {
        RPC_SYNC_BYTE,
        0x03,       /* CMD */
        0x01,       /* SEQ */
        0x01, 0x40  /* LEN = 0x4001 = 16385 > 8192 */
    };
    feed_bytes(buf, sizeof(buf));

    TEST_ASSERT_FALSE(ctx.frame_ready);
    TEST_ASSERT_EQUAL(RPC_STATE_IDLE, ctx.state);
}

/* ── 5. Garbage before sync byte is ignored ── */
void test_garbage_before_sync(void)
{
    uint8_t garbage[] = { 0x00, 0xFF, 0x12, 0x34, 0x56 };
    feed_bytes(garbage, sizeof(garbage));

    TEST_ASSERT_EQUAL(RPC_STATE_IDLE, ctx.state);
    TEST_ASSERT_FALSE(ctx.frame_ready);

    /* Now feed a valid frame */
    uint8_t buf[16];
    uint16_t len = rpc_build_frame(buf, 0x02, 0x01, NULL, 0);
    feed_bytes(buf, len);

    TEST_ASSERT_TRUE(ctx.frame_ready);
    TEST_ASSERT_EQUAL_UINT8(0x02, ctx.frame.cmd);
}

/* ── 6. Two consecutive frames ── */
void test_two_consecutive_frames(void)
{
    uint8_t buf1[16], buf2[16];
    uint16_t len1 = rpc_build_frame(buf1, 0x01, 0x00, NULL, 0);
    uint8_t payload2[] = { 0x11, 0x22 };
    uint16_t len2 = rpc_build_frame(buf2, 0x05, 0x03, payload2, 2);

    /* Feed first frame */
    feed_bytes(buf1, len1);
    TEST_ASSERT_TRUE(ctx.frame_ready);
    TEST_ASSERT_EQUAL_UINT8(0x01, ctx.frame.cmd);

    /* Reset frame_ready and feed second */
    ctx.frame_ready = false;
    feed_bytes(buf2, len2);
    TEST_ASSERT_TRUE(ctx.frame_ready);
    TEST_ASSERT_EQUAL_UINT8(0x05, ctx.frame.cmd);
    TEST_ASSERT_EQUAL_UINT8(0x03, ctx.frame.seq);
    TEST_ASSERT_EQUAL_UINT16(2, ctx.frame.len);
}

/* ── 7. Parser state is IDLE after init ── */
void test_initial_state_idle(void)
{
    TEST_ASSERT_EQUAL(RPC_STATE_IDLE, ctx.state);
    TEST_ASSERT_FALSE(ctx.frame_ready);
}

/* ── 8. Parser recovers after bad CRC ── */
void test_recovery_after_bad_crc(void)
{
    /* Send a frame with bad CRC */
    uint8_t bad[16];
    uint16_t bad_len = rpc_build_frame(bad, 0x01, 0x00, NULL, 0);
    bad[bad_len - 1] ^= 0x01;
    feed_bytes(bad, bad_len);
    TEST_ASSERT_FALSE(ctx.frame_ready);

    /* Send a valid frame — should parse correctly */
    uint8_t good[16];
    uint16_t good_len = rpc_build_frame(good, 0x03, 0x07, NULL, 0);
    feed_bytes(good, good_len);
    TEST_ASSERT_TRUE(ctx.frame_ready);
    TEST_ASSERT_EQUAL_UINT8(0x03, ctx.frame.cmd);
    TEST_ASSERT_EQUAL_UINT8(0x07, ctx.frame.seq);
}

/* ── 9. Sequence number round-trip ── */
void test_sequence_number_preserved(void)
{
    uint8_t buf[16];
    uint16_t len = rpc_build_frame(buf, 0x01, 0xFF, NULL, 0);
    feed_bytes(buf, len);

    TEST_ASSERT_TRUE(ctx.frame_ready);
    TEST_ASSERT_EQUAL_UINT8(0xFF, ctx.frame.seq);
}

/* ── 10. Large payload (256 bytes) ── */
void test_large_payload(void)
{
    uint8_t payload[256];
    for (int i = 0; i < 256; i++)
        payload[i] = (uint8_t)i;

    uint8_t buf[300];
    uint16_t len = rpc_build_frame(buf, 0x10, 0x01, payload, 256);

    TEST_ASSERT_EQUAL_UINT16(263, len);  /* 1 + 4 + 256 + 2 */
    feed_bytes(buf, len);

    TEST_ASSERT_TRUE(ctx.frame_ready);
    TEST_ASSERT_EQUAL_UINT8(0x10, ctx.frame.cmd);
    TEST_ASSERT_EQUAL_UINT16(256, ctx.frame.len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, ctx.frame.payload, 256);
}

/* ── 11. Build frame round-trip verification ── */
void test_build_frame_format(void)
{
    uint8_t payload[] = { 0xAB };
    uint8_t buf[16];
    uint16_t len = rpc_build_frame(buf, 0x22, 0x10, payload, 1);

    /* Verify wire format:
     * [0xAA] [0x22] [0x10] [0x01 0x00] [0xAB] [CRC_LO CRC_HI]
     */
    TEST_ASSERT_EQUAL_UINT8(RPC_SYNC_BYTE, buf[0]);
    TEST_ASSERT_EQUAL_UINT8(0x22, buf[1]);  /* CMD */
    TEST_ASSERT_EQUAL_UINT8(0x10, buf[2]);  /* SEQ */
    TEST_ASSERT_EQUAL_UINT8(0x01, buf[3]);  /* LEN lo */
    TEST_ASSERT_EQUAL_UINT8(0x00, buf[4]);  /* LEN hi */
    TEST_ASSERT_EQUAL_UINT8(0xAB, buf[5]);  /* Payload */
    TEST_ASSERT_EQUAL_UINT16(8, len);       /* 1 + 4 + 1 + 2 */
}

/* ── 12. Sync byte in middle of garbage is detected ── */
void test_sync_in_middle_of_stream(void)
{
    /* Garbage, then sync, then garbage (should go to HEADER but frame incomplete) */
    uint8_t data[] = { 0x00, 0x11, RPC_SYNC_BYTE, 0x01 };
    feed_bytes(data, sizeof(data));

    /* Should be in HEADER state, waiting for more bytes */
    TEST_ASSERT_EQUAL(RPC_STATE_HEADER, ctx.state);
    TEST_ASSERT_FALSE(ctx.frame_ready);
}

/* ── 13. Double sync byte — second sync restarts ── */
void test_double_sync_restarts(void)
{
    /* Start a frame, then hit another sync byte that restarts */
    uint8_t data[] = { RPC_SYNC_BYTE, 0x01, RPC_SYNC_BYTE };
    feed_bytes(data, sizeof(data));

    /* The second 0xAA doesn't match since we're in HEADER state
     * (it's treated as header byte, not sync). The actual behavior
     * depends on the state machine — in HEADER state, 0xAA is just
     * a header byte value, not a re-sync. */
    TEST_ASSERT_EQUAL(RPC_STATE_HEADER, ctx.state);
}

/* ── 14. All command byte values work ── */
void test_all_cmd_values(void)
{
    for (uint16_t cmd = 0; cmd < 256; cmd++)
    {
        rpc_parse_init(&ctx);
        uint8_t buf[16];
        uint16_t len = rpc_build_frame(buf, (uint8_t)cmd, 0x00, NULL, 0);
        feed_bytes(buf, len);
        TEST_ASSERT_TRUE_MESSAGE(ctx.frame_ready, "Frame should parse for all CMD values");
        TEST_ASSERT_EQUAL_UINT8((uint8_t)cmd, ctx.frame.cmd);
    }
}

/* ── 15. Incremental CRC matches full CRC ── */
void test_incremental_crc_matches_full(void)
{
    uint8_t data[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 };

    /* Full CRC over all 6 bytes */
    uint16_t full = rpc_crc16(data, 6);

    /* Incremental: first 4 bytes, then next 2 */
    uint16_t inc = rpc_crc16(data, 4);
    inc = rpc_crc16_continue(inc, &data[4], 2);

    TEST_ASSERT_EQUAL_UINT16(full, inc);
}

/* ── 16. Payload length at boundary (exactly MAX) ── */
void test_payload_at_max_boundary(void)
{
    /* Just verify the length check: MAX_PAYLOAD should be accepted,
     * MAX_PAYLOAD+1 should be rejected.
     * We can't actually allocate 8192 bytes of frame buffer in a test,
     * so just test the header parsing acceptance/rejection. */

    /* Length = MAX_PAYLOAD (0x2000) — accepted */
    uint8_t accept[] = {
        RPC_SYNC_BYTE,
        0x01,           /* CMD */
        0x00,           /* SEQ */
        0x00, 0x20      /* LEN = 0x2000 = 8192 = MAX */
    };
    feed_bytes(accept, sizeof(accept));
    TEST_ASSERT_EQUAL(RPC_STATE_PAYLOAD, ctx.state);

    /* Reset and test MAX+1 — rejected */
    rpc_parse_init(&ctx);
    uint8_t reject[] = {
        RPC_SYNC_BYTE,
        0x01,           /* CMD */
        0x00,           /* SEQ */
        0x01, 0x20      /* LEN = 0x2001 = 8193 > MAX */
    };
    feed_bytes(reject, sizeof(reject));
    TEST_ASSERT_EQUAL(RPC_STATE_IDLE, ctx.state);
}

/* ── 17. Frame ready flag cleared on new sync ── */
void test_frame_ready_cleared_on_new_sync(void)
{
    /* Parse a valid frame */
    uint8_t buf[16];
    uint16_t len = rpc_build_frame(buf, 0x01, 0x00, NULL, 0);
    feed_bytes(buf, len);
    TEST_ASSERT_TRUE(ctx.frame_ready);

    /* Feed a new sync byte — frame_ready should be cleared */
    rpc_parse_byte(&ctx, RPC_SYNC_BYTE);
    TEST_ASSERT_FALSE(ctx.frame_ready);
    TEST_ASSERT_EQUAL(RPC_STATE_HEADER, ctx.state);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_initial_state_idle);
    RUN_TEST(test_parse_ping_no_payload);
    RUN_TEST(test_parse_with_payload);
    RUN_TEST(test_crc_mismatch_rejected);
    RUN_TEST(test_invalid_length_rejected);
    RUN_TEST(test_garbage_before_sync);
    RUN_TEST(test_two_consecutive_frames);
    RUN_TEST(test_recovery_after_bad_crc);
    RUN_TEST(test_sequence_number_preserved);
    RUN_TEST(test_large_payload);
    RUN_TEST(test_build_frame_format);
    RUN_TEST(test_sync_in_middle_of_stream);
    RUN_TEST(test_double_sync_restarts);
    RUN_TEST(test_all_cmd_values);
    RUN_TEST(test_incremental_crc_matches_full);
    RUN_TEST(test_payload_at_max_boundary);
    RUN_TEST(test_frame_ready_cleared_on_new_sync);
    return UNITY_END();
}
