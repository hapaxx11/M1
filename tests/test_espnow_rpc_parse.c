/* See COPYING.txt for license details. */

/**
 * @file   test_espnow_rpc_parse.c
 * @brief  Host-side unit tests for espnow_rpc_parse.c.
 *
 * Regression guard for the M1_RPC_NOW_PEERS_GET / NOW_RECV_GET response
 * reception budget.  m1_espnow_hal.c used to pass a hardcoded SPI_BUF_SIZE
 * (64-byte) capacity to m1_esp32_rpc_call() for every M1_RPC_NOW_* call,
 * regardless of the caller's actual buffer size -- the same class of defect
 * fixed for the WiFi/BLE scan RPCs in m1_esp32_rpc_features.c.  A full
 * PEERS_GET response (up to ESPNOW_MAX_PEERS peers with ESPNOW_NAME_MAX-byte
 * names) is up to 497 bytes, and a full RECV_GET response is up to 249
 * bytes -- both larger than the old 64-byte reception ceiling.  These tests
 * decode a full-size synthetic response and confirm every peer / the whole
 * message survives, then reproduce the pre-fix truncation-to-64-bytes case
 * to document exactly how it used to silently drop data.
 */

#include "unity.h"
#include "espnow_rpc_parse.h"

#include <string.h>

void setUp(void)  {}
void tearDown(void) {}

/* =========================================================================
 * Reception budgets (mirrors ESPNOW_PEERS_RESP_MAX / ESPNOW_RECV_RESP_MAX
 * in m1_espnow_hal.c -- kept in sync manually since those are file-private).
 * =========================================================================*/

#define PEERS_RESP_MAX \
    (1u + (uint16_t)ESPNOW_MAX_PEERS * \
     (ESPNOW_MAC_LEN + 1u + 1u + ESPNOW_NAME_MAX))
#define RECV_RESP_MAX  (1u + ESPNOW_MAC_LEN + 2u + 240u)

/* Old, pre-fix reception ceiling that every M1_RPC_NOW_* call used to be
 * capped at regardless of the actual response size. */
#define OLD_SPI_BUF_SIZE  64u

/* =========================================================================
 * Helpers
 * =========================================================================*/

/* Builds a PEERS_GET response body with `count` peers, each using the
 * maximum-length (ESPNOW_NAME_MAX byte) name, and returns the encoded
 * length. */
static uint16_t build_peers_resp(uint8_t *out, uint8_t count)
{
    uint16_t off = 0;
    out[off++] = count;
    for (uint8_t i = 0; i < count; i++) {
        for (uint8_t b = 0; b < ESPNOW_MAC_LEN; b++)
            out[off++] = (uint8_t)(0x10 + i + b);
        out[off++] = (uint8_t)(-40 - i);           /* rssi */
        out[off++] = ESPNOW_NAME_MAX;              /* namelen */
        for (uint8_t c = 0; c < ESPNOW_NAME_MAX; c++)
            out[off++] = (uint8_t)('A' + ((i + c) % 26));
    }
    return off;
}

/* =========================================================================
 * PEERS_GET decode -- full-size response (post-fix reception budget)
 * =========================================================================*/

void test_parse_peers_full_list_all_peers_decoded(void)
{
    uint8_t body[PEERS_RESP_MAX];
    uint16_t blen = build_peers_resp(body, ESPNOW_MAX_PEERS);
    TEST_ASSERT_EQUAL_UINT16(PEERS_RESP_MAX, blen);
    TEST_ASSERT_GREATER_THAN_UINT16(OLD_SPI_BUF_SIZE, blen);

    espnow_peer_info_t peers[ESPNOW_MAX_PEERS];
    memset(peers, 0, sizeof(peers));

    uint8_t got = espnow_rpc_parse_peers(body, blen, peers,
                                        ESPNOW_MAX_PEERS, 6);
    TEST_ASSERT_EQUAL_UINT8(ESPNOW_MAX_PEERS, got);

    /* Last peer (would be dropped/corrupted under the old 64-byte cap). */
    const uint8_t last = ESPNOW_MAX_PEERS - 1;
    TEST_ASSERT_EQUAL_UINT8(0x10 + last, peers[last].mac[0]);
    TEST_ASSERT_EQUAL_INT8(-40 - last, peers[last].rssi);
    TEST_ASSERT_EQUAL_UINT8(6, peers[last].channel);
    TEST_ASSERT_EQUAL_UINT32(ESPNOW_NAME_MAX, strlen(peers[last].name));
}

/* =========================================================================
 * PEERS_GET decode -- regression: pre-fix 64-byte truncation dropped peers
 * =========================================================================*/

void test_parse_peers_old_64_byte_cap_silently_drops_peers(void)
{
    uint8_t body[PEERS_RESP_MAX];
    uint16_t blen = build_peers_resp(body, ESPNOW_MAX_PEERS);

    /* Simulate the pre-fix bug: m1_esp32_rpc_call() only had a 64-byte
     * resp_cap, so only the first OLD_SPI_BUF_SIZE bytes of the response
     * ever reached the parser, no matter how many peers the firmware
     * actually reported. */
    uint16_t truncated_len = OLD_SPI_BUF_SIZE;
    TEST_ASSERT_LESS_THAN_UINT16(blen, truncated_len + 1);

    espnow_peer_info_t peers[ESPNOW_MAX_PEERS];
    memset(peers, 0, sizeof(peers));

    uint8_t got = espnow_rpc_parse_peers(body, truncated_len, peers,
                                        ESPNOW_MAX_PEERS, 6);

    /* Each peer record's fixed 8-byte header (mac+rssi+namelen) must fit
     * within the truncated buffer for the peer to be counted at all (its
     * name may still be silently shortened). With the 64-byte cap this
     * still yields fewer decoded peers than were actually reported --
     * demonstrating the pre-fix data loss -- regardless of the exact
     * value of ESPNOW_NAME_MAX. */
    TEST_ASSERT_LESS_THAN_UINT8(ESPNOW_MAX_PEERS, got);
}

void test_parse_peers_null_out_rejected(void)
{
    uint8_t body[8] = {1, 0};
    TEST_ASSERT_EQUAL_UINT8(0, espnow_rpc_parse_peers(body, sizeof(body),
                                                       NULL, 4, 1));
}

void test_parse_peers_zero_count(void)
{
    uint8_t body[1] = {0};
    espnow_peer_info_t peers[4];
    TEST_ASSERT_EQUAL_UINT8(0, espnow_rpc_parse_peers(body, 1, peers, 4, 1));
}

void test_parse_peers_caps_to_max_peers(void)
{
    uint8_t body[PEERS_RESP_MAX];
    uint16_t blen = build_peers_resp(body, ESPNOW_MAX_PEERS);
    espnow_peer_info_t peers[4];
    memset(peers, 0, sizeof(peers));
    uint8_t got = espnow_rpc_parse_peers(body, blen, peers, 4, 1);
    TEST_ASSERT_EQUAL_UINT8(4, got);
}

/* =========================================================================
 * RECV_GET decode
 * =========================================================================*/

void test_parse_recv_full_length_message_decoded(void)
{
    uint8_t body[RECV_RESP_MAX];
    uint16_t off = 0;
    body[off++] = 1;                         /* count */
    for (uint8_t b = 0; b < ESPNOW_MAC_LEN; b++)
        body[off++] = (uint8_t)(0x20 + b);   /* sender mac */
    uint16_t msg_len = 240;                  /* ENL_MSG_MAX */
    body[off++] = (uint8_t)(msg_len & 0xFFu);
    body[off++] = (uint8_t)(msg_len >> 8);
    for (uint16_t i = 0; i < msg_len; i++)
        body[off++] = (uint8_t)(i & 0xFFu);
    TEST_ASSERT_EQUAL_UINT16(RECV_RESP_MAX, off);
    TEST_ASSERT_GREATER_THAN_UINT16(OLD_SPI_BUF_SIZE, off);

    uint8_t from_mac[6];
    uint8_t buf[240];
    uint8_t out_len = 0;
    bool ok = espnow_rpc_parse_recv(body, off, from_mac, buf, sizeof(buf),
                                    &out_len);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_UINT8(0x20, from_mac[0]);
    TEST_ASSERT_EQUAL_UINT8(240, out_len);
    TEST_ASSERT_EQUAL_UINT8(0, buf[0]);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)(239 & 0xFFu), buf[239]);
}

void test_parse_recv_old_64_byte_cap_truncates_message(void)
{
    uint8_t body[RECV_RESP_MAX];
    uint16_t off = 0;
    body[off++] = 1;
    for (uint8_t b = 0; b < ESPNOW_MAC_LEN; b++)
        body[off++] = (uint8_t)(0x20 + b);
    uint16_t msg_len = 240;
    body[off++] = (uint8_t)(msg_len & 0xFFu);
    body[off++] = (uint8_t)(msg_len >> 8);
    for (uint16_t i = 0; i < msg_len; i++)
        body[off++] = (uint8_t)(i & 0xFFu);

    /* Simulate the pre-fix 64-byte reception cap: only 64 bytes of the
     * 249-byte response ever reached the parser. */
    uint16_t truncated_len = OLD_SPI_BUF_SIZE;

    uint8_t from_mac[6];
    uint8_t buf[240];
    uint8_t out_len = 0xFFu;
    bool ok = espnow_rpc_parse_recv(body, truncated_len, from_mac, buf,
                                    sizeof(buf), &out_len);
    TEST_ASSERT_TRUE(ok);
    /* Only truncated_len - 9 header bytes of message data actually arrived. */
    TEST_ASSERT_EQUAL_UINT8(truncated_len - 9u, out_len);
    TEST_ASSERT_LESS_THAN_UINT8(240, out_len);
}

void test_parse_recv_empty_count_rejected(void)
{
    uint8_t body[1] = {0};
    uint8_t from_mac[6];
    uint8_t buf[16];
    uint8_t out_len = 0;
    TEST_ASSERT_FALSE(espnow_rpc_parse_recv(body, 1, from_mac, buf,
                                            sizeof(buf), &out_len));
}

void test_parse_recv_caps_to_buf_size(void)
{
    uint8_t body[RECV_RESP_MAX];
    uint16_t off = 0;
    body[off++] = 1;
    for (uint8_t b = 0; b < ESPNOW_MAC_LEN; b++)
        body[off++] = (uint8_t)(0x30 + b);
    uint16_t msg_len = 100;
    body[off++] = (uint8_t)(msg_len & 0xFFu);
    body[off++] = (uint8_t)(msg_len >> 8);
    for (uint16_t i = 0; i < msg_len; i++)
        body[off++] = (uint8_t)i;

    uint8_t from_mac[6];
    uint8_t buf[16];
    uint8_t out_len = 0;
    bool ok = espnow_rpc_parse_recv(body, off, from_mac, buf, sizeof(buf),
                                    &out_len);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_UINT8(sizeof(buf), out_len);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_parse_peers_full_list_all_peers_decoded);
    RUN_TEST(test_parse_peers_old_64_byte_cap_silently_drops_peers);
    RUN_TEST(test_parse_peers_null_out_rejected);
    RUN_TEST(test_parse_peers_zero_count);
    RUN_TEST(test_parse_peers_caps_to_max_peers);

    RUN_TEST(test_parse_recv_full_length_message_decoded);
    RUN_TEST(test_parse_recv_old_64_byte_cap_truncates_message);
    RUN_TEST(test_parse_recv_empty_count_rejected);
    RUN_TEST(test_parse_recv_caps_to_buf_size);

    return UNITY_END();
}
