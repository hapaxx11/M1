/* See COPYING.txt for license details. */

/**
 * @file   test_espnow_file_transfer.c
 * @brief  Host-side unit tests for the ESP-NOW file transfer protocol.
 *
 * Uses mock HAL ops to verify the state machine, CRC accumulation,
 * and stop-and-wait ARQ flow.
 */

#include "unity.h"
#include "espnow_file_transfer.h"
#include "m1_espnow_hal.h"

#include <string.h>

void setUp(void)  {}
void tearDown(void) {}

/* =========================================================================
 * Mock HAL
 * =========================================================================*/

#define MOCK_MAX_SENDS  64
#define MOCK_MAX_MSG    256

typedef struct {
    uint8_t  sends[MOCK_MAX_SENDS][MOCK_MAX_MSG];
    size_t   send_lens[MOCK_MAX_SENDS];
    int      send_count;
    bool     send_fail;

    uint8_t  file_data[4096];
    size_t   file_written;
    bool     file_opened;
    bool     file_closed;
    bool     file_write_fail;

    uint32_t time_ms;
} mock_ctx_t;

static mock_ctx_t s_mock;

static bool mock_send(const uint8_t mac[ESPNOW_FT_MAC_LEN],
                      const uint8_t *data, size_t len, void *ctx)
{
    (void)mac;
    mock_ctx_t *m = (mock_ctx_t *)ctx;
    if (m->send_fail)
        return false;
    if (m->send_count < MOCK_MAX_SENDS && len <= MOCK_MAX_MSG) {
        memcpy(m->sends[m->send_count], data, len);
        m->send_lens[m->send_count] = len;
        m->send_count++;
    }
    return true;
}

static espnow_ft_file_t mock_file_open(const char *path, void *ctx)
{
    (void)path;
    mock_ctx_t *m = (mock_ctx_t *)ctx;
    m->file_opened = true;
    m->file_written = 0;
    return (espnow_ft_file_t)1; /* non-NULL = success */
}

static bool mock_file_write(espnow_ft_file_t f, const uint8_t *data,
                            size_t len, void *ctx)
{
    (void)f;
    mock_ctx_t *m = (mock_ctx_t *)ctx;
    if (m->file_write_fail)
        return false;
    if (m->file_written + len <= sizeof(m->file_data)) {
        memcpy(m->file_data + m->file_written, data, len);
        m->file_written += len;
    }
    return true;
}

static void mock_file_close(espnow_ft_file_t f, void *ctx)
{
    (void)f;
    mock_ctx_t *m = (mock_ctx_t *)ctx;
    m->file_closed = true;
}

static uint32_t mock_millis(void *ctx)
{
    mock_ctx_t *m = (mock_ctx_t *)ctx;
    return m->time_ms;
}

static const espnow_ft_hal_ops_t s_mock_hal = {
    .send       = mock_send,
    .file_open  = mock_file_open,
    .file_write = mock_file_write,
    .file_close = mock_file_close,
    .millis     = mock_millis,
    .ctx        = &s_mock,
};

static void reset_mock(void)
{
    memset(&s_mock, 0, sizeof(s_mock));
}

/* =========================================================================
 * CRC32 tests
 * =========================================================================*/

void test_crc32_empty(void)
{
    uint32_t crc = espnow_ft_crc32(0, NULL, 0);
    TEST_ASSERT_EQUAL_UINT32(0, crc);
}

void test_crc32_known_value(void)
{
    const uint8_t data[] = "123456789";
    uint32_t crc = espnow_ft_crc32(0, data, 9);
    /* Standard CRC32 of "123456789" = 0xCBF43926 */
    TEST_ASSERT_EQUAL_HEX32(0xCBF43926, crc);
}

void test_crc32_incremental(void)
{
    const uint8_t data[] = "123456789";
    uint32_t crc1 = espnow_ft_crc32(0, data, 4);
    uint32_t crc2 = espnow_ft_crc32(crc1, data + 4, 5);
    TEST_ASSERT_EQUAL_HEX32(0xCBF43926, crc2);
}

/* =========================================================================
 * Sender tests
 * =========================================================================*/

static const uint8_t PEER_MAC[ESPNOW_FT_MAC_LEN] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};

void test_send_init(void)
{
    reset_mock();
    espnow_ft_ctx_t ctx;
    espnow_ft_send_init(&ctx, &s_mock_hal, PEER_MAC, "test.sub",
                         1024, 0xDEADBEEF, 100);
    TEST_ASSERT_EQUAL(ESPNOW_FT_STATE_IDLE, ctx.state);
    TEST_ASSERT_EQUAL_STRING("test.sub", ctx.filename);
    TEST_ASSERT_EQUAL_UINT32(1024, ctx.file_size);
    TEST_ASSERT_EQUAL_HEX32(0xDEADBEEF, ctx.expected_crc32);
    TEST_ASSERT_EQUAL_UINT8(100, ctx.chunk_size);
}

void test_send_offer_transitions_to_offer_sent(void)
{
    reset_mock();
    espnow_ft_ctx_t ctx;
    espnow_ft_send_init(&ctx, &s_mock_hal, PEER_MAC, "test.sub",
                         1024, 0xDEADBEEF, 100);
    TEST_ASSERT_TRUE(espnow_ft_send_offer(&ctx));
    TEST_ASSERT_EQUAL(ESPNOW_FT_STATE_OFFER_SENT, ctx.state);
    TEST_ASSERT_EQUAL_INT(1, s_mock.send_count);
    /* First byte of sent message should be OFFER type */
    TEST_ASSERT_EQUAL_UINT8(ESPNOW_FT_MSG_OFFER, s_mock.sends[0][0]);
}

void test_sender_frames_fit_direct_transport_budget(void)
{
    reset_mock();
    espnow_ft_ctx_t ctx;
    espnow_ft_send_init(&ctx, &s_mock_hal, PEER_MAC, "capture.sub",
                         72, 0x12345678, 36);
    TEST_ASSERT_TRUE(espnow_ft_send_offer(&ctx));
    TEST_ASSERT_LESS_OR_EQUAL_UINT(M1_ESPNOW_SEND_PAYLOAD_MAX,
                                   s_mock.send_lens[0]);

    TEST_ASSERT_TRUE(espnow_ft_send_on_recv(&ctx, ESPNOW_FT_MSG_ACCEPT, NULL, 0));
    uint8_t data[36];
    memset(data, 0xA5, sizeof(data));
    TEST_ASSERT_TRUE(espnow_ft_send_chunk(&ctx, data, sizeof(data)));
    TEST_ASSERT_LESS_OR_EQUAL_UINT(M1_ESPNOW_SEND_PAYLOAD_MAX,
                                   s_mock.send_lens[1]);
}

void test_send_accept_transitions_to_sending(void)
{
    reset_mock();
    espnow_ft_ctx_t ctx;
    espnow_ft_send_init(&ctx, &s_mock_hal, PEER_MAC, "test.sub",
                         1024, 0xDEADBEEF, 100);
    espnow_ft_send_offer(&ctx);
    TEST_ASSERT_TRUE(espnow_ft_send_on_recv(&ctx, ESPNOW_FT_MSG_ACCEPT, NULL, 0));
    TEST_ASSERT_EQUAL(ESPNOW_FT_STATE_SENDING, ctx.state);
}

void test_send_reject_transitions_to_failed(void)
{
    reset_mock();
    espnow_ft_ctx_t ctx;
    espnow_ft_send_init(&ctx, &s_mock_hal, PEER_MAC, "test.sub",
                         1024, 0xDEADBEEF, 100);
    espnow_ft_send_offer(&ctx);
    TEST_ASSERT_TRUE(espnow_ft_send_on_recv(&ctx, ESPNOW_FT_MSG_REJECT, NULL, 0));
    TEST_ASSERT_EQUAL(ESPNOW_FT_STATE_FAILED, ctx.state);
}

void test_send_chunk_transitions_to_wait_ack(void)
{
    reset_mock();
    espnow_ft_ctx_t ctx;
    espnow_ft_send_init(&ctx, &s_mock_hal, PEER_MAC, "test.sub",
                         200, 0x12345678, 100);
    espnow_ft_send_offer(&ctx);
    espnow_ft_send_on_recv(&ctx, ESPNOW_FT_MSG_ACCEPT, NULL, 0);

    uint8_t data[100];
    memset(data, 0xAA, sizeof(data));
    TEST_ASSERT_TRUE(espnow_ft_send_chunk(&ctx, data, 100));
    TEST_ASSERT_EQUAL(ESPNOW_FT_STATE_WAIT_ACK, ctx.state);
}

void test_send_ack_after_all_data_sends_complete(void)
{
    reset_mock();
    espnow_ft_ctx_t ctx;
    espnow_ft_send_init(&ctx, &s_mock_hal, PEER_MAC, "test.sub",
                         50, 0x12345678, 100);
    espnow_ft_send_offer(&ctx);
    espnow_ft_send_on_recv(&ctx, ESPNOW_FT_MSG_ACCEPT, NULL, 0);

    uint8_t data[50];
    memset(data, 0xBB, sizeof(data));
    espnow_ft_send_chunk(&ctx, data, 50);

    /* Receive ACK — should complete since all bytes are sent */
    TEST_ASSERT_TRUE(espnow_ft_send_on_recv(&ctx, ESPNOW_FT_MSG_ACK, NULL, 0));
    TEST_ASSERT_EQUAL(ESPNOW_FT_STATE_DONE, ctx.state);
}

void test_send_timeout_retries(void)
{
    reset_mock();
    espnow_ft_ctx_t ctx;
    espnow_ft_send_init(&ctx, &s_mock_hal, PEER_MAC, "test.sub",
                         200, 0x12345678, 100);
    espnow_ft_send_offer(&ctx);
    espnow_ft_send_on_recv(&ctx, ESPNOW_FT_MSG_ACCEPT, NULL, 0);

    uint8_t data[100];
    memset(data, 0xCC, sizeof(data));
    espnow_ft_send_chunk(&ctx, data, 100);
    TEST_ASSERT_EQUAL(ESPNOW_FT_STATE_WAIT_ACK, ctx.state);

    /* Advance time past timeout */
    s_mock.time_ms = 600;
    TEST_ASSERT_TRUE(espnow_ft_send_check_timeout(&ctx));
    TEST_ASSERT_EQUAL(ESPNOW_FT_STATE_SENDING, ctx.state);
}

void test_send_timeout_max_retries_fails(void)
{
    reset_mock();
    espnow_ft_ctx_t ctx;
    espnow_ft_send_init(&ctx, &s_mock_hal, PEER_MAC, "test.sub",
                         200, 0x12345678, 100);
    espnow_ft_send_offer(&ctx);
    espnow_ft_send_on_recv(&ctx, ESPNOW_FT_MSG_ACCEPT, NULL, 0);

    uint8_t data[100];
    memset(data, 0xDD, sizeof(data));

    /* Exhaust retries */
    for (int i = 0; i <= ESPNOW_FT_MAX_RETRIES; i++) {
        if (ctx.state == ESPNOW_FT_STATE_SENDING)
            espnow_ft_send_chunk(&ctx, data, 100);
        s_mock.time_ms += 600;
        espnow_ft_send_check_timeout(&ctx);
    }
    TEST_ASSERT_EQUAL(ESPNOW_FT_STATE_FAILED, ctx.state);
}

/* =========================================================================
 * Receiver tests
 * =========================================================================*/

void test_recv_init(void)
{
    reset_mock();
    espnow_ft_ctx_t ctx;
    espnow_ft_recv_init(&ctx, &s_mock_hal);
    TEST_ASSERT_EQUAL(ESPNOW_FT_STATE_IDLE, ctx.state);
}

void test_recv_offer_parses_metadata(void)
{
    reset_mock();
    espnow_ft_ctx_t ctx;
    espnow_ft_recv_init(&ctx, &s_mock_hal);

    /* Build offer payload: filename + size(4) + crc(4) + chunk_size(1) */
    uint8_t payload[ESPNOW_FT_FILENAME_MAX + 4 + 4 + 1];
    size_t off = ESPNOW_FT_FILENAME_MAX;
    memset(payload, 0, sizeof(payload));
    memcpy(payload, "hello.sub", 9);
    /* size = 256 (LE) */
    payload[off + 0] = 0x00; payload[off + 1] = 0x01;
    payload[off + 2] = 0x00; payload[off + 3] = 0x00;
    off += 4;
    /* crc = 0xAABBCCDD (LE) */
    payload[off + 0] = 0xDD; payload[off + 1] = 0xCC;
    payload[off + 2] = 0xBB; payload[off + 3] = 0xAA;
    off += 4;
    /* chunk_size = 100 */
    payload[off] = 100;

    TEST_ASSERT_TRUE(espnow_ft_recv_on_msg(&ctx, PEER_MAC,
        ESPNOW_FT_MSG_OFFER, 0, payload, sizeof(payload)));
    TEST_ASSERT_EQUAL(ESPNOW_FT_STATE_OFFER_RECEIVED, ctx.state);
    TEST_ASSERT_EQUAL_STRING("hello.sub", ctx.filename);
    TEST_ASSERT_EQUAL_UINT32(256, ctx.file_size);
    TEST_ASSERT_EQUAL_HEX32(0xAABBCCDD, ctx.expected_crc32);
    TEST_ASSERT_EQUAL_UINT8(100, ctx.chunk_size);
}

void test_recv_accept_opens_file_and_sends_accept(void)
{
    reset_mock();
    espnow_ft_ctx_t ctx;
    espnow_ft_recv_init(&ctx, &s_mock_hal);

    /* Simulate offer */
    uint8_t payload[ESPNOW_FT_FILENAME_MAX + 4 + 4 + 1];
    size_t off = ESPNOW_FT_FILENAME_MAX;
    memset(payload, 0, sizeof(payload));
    memcpy(payload, "test.sub", 8);
    payload[off] = 10; /* size = 10 */
    payload[off + 8] = 10; /* chunk_size = 10 */
    espnow_ft_recv_on_msg(&ctx, PEER_MAC, ESPNOW_FT_MSG_OFFER, 0,
                           payload, sizeof(payload));

    TEST_ASSERT_TRUE(espnow_ft_recv_accept(&ctx, "/sd/test.sub"));
    TEST_ASSERT_EQUAL(ESPNOW_FT_STATE_RECEIVING, ctx.state);
    TEST_ASSERT_TRUE(s_mock.file_opened);
    /* Check that ACCEPT was sent */
    TEST_ASSERT_EQUAL_INT(1, s_mock.send_count);
    TEST_ASSERT_EQUAL_UINT8(ESPNOW_FT_MSG_ACCEPT, s_mock.sends[0][0]);
}

void test_recv_reject_sends_reject_and_goes_idle(void)
{
    reset_mock();
    espnow_ft_ctx_t ctx;
    espnow_ft_recv_init(&ctx, &s_mock_hal);

    uint8_t payload[ESPNOW_FT_FILENAME_MAX + 4 + 4 + 1];
    memset(payload, 0, sizeof(payload));
    payload[ESPNOW_FT_FILENAME_MAX + 8] = 100;
    espnow_ft_recv_on_msg(&ctx, PEER_MAC, ESPNOW_FT_MSG_OFFER, 0,
                           payload, sizeof(payload));

    TEST_ASSERT_TRUE(espnow_ft_recv_reject(&ctx));
    TEST_ASSERT_EQUAL(ESPNOW_FT_STATE_IDLE, ctx.state);
    TEST_ASSERT_EQUAL_UINT8(ESPNOW_FT_MSG_REJECT, s_mock.sends[0][0]);
}

void test_recv_data_writes_to_file_and_acks(void)
{
    reset_mock();
    espnow_ft_ctx_t ctx;
    espnow_ft_recv_init(&ctx, &s_mock_hal);

    /* Setup: offer → accept */
    uint8_t payload[ESPNOW_FT_FILENAME_MAX + 4 + 4 + 1];
    size_t off = ESPNOW_FT_FILENAME_MAX;
    memset(payload, 0, sizeof(payload));
    payload[off] = 10; /* size = 10 */
    payload[off + 8] = 10; /* chunk_size = 10 */
    espnow_ft_recv_on_msg(&ctx, PEER_MAC, ESPNOW_FT_MSG_OFFER, 0,
                           payload, sizeof(payload));
    espnow_ft_recv_accept(&ctx, "/sd/test.sub");
    s_mock.send_count = 0; /* reset after accept */

    /* Send data chunk: offset(4LE) + data */
    uint8_t data_msg[4 + 10];
    memset(data_msg, 0, 4);  /* offset = 0 */
    memset(data_msg + 4, 0x42, 10);

    TEST_ASSERT_TRUE(espnow_ft_recv_on_msg(&ctx, PEER_MAC,
        ESPNOW_FT_MSG_DATA, 0, data_msg, sizeof(data_msg)));

    /* Verify file was written */
    TEST_ASSERT_EQUAL_UINT(10, s_mock.file_written);
    TEST_ASSERT_EQUAL_UINT8(0x42, s_mock.file_data[0]);

    /* Verify ACK was sent */
    TEST_ASSERT_EQUAL_INT(1, s_mock.send_count);
    TEST_ASSERT_EQUAL_UINT8(ESPNOW_FT_MSG_ACK, s_mock.sends[0][0]);
}

void test_recv_complete_success(void)
{
    reset_mock();
    espnow_ft_ctx_t ctx;
    espnow_ft_recv_init(&ctx, &s_mock_hal);

    /* Compute CRC of test data */
    uint8_t file_data[10];
    memset(file_data, 0x42, 10);
    uint32_t expected_crc = espnow_ft_crc32(0, file_data, 10);

    /* Offer with matching size and CRC */
    uint8_t payload[ESPNOW_FT_FILENAME_MAX + 4 + 4 + 1];
    size_t off = ESPNOW_FT_FILENAME_MAX;
    memset(payload, 0, sizeof(payload));
    payload[off] = 10; /* size = 10 (LE) */
    off += 4;
    payload[off + 0] = (uint8_t)(expected_crc >>  0);
    payload[off + 1] = (uint8_t)(expected_crc >>  8);
    payload[off + 2] = (uint8_t)(expected_crc >> 16);
    payload[off + 3] = (uint8_t)(expected_crc >> 24);
    off += 4;
    payload[off] = 10;
    espnow_ft_recv_on_msg(&ctx, PEER_MAC, ESPNOW_FT_MSG_OFFER, 0,
                           payload, sizeof(payload));
    espnow_ft_recv_accept(&ctx, "/sd/out.bin");

    /* Send one data chunk */
    uint8_t data_msg[4 + 10];
    memset(data_msg, 0, 4);
    memset(data_msg + 4, 0x42, 10);
    espnow_ft_recv_on_msg(&ctx, PEER_MAC, ESPNOW_FT_MSG_DATA, 0,
                           data_msg, sizeof(data_msg));

    /* Send COMPLETE */
    TEST_ASSERT_TRUE(espnow_ft_recv_on_msg(&ctx, PEER_MAC,
        ESPNOW_FT_MSG_COMPLETE, 1, NULL, 0));
    TEST_ASSERT_EQUAL(ESPNOW_FT_STATE_DONE, ctx.state);
    TEST_ASSERT_TRUE(s_mock.file_closed);
}

void test_recv_complete_crc_mismatch_fails(void)
{
    reset_mock();
    espnow_ft_ctx_t ctx;
    espnow_ft_recv_init(&ctx, &s_mock_hal);

    /* Offer with wrong CRC */
    uint8_t payload[ESPNOW_FT_FILENAME_MAX + 4 + 4 + 1];
    size_t off = ESPNOW_FT_FILENAME_MAX;
    memset(payload, 0, sizeof(payload));
    payload[off] = 10; /* size = 10 */
    payload[off + 4] = 0xFF; /* bogus CRC */
    payload[off + 8] = 10;
    espnow_ft_recv_on_msg(&ctx, PEER_MAC, ESPNOW_FT_MSG_OFFER, 0,
                           payload, sizeof(payload));
    espnow_ft_recv_accept(&ctx, "/sd/out.bin");

    /* Send data */
    uint8_t data_msg[4 + 10];
    memset(data_msg, 0, 4);
    memset(data_msg + 4, 0x42, 10);
    espnow_ft_recv_on_msg(&ctx, PEER_MAC, ESPNOW_FT_MSG_DATA, 0,
                           data_msg, sizeof(data_msg));

    /* COMPLETE — CRC won't match */
    espnow_ft_recv_on_msg(&ctx, PEER_MAC, ESPNOW_FT_MSG_COMPLETE, 1, NULL, 0);
    TEST_ASSERT_EQUAL(ESPNOW_FT_STATE_FAILED, ctx.state);
}

/* =========================================================================
 * Test runner
 * =========================================================================*/

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_crc32_empty);
    RUN_TEST(test_crc32_known_value);
    RUN_TEST(test_crc32_incremental);

    RUN_TEST(test_send_init);
    RUN_TEST(test_send_offer_transitions_to_offer_sent);
    RUN_TEST(test_send_accept_transitions_to_sending);
    RUN_TEST(test_send_reject_transitions_to_failed);
    RUN_TEST(test_send_chunk_transitions_to_wait_ack);
    RUN_TEST(test_send_ack_after_all_data_sends_complete);
    RUN_TEST(test_send_timeout_retries);
    RUN_TEST(test_send_timeout_max_retries_fails);

    RUN_TEST(test_recv_init);
    RUN_TEST(test_recv_offer_parses_metadata);
    RUN_TEST(test_recv_accept_opens_file_and_sends_accept);
    RUN_TEST(test_recv_reject_sends_reject_and_goes_idle);
    RUN_TEST(test_recv_data_writes_to_file_and_acks);
    RUN_TEST(test_recv_complete_success);
    RUN_TEST(test_recv_complete_crc_mismatch_fails);

    return UNITY_END();
}
