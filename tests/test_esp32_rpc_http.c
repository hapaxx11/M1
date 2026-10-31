/* See COPYING.txt for license details. */

#include "unity.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "m1_esp32_rpc.h"
#include "m1_esp32_rpc_http.h"

#define TEST_PATH "/tmp/m1_rpc_http_test.bin"
#define MAX_STEPS 12
#define MAX_PAYLOAD 1024

typedef struct {
    uint16_t msg_id;
    uint8_t nak;
    uint16_t payload_len;
    uint8_t payload[MAX_PAYLOAD];
} rpc_step_t;

static rpc_step_t s_steps[MAX_STEPS];
static int s_step_count;
static int s_step_index;
static char s_open_url[1025];
static uint8_t s_accept;
static uint32_t s_timeout_ms;
static uint16_t s_read_wants[MAX_STEPS];
static int s_read_count;
static int s_close_count;

uint64_t m1_esp32_caps_get_bitmap(void) { return 0u; }
bool m1_esp32_caps_is_queried(void) { return true; }
void m1_esp32_caps_init(void) {}
uint8_t m1_esp32_get_init_status(void) { return 1u; }
void m1_wdt_reset(void) {}

uint8_t spi_AT_send_recv_bin(const uint8_t *tx_buf, int tx_len,
                             uint8_t *rx_buf, int rx_buf_size,
                             int *out_len, int timeout_sec)
{
    (void)tx_buf; (void)tx_len; (void)rx_buf; (void)rx_buf_size;
    (void)out_len; (void)timeout_sec;
    return 1u;
}

uint8_t spi_m1link_send_recv_bin_timed(const uint8_t *tx_buf, int tx_len,
                                       uint8_t *rx_buf, int rx_buf_size,
                                       int *out_len, uint32_t timeout_ms)
{
    (void)tx_buf; (void)tx_len; (void)rx_buf; (void)rx_buf_size;
    (void)out_len; (void)timeout_ms;
    return 1u;
}

uint32_t m1_esp32_m1link_last_elapsed_ms(void) { return 0u; }

static uint16_t rd16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8u);
}

static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8u) |
           ((uint32_t)p[2] << 16u) | ((uint32_t)p[3] << 24u);
}

static uint8_t fake_transport(const uint8_t *tx_buf, int tx_len,
                              uint8_t *rx_buf, int rx_buf_size,
                              int *out_len, uint32_t timeout_ms)
{
    (void)timeout_ms;
    TEST_ASSERT_NOT_NULL(tx_buf);
    TEST_ASSERT_NOT_NULL(rx_buf);
    TEST_ASSERT_NOT_NULL(out_len);
    TEST_ASSERT_TRUE(s_step_index < s_step_count);
    TEST_ASSERT_GREATER_OR_EQUAL_INT(M1_ESP32_RPC_HDR_SIZE, tx_len);

    rpc_step_t *step = &s_steps[s_step_index++];
    TEST_ASSERT_EQUAL_UINT16(step->msg_id, rd16(&tx_buf[4]));
    uint16_t req_len = rd16(&tx_buf[6]);
    const uint8_t *req = &tx_buf[M1_ESP32_RPC_HDR_SIZE];

    if (step->msg_id == M1_ESP32_RPC_NET_HTTP_OPEN) {
        uint16_t url_len = rd16(&req[5]);
        TEST_ASSERT_EQUAL_UINT16(7u + url_len, req_len);
        TEST_ASSERT_TRUE(url_len < sizeof(s_open_url));
        memcpy(s_open_url, &req[7], url_len);
        s_open_url[url_len] = '\0';
        s_accept = req[0];
        s_timeout_ms = rd32(&req[1]);
    } else if (step->msg_id == M1_ESP32_RPC_NET_HTTP_READ) {
        TEST_ASSERT_EQUAL_UINT16(2u, req_len);
        TEST_ASSERT_TRUE(s_read_count < MAX_STEPS);
        s_read_wants[s_read_count++] = rd16(req);
    } else if (step->msg_id == M1_ESP32_RPC_NET_HTTP_CLOSE) {
        s_close_count++;
    }

    const uint8_t *payload = step->payload;
    uint16_t payload_len = step->payload_len;
    uint8_t nak_payload[1];
    if (step->nak) {
        nak_payload[0] = step->nak;
        payload = nak_payload;
        payload_len = 1u;
    }

    uint16_t frame_len = m1_esp32_rpc_build_req(
        rx_buf, (uint16_t)rx_buf_size, step->msg_id, payload, payload_len);
    TEST_ASSERT_GREATER_THAN_UINT16(0u, frame_len);
    rx_buf[3] = step->nak ? M1_ESP32_RPC_NAK : M1_ESP32_RPC_RESP;
    uint16_t crc = m1_esp32_rpc_crc16(rx_buf, M1_ESP32_RPC_HDR_SIZE + payload_len);
    rx_buf[M1_ESP32_RPC_HDR_SIZE + payload_len] = (uint8_t)crc;
    rx_buf[M1_ESP32_RPC_HDR_SIZE + payload_len + 1u] = (uint8_t)(crc >> 8u);
    *out_len = frame_len;
    return 0u;
}

static void queue_step(uint16_t msg_id, const void *payload, uint16_t payload_len)
{
    TEST_ASSERT_TRUE(s_step_count < MAX_STEPS);
    rpc_step_t *step = &s_steps[s_step_count++];
    memset(step, 0, sizeof(*step));
    step->msg_id = msg_id;
    step->payload_len = payload_len;
    if (payload_len > 0u)
        memcpy(step->payload, payload, payload_len);
}

static void queue_nak(uint16_t msg_id, m1_esp32_rpc_status_t status)
{
    queue_step(msg_id, NULL, 0u);
    s_steps[s_step_count - 1].nak = (uint8_t)status;
}

static void queue_open(m1_esp32_rpc_http_result_t result,
                       uint16_t http_status, int32_t content_length)
{
    uint8_t resp[7] = {0};
    resp[0] = (uint8_t)result;
    resp[1] = (uint8_t)http_status;
    resp[2] = (uint8_t)(http_status >> 8u);
    uint32_t length = (uint32_t)content_length;
    for (int i = 0; i < 4; i++)
        resp[3 + i] = (uint8_t)(length >> (i * 8));
    queue_step(M1_ESP32_RPC_NET_HTTP_OPEN, resp, sizeof(resp));
}

static void queue_close(void)
{
    queue_step(M1_ESP32_RPC_NET_HTTP_CLOSE, NULL, 0u);
}

void setUp(void)
{
    memset(s_steps, 0, sizeof(s_steps));
    s_step_count = 0;
    s_step_index = 0;
    s_open_url[0] = '\0';
    s_accept = 0xFFu;
    s_timeout_ms = 0u;
    memset(s_read_wants, 0, sizeof(s_read_wants));
    s_read_count = 0;
    s_close_count = 0;
    m1_esp32_rpc_set_transport(fake_transport);
}

void tearDown(void)
{
    TEST_ASSERT_EQUAL_INT(s_step_count, s_step_index);
    m1_esp32_rpc_set_transport(NULL);
    remove(TEST_PATH);
}

static void test_get_streams_release_json(void)
{
    queue_open(M1_ESP32_RPC_HTTP_RESULT_OK, 200u, 5);
    queue_step(M1_ESP32_RPC_NET_HTTP_READ, "he", 2u);
    queue_step(M1_ESP32_RPC_NET_HTTP_READ, "llo", 3u);
    queue_close();

    char response[16];
    TEST_ASSERT_EQUAL(HTTP_OK,
        m1_esp32_rpc_http_get("https://api.github.com/repos/hapaxx11/M1/releases?per_page=5",
                              response, sizeof(response), 30u));
    TEST_ASSERT_EQUAL_STRING("hello", response);
    TEST_ASSERT_EQUAL_STRING("https://api.github.com/repos/hapaxx11/M1/releases?per_page=5",
                             s_open_url);
    TEST_ASSERT_EQUAL_UINT8(M1_ESP32_RPC_HTTP_ACCEPT_JSON, s_accept);
    TEST_ASSERT_EQUAL_UINT32(30000u, s_timeout_ms);
    TEST_ASSERT_EQUAL_INT(2, s_read_count);
    TEST_ASSERT_EQUAL_UINT16(5u, s_read_wants[0]);
    TEST_ASSERT_EQUAL_UINT16(3u, s_read_wants[1]);
    TEST_ASSERT_EQUAL_INT(1, s_close_count);
}

static void test_get_rejects_truncated_json(void)
{
    queue_open(M1_ESP32_RPC_HTTP_RESULT_OK, 200u, 10);
    queue_step(M1_ESP32_RPC_NET_HTTP_READ, "12345", 5u);
    queue_close();

    char response[6];
    TEST_ASSERT_EQUAL(HTTP_ERR_RESPONSE_TOO_LARGE,
        m1_esp32_rpc_http_get("https://api.github.com/test", response,
                              sizeof(response), 30u));
    TEST_ASSERT_EQUAL_STRING("12345", response);
    TEST_ASSERT_EQUAL_INT(1, s_close_count);
}

static void test_download_streams_binary_to_sd(void)
{
    queue_open(M1_ESP32_RPC_HTTP_RESULT_OK, 200u, 6);
    queue_step(M1_ESP32_RPC_NET_HTTP_READ, "abc", 3u);
    queue_step(M1_ESP32_RPC_NET_HTTP_READ, "def", 3u);
    queue_close();

    TEST_ASSERT_EQUAL(HTTP_OK,
        m1_esp32_rpc_http_download_to_file("https://github.com/fw.bin", TEST_PATH,
                                           NULL, 60u));
    TEST_ASSERT_EQUAL_UINT8(M1_ESP32_RPC_HTTP_ACCEPT_BINARY, s_accept);
    TEST_ASSERT_EQUAL_UINT32(60000u, s_timeout_ms);

    FILE *f = fopen(TEST_PATH, "rb");
    TEST_ASSERT_NOT_NULL(f);
    char data[7] = {0};
    TEST_ASSERT_EQUAL_size_t(6u, fread(data, 1u, 6u, f));
    fclose(f);
    TEST_ASSERT_EQUAL_STRING("abcdef", data);
    TEST_ASSERT_EQUAL_INT(1, s_close_count);
}

static void test_open_dns_error_is_specific(void)
{
    queue_open(M1_ESP32_RPC_HTTP_RESULT_DNS, 0u, -1);
    char response[16];
    TEST_ASSERT_EQUAL(HTTP_ERR_DNS_FAIL,
        m1_esp32_rpc_http_get("https://api.github.com/test", response,
                              sizeof(response), 30u));
    TEST_ASSERT_EQUAL_INT(0, s_close_count);
}

static void test_old_brain_is_unsupported_not_dns(void)
{
    queue_nak(M1_ESP32_RPC_NET_HTTP_OPEN, M1_ESP32_RPC_ERR_UNSUPPORTED);
    char response[16];
    TEST_ASSERT_EQUAL(HTTP_ERR_UNSUPPORTED_TRANSPORT,
        m1_esp32_rpc_http_get("https://api.github.com/test", response,
                              sizeof(response), 30u));
    TEST_ASSERT_EQUAL_INT(0, s_close_count);
}

static void test_http_error_closes_stream(void)
{
    queue_open(M1_ESP32_RPC_HTTP_RESULT_OK, 404u, 0);
    queue_close();
    char response[16];
    TEST_ASSERT_EQUAL(HTTP_ERR_HTTP_ERROR,
        m1_esp32_rpc_http_get("https://api.github.com/missing", response,
                              sizeof(response), 30u));
    TEST_ASSERT_EQUAL_INT(1, s_close_count);
}

static void test_read_timeout_closes_and_deletes_partial_file(void)
{
    queue_open(M1_ESP32_RPC_HTTP_RESULT_OK, 200u, 6);
    queue_step(M1_ESP32_RPC_NET_HTTP_READ, "abc", 3u);
    queue_nak(M1_ESP32_RPC_NET_HTTP_READ, M1_ESP32_RPC_ERR_TIMEOUT);
    queue_close();

    TEST_ASSERT_EQUAL(HTTP_ERR_TIMEOUT,
        m1_esp32_rpc_http_download_to_file("https://github.com/fw.bin", TEST_PATH,
                                           NULL, 60u));
    TEST_ASSERT_EQUAL_INT(1, s_close_count);
    TEST_ASSERT_NULL(fopen(TEST_PATH, "rb"));
}

static bool cancel_progress(uint32_t downloaded, uint32_t total)
{
    (void)total;
    return downloaded == 0u;
}

static void test_cancel_closes_and_deletes_partial_file(void)
{
    queue_open(M1_ESP32_RPC_HTTP_RESULT_OK, 200u, 6);
    queue_step(M1_ESP32_RPC_NET_HTTP_READ, "abc", 3u);
    queue_close();

    TEST_ASSERT_EQUAL(HTTP_ERR_CANCELLED,
        m1_esp32_rpc_http_download_to_file("https://github.com/fw.bin", TEST_PATH,
                                           cancel_progress, 60u));
    TEST_ASSERT_EQUAL_INT(1, s_close_count);
    TEST_ASSERT_NULL(fopen(TEST_PATH, "rb"));
}

static void test_invalid_arguments_do_not_use_rpc(void)
{
    char response[2];
    TEST_ASSERT_EQUAL(HTTP_ERR_INVALID_ARG,
        m1_esp32_rpc_http_get(NULL, response, sizeof(response), 0u));
    TEST_ASSERT_EQUAL(HTTP_ERR_INVALID_ARG,
        m1_esp32_rpc_http_download_to_file("https://github.com/fw.bin", "",
                                           NULL, 0u));
    TEST_ASSERT_EQUAL_INT(0, s_step_count);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_get_streams_release_json);
    RUN_TEST(test_get_rejects_truncated_json);
    RUN_TEST(test_download_streams_binary_to_sd);
    RUN_TEST(test_open_dns_error_is_specific);
    RUN_TEST(test_old_brain_is_unsupported_not_dns);
    RUN_TEST(test_http_error_closes_stream);
    RUN_TEST(test_read_timeout_closes_and_deletes_partial_file);
    RUN_TEST(test_cancel_closes_and_deletes_partial_file);
    RUN_TEST(test_invalid_arguments_do_not_use_rpc);
    return UNITY_END();
}
