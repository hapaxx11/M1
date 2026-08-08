/* See COPYING.txt for license details. */

/*
 * test_esp32_m1link.c — host-side unit tests for the full-duplex "M1 Link"
 * framing/pipelining helper m1_esp32_m1link_send_recv().
 *
 * The native brain CD3 firmware is an ESP-IDF full-duplex `spi_slave` device:
 * every transaction clocks EXACTLY 512 bytes in both directions and the reply
 * is pipelined onto a LATER transaction.  The pure helper drives that protocol
 * over an injected single-transaction exchange primitive; these tests supply a
 * fake exchange that replays a scripted queue of slave frames so the framing,
 * IDLE/EVENT skipping, msg_id matching, FRAG reassembly, and poll-budget
 * behaviour all run on the host.
 *
 * Regression guard for the PR #684 follow-up: brain-CD3 features were broken
 * because the M1 side spoke the wrong SPI transport.  This verifies the
 * replacement transport's logic.
 */

#include <string.h>
#include "unity.h"
#include "m1_esp32_rpc.h"

/* The rpc module (linked in) references these on-target symbols; stub them. */
static uint64_t g_bitmap;
uint64_t m1_esp32_caps_get_bitmap(void) { return g_bitmap; }

uint8_t spi_AT_send_recv_bin(const uint8_t *tx_buf, int tx_len,
                             uint8_t *rx_buf, int rx_buf_size,
                             int *out_len, int timeout_sec)
{
    (void)tx_buf; (void)tx_len; (void)rx_buf; (void)rx_buf_size; (void)timeout_sec;
    if (out_len) *out_len = 0;
    return 1;
}
uint8_t spi_m1link_send_recv_bin(const uint8_t *tx_buf, int tx_len,
                                 uint8_t *rx_buf, int rx_buf_size,
                                 int *out_len, int timeout_sec)
{
    (void)tx_buf; (void)tx_len; (void)rx_buf; (void)rx_buf_size; (void)timeout_sec;
    if (out_len) *out_len = 0;
    return 1;
}

/* ------------------------------------------------------------------ */
/* Fake full-duplex exchange: replays a scripted queue of slave frames */
/* ------------------------------------------------------------------ */

#define MTU        M1_ESP32_M1LINK_MTU
#define MAX_TXNS   32

/* Each scripted slave frame occupies one MTU-sized slot. */
static uint8_t g_slave[MAX_TXNS][MTU];
static int     g_slave_count;      /* number of scripted frames */
static int     g_call;             /* transactions performed so far */
static int     g_fail_at;          /* transaction index to fail (-1 = never) */

/* Record of the payloads the master clocked out, for assertions. */
static uint8_t g_tx_type[MAX_TXNS]; /* msg_type of each master frame */
static int     g_tx_calls;

static void fake_reset(void)
{
    memset(g_slave, 0, sizeof(g_slave));
    g_slave_count = 0;
    g_call = 0;
    g_fail_at = -1;
    memset(g_tx_type, 0, sizeof(g_tx_type));
    g_tx_calls = 0;
}

/* Queue a full slave frame (header+payload+crc) as the reply for the Nth txn. */
static void queue_frame(uint8_t msg_type, uint16_t msg_id,
                        const uint8_t *payload, uint16_t plen)
{
    TEST_ASSERT_LESS_THAN_INT(MAX_TXNS, g_slave_count);
    uint8_t *b = g_slave[g_slave_count];
    b[0] = (uint8_t)(M1_ESP32_RPC_MAGIC & 0xFFu);
    b[1] = (uint8_t)((M1_ESP32_RPC_MAGIC >> 8u) & 0xFFu);
    b[2] = M1_ESP32_RPC_VERSION;
    b[3] = msg_type;
    b[4] = (uint8_t)(msg_id & 0xFFu);
    b[5] = (uint8_t)((msg_id >> 8u) & 0xFFu);
    b[6] = (uint8_t)(plen & 0xFFu);
    b[7] = (uint8_t)((plen >> 8u) & 0xFFu);
    if (payload && plen)
        memcpy(b + M1_ESP32_RPC_HDR_SIZE, payload, plen);
    uint16_t crc = m1_esp32_rpc_crc16(b, M1_ESP32_RPC_HDR_SIZE + plen);
    b[M1_ESP32_RPC_HDR_SIZE + plen]      = (uint8_t)(crc & 0xFFu);
    b[M1_ESP32_RPC_HDR_SIZE + plen + 1u] = (uint8_t)((crc >> 8u) & 0xFFu);
    g_slave_count++;
}

/* Queue an all-zero (invalid / pure padding) slave frame. */
static void queue_blank(void)
{
    TEST_ASSERT_LESS_THAN_INT(MAX_TXNS, g_slave_count);
    memset(g_slave[g_slave_count], 0, MTU);
    g_slave_count++;
}

static int fake_xfer(const uint8_t *tx, uint8_t *rx, uint16_t mtu, void *ctx)
{
    (void)ctx;
    TEST_ASSERT_EQUAL_UINT16(MTU, mtu);

    /* Record the master frame's msg_type (head of the padded buffer). */
    if (g_tx_calls < MAX_TXNS)
        g_tx_type[g_tx_calls] = tx[3];
    g_tx_calls++;

    if (g_fail_at >= 0 && g_call == g_fail_at) {
        g_call++;
        return -1; /* simulate a transport error on this transaction */
    }

    memset(rx, 0, mtu);
    if (g_call < g_slave_count)
        memcpy(rx, g_slave[g_call], mtu);
    /* else: nothing queued -> leave rx all-zero (invalid frame). */
    g_call++;
    return 0;
}

/* ------------------------------------------------------------------ */

static uint8_t   s_tx[MTU];
static uint8_t   s_rx[MTU];
static uint8_t   s_req[MTU];
static uint8_t   s_out[MTU];

/* Build a REQ frame into s_req and return its length. */
static int build_req(uint16_t msg_id, const uint8_t *payload, uint16_t plen)
{
    return (int)m1_esp32_rpc_build_req(s_req, (uint16_t)sizeof(s_req),
                                       msg_id, payload, plen);
}

void setUp(void)    { fake_reset(); }
void tearDown(void) {}

/* ---- Response on the first poll (transaction after the request) -------- */
void test_m1link_resp_on_first_poll(void)
{
    const uint8_t cookie[4] = {1, 2, 3, 4};
    int req_len = build_req(M1_ESP32_RPC_SYS_PING, cookie, 4u);

    /* txn 0 (request): brain not ready yet -> IDLE.  txn 1: the RESP. */
    queue_frame(M1_ESP32_RPC_IDLE, 0u, NULL, 0u);
    queue_frame(M1_ESP32_RPC_RESP, M1_ESP32_RPC_SYS_PING, cookie, 4u);

    int out_len = 0;
    uint8_t rc = m1_esp32_m1link_send_recv(fake_xfer, NULL, s_tx, s_rx, MTU, 8,
                                           s_req, req_len, s_out,
                                           (int)sizeof(s_out), &out_len);
    TEST_ASSERT_EQUAL_UINT8(0u, rc);
    TEST_ASSERT_EQUAL_INT(M1_ESP32_RPC_HDR_SIZE + 4 + M1_ESP32_RPC_CRC_SIZE,
                          out_len);

    /* The copied frame must decode cleanly as a RESP for our msg_id. */
    const uint8_t *pl = NULL; uint16_t pl_len = 0u;
    TEST_ASSERT_EQUAL_INT(M1_ESP32_RPC_OK,
        m1_esp32_rpc_decode_resp(s_out, (uint16_t)out_len,
                                 M1_ESP32_RPC_SYS_PING, &pl, &pl_len));
    TEST_ASSERT_EQUAL_UINT16(4u, pl_len);
    TEST_ASSERT_EQUAL_MEMORY(cookie, pl, 4);

    /* First txn sends REQ, the follow-up sends an IDLE filler. */
    TEST_ASSERT_EQUAL_UINT8(M1_ESP32_RPC_REQ, g_tx_type[0]);
    TEST_ASSERT_EQUAL_UINT8(M1_ESP32_RPC_IDLE, g_tx_type[1]);
}

/* ---- Pipelining: response several transactions later ------------------- */
void test_m1link_resp_after_several_polls(void)
{
    int req_len = build_req(M1_ESP32_RPC_SYS_GET_STATUS, NULL, 0u);

    queue_frame(M1_ESP32_RPC_IDLE, 0u, NULL, 0u);      /* txn 0 */
    queue_frame(M1_ESP32_RPC_IDLE, 0u, NULL, 0u);      /* txn 1 */
    queue_blank();                                     /* txn 2: padding */
    const uint8_t body[3] = {0xAA, 0xBB, 0xCC};
    queue_frame(M1_ESP32_RPC_RESP, M1_ESP32_RPC_SYS_GET_STATUS, body, 3u);

    int out_len = 0;
    uint8_t rc = m1_esp32_m1link_send_recv(fake_xfer, NULL, s_tx, s_rx, MTU, 8,
                                           s_req, req_len, s_out,
                                           (int)sizeof(s_out), &out_len);
    TEST_ASSERT_EQUAL_UINT8(0u, rc);
    const uint8_t *pl = NULL; uint16_t pl_len = 0u;
    TEST_ASSERT_EQUAL_INT(M1_ESP32_RPC_OK,
        m1_esp32_rpc_decode_resp(s_out, (uint16_t)out_len,
                                 M1_ESP32_RPC_SYS_GET_STATUS, &pl, &pl_len));
    TEST_ASSERT_EQUAL_UINT16(3u, pl_len);
    TEST_ASSERT_EQUAL_MEMORY(body, pl, 3);
}

/* ---- EVENT frames (different msg_id) are skipped ----------------------- */
void test_m1link_skips_event_frames(void)
{
    const uint8_t cookie[4] = {9, 9, 9, 9};
    int req_len = build_req(M1_ESP32_RPC_SYS_PING, cookie, 4u);

    /* An unsolicited EVENT with its own id must not satisfy the request. */
    const uint8_t evt[2] = {0x01, 0x02};
    queue_frame(M1_ESP32_RPC_EVENT, 0xE001u, evt, 2u);   /* txn 0 */
    queue_frame(M1_ESP32_RPC_RESP, M1_ESP32_RPC_SYS_PING, cookie, 4u); /*txn1*/

    int out_len = 0;
    uint8_t rc = m1_esp32_m1link_send_recv(fake_xfer, NULL, s_tx, s_rx, MTU, 8,
                                           s_req, req_len, s_out,
                                           (int)sizeof(s_out), &out_len);
    TEST_ASSERT_EQUAL_UINT8(0u, rc);
    const uint8_t *pl = NULL; uint16_t pl_len = 0u;
    TEST_ASSERT_EQUAL_INT(M1_ESP32_RPC_OK,
        m1_esp32_rpc_decode_resp(s_out, (uint16_t)out_len,
                                 M1_ESP32_RPC_SYS_PING, &pl, &pl_len));
    TEST_ASSERT_EQUAL_UINT16(4u, pl_len);
}

/* ---- FRAG chain is reassembled into one RESP frame --------------------- */
void test_m1link_reassembles_fragments(void)
{
    int req_len = build_req(M1_ESP32_RPC_SYS_GET_STATUS, NULL, 0u);

    uint8_t part1[5] = {0x10, 0x11, 0x12, 0x13, 0x14};
    uint8_t part2[3] = {0x20, 0x21, 0x22};
    uint8_t part3[2] = {0x30, 0x31};
    queue_frame(M1_ESP32_RPC_FRAG, M1_ESP32_RPC_SYS_GET_STATUS, part1, 5u);
    queue_frame(M1_ESP32_RPC_FRAG, M1_ESP32_RPC_SYS_GET_STATUS, part2, 3u);
    queue_frame(M1_ESP32_RPC_RESP, M1_ESP32_RPC_SYS_GET_STATUS, part3, 2u);

    int out_len = 0;
    uint8_t rc = m1_esp32_m1link_send_recv(fake_xfer, NULL, s_tx, s_rx, MTU, 8,
                                           s_req, req_len, s_out,
                                           (int)sizeof(s_out), &out_len);
    TEST_ASSERT_EQUAL_UINT8(0u, rc);

    const uint8_t *pl = NULL; uint16_t pl_len = 0u;
    TEST_ASSERT_EQUAL_INT(M1_ESP32_RPC_OK,
        m1_esp32_rpc_decode_resp(s_out, (uint16_t)out_len,
                                 M1_ESP32_RPC_SYS_GET_STATUS, &pl, &pl_len));
    TEST_ASSERT_EQUAL_UINT16(10u, pl_len);
    uint8_t expect[10] = {0x10,0x11,0x12,0x13,0x14, 0x20,0x21,0x22, 0x30,0x31};
    TEST_ASSERT_EQUAL_MEMORY(expect, pl, 10);
}

/* ---- NAK is delivered (decode surfaces the status byte) ---------------- */
void test_m1link_delivers_nak(void)
{
    int req_len = build_req(M1_ESP32_RPC_SYS_GET_STATUS, NULL, 0u);
    uint8_t status[1] = { (uint8_t)M1_ESP32_RPC_ERR_UNSUPPORTED };
    queue_frame(M1_ESP32_RPC_NAK, M1_ESP32_RPC_SYS_GET_STATUS, status, 1u);

    int out_len = 0;
    uint8_t rc = m1_esp32_m1link_send_recv(fake_xfer, NULL, s_tx, s_rx, MTU, 8,
                                           s_req, req_len, s_out,
                                           (int)sizeof(s_out), &out_len);
    TEST_ASSERT_EQUAL_UINT8(0u, rc); /* helper succeeds: it matched a frame */

    const uint8_t *pl = NULL; uint16_t pl_len = 0u;
    TEST_ASSERT_EQUAL_INT(M1_ESP32_RPC_ERR_UNSUPPORTED,
        m1_esp32_rpc_decode_resp(s_out, (uint16_t)out_len,
                                 M1_ESP32_RPC_SYS_GET_STATUS, &pl, &pl_len));
}

/* ---- No matching reply within the poll budget -> non-zero -------------- */
void test_m1link_timeout_when_never_answered(void)
{
    int req_len = build_req(M1_ESP32_RPC_SYS_PING, NULL, 0u);
    /* Every transaction returns IDLE -> never satisfied. */
    for (int i = 0; i < 12; i++)
        queue_frame(M1_ESP32_RPC_IDLE, 0u, NULL, 0u);

    int out_len = 0;
    uint8_t rc = m1_esp32_m1link_send_recv(fake_xfer, NULL, s_tx, s_rx, MTU, 4,
                                           s_req, req_len, s_out,
                                           (int)sizeof(s_out), &out_len);
    TEST_ASSERT_NOT_EQUAL(0u, rc);
    TEST_ASSERT_EQUAL_INT(0, out_len);
    /* Budget honoured: request + max_polls follow-ups = 5 transactions. */
    TEST_ASSERT_EQUAL_INT(5, g_tx_calls);
}

/* ---- Transport error propagates ---------------------------------------- */
void test_m1link_transport_error(void)
{
    int req_len = build_req(M1_ESP32_RPC_SYS_PING, NULL, 0u);
    g_fail_at = 0; /* fail on the very first transaction */

    int out_len = 0;
    uint8_t rc = m1_esp32_m1link_send_recv(fake_xfer, NULL, s_tx, s_rx, MTU, 8,
                                           s_req, req_len, s_out,
                                           (int)sizeof(s_out), &out_len);
    TEST_ASSERT_NOT_EQUAL(0u, rc);
    TEST_ASSERT_EQUAL_INT(0, out_len);
}

/* ---- Invalid arguments are rejected ------------------------------------ */
void test_m1link_invalid_args(void)
{
    int req_len = build_req(M1_ESP32_RPC_SYS_PING, NULL, 0u);
    int out_len = 0;

    /* NULL xfer */
    TEST_ASSERT_NOT_EQUAL(0u,
        m1_esp32_m1link_send_recv(NULL, NULL, s_tx, s_rx, MTU, 8,
                                  s_req, req_len, s_out,
                                  (int)sizeof(s_out), &out_len));
    /* max_polls < 1 */
    TEST_ASSERT_NOT_EQUAL(0u,
        m1_esp32_m1link_send_recv(fake_xfer, NULL, s_tx, s_rx, MTU, 0,
                                  s_req, req_len, s_out,
                                  (int)sizeof(s_out), &out_len));
    /* tx_len > mtu */
    TEST_ASSERT_NOT_EQUAL(0u,
        m1_esp32_m1link_send_recv(fake_xfer, NULL, s_tx, s_rx, MTU, 8,
                                  s_req, (int)MTU + 1, s_out,
                                  (int)sizeof(s_out), &out_len));
}

/* ---- Ignores a stale frame whose msg_id differs, then matches ---------- */
void test_m1link_ignores_wrong_msg_id(void)
{
    int req_len = build_req(M1_ESP32_RPC_SYS_GET_STATUS, NULL, 0u);

    const uint8_t stale[2] = {0x55, 0x66};
    /* A well-formed RESP for a DIFFERENT command must be skipped. */
    queue_frame(M1_ESP32_RPC_RESP, M1_ESP32_RPC_SYS_PING, stale, 2u);
    const uint8_t body[2] = {0x77, 0x88};
    queue_frame(M1_ESP32_RPC_RESP, M1_ESP32_RPC_SYS_GET_STATUS, body, 2u);

    int out_len = 0;
    uint8_t rc = m1_esp32_m1link_send_recv(fake_xfer, NULL, s_tx, s_rx, MTU, 8,
                                           s_req, req_len, s_out,
                                           (int)sizeof(s_out), &out_len);
    TEST_ASSERT_EQUAL_UINT8(0u, rc);
    const uint8_t *pl = NULL; uint16_t pl_len = 0u;
    TEST_ASSERT_EQUAL_INT(M1_ESP32_RPC_OK,
        m1_esp32_rpc_decode_resp(s_out, (uint16_t)out_len,
                                 M1_ESP32_RPC_SYS_GET_STATUS, &pl, &pl_len));
    TEST_ASSERT_EQUAL_MEMORY(body, pl, 2);
}

/* ---- Malformed oversized frame is rejected without OOB access ---------- */
void test_m1link_rejects_oversized_declared_payload(void)
{
    int req_len = build_req(M1_ESP32_RPC_SYS_PING, NULL, 0u);

    queue_blank();
    uint8_t *bad = g_slave[0];
    bad[0] = (uint8_t)(M1_ESP32_RPC_MAGIC & 0xFFu);
    bad[1] = (uint8_t)((M1_ESP32_RPC_MAGIC >> 8u) & 0xFFu);
    bad[2] = M1_ESP32_RPC_VERSION;
    bad[3] = M1_ESP32_RPC_RESP;
    bad[4] = (uint8_t)(M1_ESP32_RPC_SYS_PING & 0xFFu);
    bad[5] = (uint8_t)((M1_ESP32_RPC_SYS_PING >> 8u) & 0xFFu);
    bad[6] = 0xFFu;
    bad[7] = 0xFFu; /* Declares a payload larger than this buffer. */

    const uint8_t ok[1] = {0x42};
    queue_frame(M1_ESP32_RPC_RESP, M1_ESP32_RPC_SYS_PING, ok, 1u);

    int out_len = 0;
    uint8_t rc = m1_esp32_m1link_send_recv(fake_xfer, NULL, s_tx, s_rx, MTU, 4,
                                           s_req, req_len, s_out,
                                           (int)sizeof(s_out), &out_len);
    TEST_ASSERT_EQUAL_UINT8(0u, rc);
    TEST_ASSERT_EQUAL_INT(M1_ESP32_RPC_HDR_SIZE + 1 + M1_ESP32_RPC_CRC_SIZE,
                          out_len);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_m1link_resp_on_first_poll);
    RUN_TEST(test_m1link_resp_after_several_polls);
    RUN_TEST(test_m1link_skips_event_frames);
    RUN_TEST(test_m1link_reassembles_fragments);
    RUN_TEST(test_m1link_delivers_nak);
    RUN_TEST(test_m1link_timeout_when_never_answered);
    RUN_TEST(test_m1link_transport_error);
    RUN_TEST(test_m1link_invalid_args);
    RUN_TEST(test_m1link_ignores_wrong_msg_id);
    RUN_TEST(test_m1link_rejects_oversized_declared_payload);
    return UNITY_END();
}
