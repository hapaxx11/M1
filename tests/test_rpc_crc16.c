/* See COPYING.txt for license details. */

/*
 * test_rpc_crc16.c
 *
 * Unit tests for the RPC CRC-16 implementation and frame format.
 * Tests the table-driven CRC used by the M1 RPC binary protocol.
 *
 * The M1 RPC CRC-16 uses a custom lookup table (poly=0x1021,
 * init=0xFFFF, no reflect, no final XOR).  The production table in
 * m1_rpc.c differs from the canonical CRC-16/CCITT-FALSE table in
 * 46 entries, so the check value for "123456789" is 0xC9B1 — NOT
 * the standard CCITT-FALSE result of 0x29B1.  Both the table and
 * expected values here are copied verbatim from the production code;
 * do NOT "correct" them to match online CRC calculators.
 *
 * Build with the host-side CMake:
 *   cmake -B build-tests -S tests && cmake --build build-tests
 *   ctest --test-dir build-tests --output-on-failure
 */

#include "unity.h"
#include "rpc_crc16_impl.h"
#include <string.h>
#include <stdio.h>

/* RPC protocol constants — must match m1_rpc.h */
#define RPC_SYNC_BYTE   0xAA
#define RPC_HEADER_SIZE 5       /* SYNC + CMD + SEQ + LEN(2) */
#define RPC_CRC_SIZE    2

/* Unity setup / teardown (required stubs) */
void setUp(void) {}
void tearDown(void) {}

/* ===================================================================
 * CRC-16 known vectors (M1 custom table)
 * =================================================================== */

void test_crc16_known_vector_123456789(void)
{
    /* M1 RPC CRC-16 over "123456789" = 0xC9B1.
     * The M1 lookup table differs from the canonical CRC-16/CCITT-FALSE
     * table (which yields 0x29B1), so this value is intentionally
     * different.  Verified against the production table in m1_rpc.c. */
    uint8_t data[] = "123456789";
    uint16_t crc = rpc_crc16(data, 9);
    TEST_ASSERT_EQUAL_HEX16(0xC9B1, crc);
}

void test_crc16_empty(void)
{
    /* CRC of empty data should be the init value: 0xFFFF */
    uint16_t crc = rpc_crc16(NULL, 0);
    TEST_ASSERT_EQUAL_HEX16(0xFFFF, crc);
}

void test_crc16_single_byte_zero(void)
{
    uint8_t data[] = {0x00};
    uint16_t crc = rpc_crc16(data, 1);
    /* CRC-16/CCITT-FALSE of single 0x00 byte = 0xE1F0 */
    TEST_ASSERT_EQUAL_HEX16(0xE1F0, crc);
}

void test_crc16_single_byte_ff(void)
{
    uint8_t data[] = {0xFF};
    uint16_t crc = rpc_crc16(data, 1);
    TEST_ASSERT_EQUAL_HEX16(0xFF00, crc);
}

void test_crc16_two_bytes(void)
{
    uint8_t data[] = {0xAA, 0x55};
    uint16_t crc = rpc_crc16(data, 2);
    TEST_ASSERT_EQUAL_HEX16(0x05EA, crc);
}

/* ===================================================================
 * Incremental CRC (rpc_crc16_continue)
 * =================================================================== */

void test_crc16_continue_matches_one_shot(void)
{
    /* Computing CRC over "123456789" in two parts should match one-shot */
    uint8_t data[] = "123456789";
    uint16_t one_shot = rpc_crc16(data, 9);

    /* Split at position 5: "12345" + "6789" */
    uint16_t partial = rpc_crc16(data, 5);
    uint16_t full = rpc_crc16_continue(partial, data + 5, 4);

    TEST_ASSERT_EQUAL_HEX16(one_shot, full);
}

void test_crc16_continue_byte_at_a_time(void)
{
    /* Feed byte-by-byte using continue, should match one-shot */
    uint8_t data[] = "HELLO";
    uint16_t one_shot = rpc_crc16(data, 5);

    uint16_t crc = 0xFFFF; /* CRC16_INIT */
    for (int i = 0; i < 5; i++)
    {
        crc = rpc_crc16_continue(crc, &data[i], 1);
    }

    TEST_ASSERT_EQUAL_HEX16(one_shot, crc);
}

void test_crc16_continue_empty_is_identity(void)
{
    /* Continue with 0 bytes should return input CRC unchanged */
    uint16_t crc = 0x1234;
    uint16_t result = rpc_crc16_continue(crc, NULL, 0);
    TEST_ASSERT_EQUAL_HEX16(0x1234, result);
}

/* ===================================================================
 * Frame CRC computation (simulates m1_rpc_send_frame layout)
 * =================================================================== */

/**
 * Build a complete RPC frame in `out` and return its total size.
 * This mirrors the frame builder in m1_rpc.c::m1_rpc_send_frame().
 */
static uint16_t build_rpc_frame(uint8_t cmd, uint8_t seq,
                                const uint8_t *payload, uint16_t payload_len,
                                uint8_t *out, uint16_t out_size)
{
    if (payload == NULL && payload_len > 0)
        return 0;

    uint16_t frame_size = RPC_HEADER_SIZE + payload_len + RPC_CRC_SIZE;
    if (frame_size > out_size)
        return 0;

    uint16_t idx = 0;
    out[idx++] = RPC_SYNC_BYTE;
    out[idx++] = cmd;
    out[idx++] = seq;
    out[idx++] = (uint8_t)(payload_len & 0xFF);
    out[idx++] = (uint8_t)(payload_len >> 8);

    if (payload != NULL && payload_len > 0)
    {
        memcpy(&out[idx], payload, payload_len);
        idx += payload_len;
    }

    /* CRC over CMD + SEQ + LEN + PAYLOAD (bytes 1..idx-1) */
    uint16_t crc = rpc_crc16(&out[1], idx - 1);
    out[idx++] = (uint8_t)(crc & 0xFF);
    out[idx++] = (uint8_t)(crc >> 8);

    return frame_size;
}

/**
 * Validate that a frame's CRC matches its content.
 * Returns 1 if valid, 0 if invalid.
 */
static int validate_rpc_frame(const uint8_t *frame, uint16_t frame_len)
{
    if (frame_len < RPC_HEADER_SIZE + RPC_CRC_SIZE)
        return 0;
    if (frame[0] != RPC_SYNC_BYTE)
        return 0;

    uint16_t payload_len = (uint16_t)frame[3] | ((uint16_t)frame[4] << 8);
    uint16_t expected_len = RPC_HEADER_SIZE + payload_len + RPC_CRC_SIZE;
    if (frame_len != expected_len)
        return 0;

    /* CRC is over header fields (CMD+SEQ+LEN) + payload */
    uint16_t crc_data_len = 4 + payload_len; /* CMD(1)+SEQ(1)+LEN(2)+PAYLOAD */
    uint16_t computed_crc = rpc_crc16(&frame[1], crc_data_len);

    uint16_t rx_crc = (uint16_t)frame[frame_len - 2] |
                      ((uint16_t)frame[frame_len - 1] << 8);

    return (computed_crc == rx_crc) ? 1 : 0;
}

void test_frame_ping_no_payload(void)
{
    /* PING frame: cmd=0x01, seq=0, no payload */
    uint8_t frame[64];
    uint16_t len = build_rpc_frame(0x01, 0x00, NULL, 0, frame, sizeof(frame));

    TEST_ASSERT_EQUAL_UINT16(RPC_HEADER_SIZE + RPC_CRC_SIZE, len);
    TEST_ASSERT_EQUAL_HEX8(RPC_SYNC_BYTE, frame[0]);
    TEST_ASSERT_EQUAL_HEX8(0x01, frame[1]); /* cmd */
    TEST_ASSERT_EQUAL_HEX8(0x00, frame[2]); /* seq */
    TEST_ASSERT_EQUAL_HEX8(0x00, frame[3]); /* len lo */
    TEST_ASSERT_EQUAL_HEX8(0x00, frame[4]); /* len hi */
    TEST_ASSERT_TRUE(validate_rpc_frame(frame, len));
}

void test_frame_button_click(void)
{
    /* BUTTON_CLICK frame: cmd=0x22, seq=1, payload={button_id} */
    uint8_t payload[] = {0x00}; /* RPC_BUTTON_OK */
    uint8_t frame[64];
    uint16_t len = build_rpc_frame(0x22, 0x01, payload, 1, frame, sizeof(frame));

    TEST_ASSERT_EQUAL_UINT16(RPC_HEADER_SIZE + 1 + RPC_CRC_SIZE, len);
    TEST_ASSERT_EQUAL_HEX8(0x22, frame[1]);
    TEST_ASSERT_EQUAL_HEX8(0x01, frame[2]);
    TEST_ASSERT_EQUAL_HEX8(0x01, frame[3]); /* payload len = 1 */
    TEST_ASSERT_EQUAL_HEX8(0x00, frame[4]); /* payload len hi */
    TEST_ASSERT_EQUAL_HEX8(0x00, frame[5]); /* payload: button OK */
    TEST_ASSERT_TRUE(validate_rpc_frame(frame, len));
}

void test_frame_with_payload(void)
{
    /* File read frame with a path payload */
    uint8_t payload[] = "0:/SUBGHZ/test.sub";
    uint16_t plen = (uint16_t)strlen((char *)payload);
    uint8_t frame[128];
    uint16_t len = build_rpc_frame(0x32, 0x42, payload, plen, frame, sizeof(frame));

    TEST_ASSERT_TRUE(validate_rpc_frame(frame, len));

    /* Verify payload content in frame */
    TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, &frame[5], plen);
}

void test_frame_crc_corruption_detected(void)
{
    /* Build a valid frame, then corrupt one byte — CRC should fail */
    uint8_t payload[] = {0xDE, 0xAD, 0xBE, 0xEF};
    uint8_t frame[64];
    uint16_t len = build_rpc_frame(0x03, 0x10, payload, 4, frame, sizeof(frame));

    /* Valid first */
    TEST_ASSERT_TRUE(validate_rpc_frame(frame, len));

    /* Corrupt a payload byte */
    frame[6] ^= 0x01;
    TEST_ASSERT_FALSE(validate_rpc_frame(frame, len));
}

void test_frame_crc_incremental_matches(void)
{
    /* Verify that the incremental CRC used by the parser matches
     * the one-shot CRC used by the frame builder. */
    uint8_t header[] = {0x03, 0x10, 0x04, 0x00}; /* CMD=0x03 SEQ=0x10 LEN=4 */
    uint8_t payload[] = {0xDE, 0xAD, 0xBE, 0xEF};

    /* One-shot over header+payload */
    uint8_t combined[8];
    memcpy(combined, header, 4);
    memcpy(combined + 4, payload, 4);
    uint16_t one_shot = rpc_crc16(combined, 8);

    /* Incremental: header then payload (matches parser in m1_rpc.c) */
    uint16_t incremental = rpc_crc16(header, 4);
    incremental = rpc_crc16_continue(incremental, payload, 4);

    TEST_ASSERT_EQUAL_HEX16(one_shot, incremental);
}

void test_frame_max_seq_number(void)
{
    /* Sequence number wraps around — 0xFF should work fine */
    uint8_t frame[64];
    uint16_t len = build_rpc_frame(0x01, 0xFF, NULL, 0, frame, sizeof(frame));
    TEST_ASSERT_EQUAL_HEX8(0xFF, frame[2]);
    TEST_ASSERT_TRUE(validate_rpc_frame(frame, len));
}

void test_frame_all_commands_have_valid_crc(void)
{
    /* Verify CRC validity for all RPC command IDs used in production */
    uint8_t cmds[] = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, /* System */
        0x10, 0x11, 0x12, 0x13,                           /* Screen */
        0x20, 0x21, 0x22,                                 /* Input */
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, /* File */
        0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,   /* FW */
        0x50, 0x51, 0x52, 0x53, 0x54,                     /* ESP32 */
        0x60, 0x61, 0x62, 0x63                             /* CLI/Debug */
    };

    for (unsigned i = 0; i < sizeof(cmds); i++)
    {
        uint8_t frame[64];
        uint16_t len = build_rpc_frame(cmds[i], (uint8_t)i, NULL, 0,
                                       frame, sizeof(frame));
        char msg[64];
        snprintf(msg, sizeof(msg), "CRC failed for cmd=0x%02X", cmds[i]);
        TEST_ASSERT_TRUE_MESSAGE(validate_rpc_frame(frame, len), msg);
    }
}

/* ===================================================================
 * Runner
 * =================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* CRC known vectors */
    RUN_TEST(test_crc16_known_vector_123456789);
    RUN_TEST(test_crc16_empty);
    RUN_TEST(test_crc16_single_byte_zero);
    RUN_TEST(test_crc16_single_byte_ff);
    RUN_TEST(test_crc16_two_bytes);

    /* Incremental CRC */
    RUN_TEST(test_crc16_continue_matches_one_shot);
    RUN_TEST(test_crc16_continue_byte_at_a_time);
    RUN_TEST(test_crc16_continue_empty_is_identity);

    /* Frame format */
    RUN_TEST(test_frame_ping_no_payload);
    RUN_TEST(test_frame_button_click);
    RUN_TEST(test_frame_with_payload);
    RUN_TEST(test_frame_crc_corruption_detected);
    RUN_TEST(test_frame_crc_incremental_matches);
    RUN_TEST(test_frame_max_seq_number);
    RUN_TEST(test_frame_all_commands_have_valid_crc);

    return UNITY_END();
}
