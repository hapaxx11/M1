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
    TEST_ASSERT_TRUE(g_slave_count < MAX_TXNS);
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
    TEST_ASSERT_TRUE(g_slave_count < MAX_TXNS);
    memset(g_slave[g_slave_count], 0, MTU);
    g_slave_count++;
}

/* Queue a full slave frame whose header starts @p shift bytes into the MTU
 * buffer, modelling residue left in the SPI FIFO by a prior half-duplex (AT /
 * SiN360) transfer that byte-shifts the whole full-duplex frame.  The leading
 * @p shift bytes are filled with 0xFF garbage so a naive offset-0 parse fails. */
static void queue_frame_shifted(uint16_t shift, uint8_t msg_type,
                                uint16_t msg_id,
                                const uint8_t *payload, uint16_t plen)
{
    TEST_ASSERT_TRUE(g_slave_count < MAX_TXNS);
    TEST_ASSERT_TRUE((size_t)shift + M1_ESP32_RPC_HDR_SIZE + plen +
                     M1_ESP32_RPC_CRC_SIZE <= MTU);
    uint8_t *slot = g_slave[g_slave_count];
    memset(slot, 0xFF, shift);
    uint8_t *b = slot + shift;
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

/* ---- Delayed bulk response: reply arrives after MANY IDLE polls ---------
 *
 * Regression for the "every brain feature fails" root cause.  A WiFi/BLE scan
 * keeps the brain busy for a second or more before it queues its RESP, so the
 * host sees a long run of IDLE frames first.  The old fixed 8-poll budget
 * (with no inter-poll pacing) gave up long before that reply arrived.  This
 * proves the helper keeps polling through a long IDLE run and still matches a
 * late RESP when given the larger, time-scaled budget the on-target transport
 * now passes down. */
void test_m1link_waits_through_many_idle_polls_for_scan_reply(void)
{
    int req_len = build_req(M1_ESP32_RPC_WIFI_SCAN, NULL, 0u);

    /* 30 IDLE transactions model the ~1s+ the brain spends scanning before it
     * queues its response (30 polls * ~50-100ms handshake-paced each). */
    for (int i = 0; i < 30; i++)
        queue_frame(M1_ESP32_RPC_IDLE, 0u, NULL, 0u);

    /* The eventual WIFI_SCAN RESP: [count:2 LE] = 0 APs (empty environment). */
    const uint8_t scan_resp[2] = {0x00, 0x00};
    queue_frame(M1_ESP32_RPC_RESP, M1_ESP32_RPC_WIFI_SCAN, scan_resp, 2u);

    int out_len = 0;
    /* Budget comfortably larger than the old hard-coded 8. */
    uint8_t rc = m1_esp32_m1link_send_recv(fake_xfer, NULL, s_tx, s_rx, MTU, 40,
                                           s_req, req_len, s_out,
                                           (int)sizeof(s_out), &out_len);
    TEST_ASSERT_EQUAL_UINT8(0u, rc);
    const uint8_t *pl = NULL; uint16_t pl_len = 0u;
    TEST_ASSERT_EQUAL_INT(M1_ESP32_RPC_OK,
        m1_esp32_rpc_decode_resp(s_out, (uint16_t)out_len,
                                 M1_ESP32_RPC_WIFI_SCAN, &pl, &pl_len));
    TEST_ASSERT_EQUAL_UINT16(2u, pl_len);
    TEST_ASSERT_EQUAL_MEMORY(scan_resp, pl, 2);
    /* All 30 IDLE polls plus the final RESP transaction were consumed. */
    TEST_ASSERT_EQUAL_INT(31, g_tx_calls);
}

/* ---- The same delayed reply is LOST under the old 8-poll budget ----------
 * Mirrors the pre-fix behaviour: with only 8 follow-up polls the host returns
 * "no match" while the brain is still mid-scan, which is exactly the
 * "AP scan failed, please try again" symptom reported against the brain. */
void test_m1link_short_budget_drops_delayed_scan_reply(void)
{
    int req_len = build_req(M1_ESP32_RPC_WIFI_SCAN, NULL, 0u);

    for (int i = 0; i < 12; i++)
        queue_frame(M1_ESP32_RPC_IDLE, 0u, NULL, 0u);
    const uint8_t scan_resp[2] = {0x00, 0x00};
    queue_frame(M1_ESP32_RPC_RESP, M1_ESP32_RPC_WIFI_SCAN, scan_resp, 2u);

    int out_len = 0;
    /* Old fixed budget: request + 8 polls — the RESP (txn 12) never arrives. */
    uint8_t rc = m1_esp32_m1link_send_recv(fake_xfer, NULL, s_tx, s_rx, MTU, 8,
                                           s_req, req_len, s_out,
                                           (int)sizeof(s_out), &out_len);
    TEST_ASSERT_NOT_EQUAL(0u, rc);
    TEST_ASSERT_EQUAL_INT(0, out_len);
    TEST_ASSERT_EQUAL_INT(9, g_tx_calls);
}

/* ---- Byte-shifted response is recovered (FIFO-residue root cause) --------
 *
 * Regression for the "ESP32 Unknown (fallback)" brain-CD3 detection failure.
 * When the full-duplex M1 Link probe runs after a half-duplex AT / SiN360
 * transfer that left a byte of residue in the SPI FIFO, the whole reply frame
 * is shifted a few bytes into the received buffer.  The helper must locate the
 * frame by scanning for the RPC magic rather than trusting offset 0, and must
 * copy the response out from the located frame base (not the buffer start). */
void test_m1link_recovers_byte_shifted_response(void)
{
    int req_len = build_req(M1_ESP32_RPC_SYS_GET_STATUS, NULL, 0u);

    queue_frame(M1_ESP32_RPC_IDLE, 0u, NULL, 0u);            /* txn 0 */
    const uint8_t body[3] = {0xAA, 0xBB, 0xCC};
    /* txn 1: the RESP, shifted 3 bytes deep by FIFO residue. */
    queue_frame_shifted(3u, M1_ESP32_RPC_RESP,
                        M1_ESP32_RPC_SYS_GET_STATUS, body, 3u);

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

/* ---- Byte-shifted PING echo is recovered (the actual detection probe) ---- */
void test_m1link_recovers_byte_shifted_ping(void)
{
    const uint8_t cookie[4] = {0x4D, 0x31, 0x50, 0x49}; /* "M1PI" */
    int req_len = build_req(M1_ESP32_RPC_SYS_PING, cookie, 4u);

    /* txn 0: the PING RESP, shifted a single byte into the buffer. */
    queue_frame_shifted(1u, M1_ESP32_RPC_RESP,
                        M1_ESP32_RPC_SYS_PING, cookie, 4u);

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
    TEST_ASSERT_EQUAL_MEMORY(cookie, pl, 4);
}

/* ---- FRAG chain whose frames are shifted still reassembles --------------- */
void test_m1link_reassembles_shifted_fragments(void)
{
    int req_len = build_req(M1_ESP32_RPC_SYS_GET_STATUS, NULL, 0u);

    const uint8_t part1[3] = {0x01, 0x02, 0x03};
    const uint8_t part2[2] = {0x04, 0x05};
    queue_frame_shifted(2u, M1_ESP32_RPC_FRAG,
                        M1_ESP32_RPC_SYS_GET_STATUS, part1, 3u);
    queue_frame_shifted(5u, M1_ESP32_RPC_RESP,
                        M1_ESP32_RPC_SYS_GET_STATUS, part2, 2u);

    int out_len = 0;
    uint8_t rc = m1_esp32_m1link_send_recv(fake_xfer, NULL, s_tx, s_rx, MTU, 8,
                                           s_req, req_len, s_out,
                                           (int)sizeof(s_out), &out_len);
    TEST_ASSERT_EQUAL_UINT8(0u, rc);
    const uint8_t *pl = NULL; uint16_t pl_len = 0u;
    TEST_ASSERT_EQUAL_INT(M1_ESP32_RPC_OK,
        m1_esp32_rpc_decode_resp(s_out, (uint16_t)out_len,
                                 M1_ESP32_RPC_SYS_GET_STATUS, &pl, &pl_len));
    TEST_ASSERT_EQUAL_UINT16(5u, pl_len);
    const uint8_t expect[5] = {0x01, 0x02, 0x03, 0x04, 0x05};
    TEST_ASSERT_EQUAL_MEMORY(expect, pl, 5);
}

/* ---- A stray magic pair before the real frame must not cause a mismatch --
 * The scan skips a candidate offset that fails version/CRC validation and
 * keeps looking, so a 0x4D 0x31 pair sitting in the residue does not shadow
 * the genuine frame that follows it. */
void test_m1link_ignores_stray_magic_before_real_frame(void)
{
    int req_len = build_req(M1_ESP32_RPC_SYS_PING, NULL, 0u);

    const uint8_t cookie[4] = {7, 7, 7, 7};
    queue_frame_shifted(4u, M1_ESP32_RPC_RESP,
                        M1_ESP32_RPC_SYS_PING, cookie, 4u);
    /* Plant a bogus magic pair in the leading residue (invalid version). */
    g_slave[0][0] = (uint8_t)(M1_ESP32_RPC_MAGIC & 0xFFu);
    g_slave[0][1] = (uint8_t)((M1_ESP32_RPC_MAGIC >> 8u) & 0xFFu);
    g_slave[0][2] = 0x7Eu; /* not M1_ESP32_RPC_VERSION */

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
    TEST_ASSERT_EQUAL_MEMORY(cookie, pl, 4);
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
    RUN_TEST(test_m1link_waits_through_many_idle_polls_for_scan_reply);
    RUN_TEST(test_m1link_short_budget_drops_delayed_scan_reply);
    RUN_TEST(test_m1link_recovers_byte_shifted_response);
    RUN_TEST(test_m1link_recovers_byte_shifted_ping);
    RUN_TEST(test_m1link_reassembles_shifted_fragments);
    RUN_TEST(test_m1link_ignores_stray_magic_before_real_frame);
    return UNITY_END();
}
