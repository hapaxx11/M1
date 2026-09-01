/* See COPYING.txt for license details. */

/*
 * test_esp32_rpc.c — host-side unit tests for the M1_RPC compatibility layer.
 *
 * Covers:
 *   - m1_esp32_rpc_call(): REQ build -> transport -> RESP/NAK decode, payload
 *     copy + truncation, transport failure, oversize guard.
 *   - m1_esp32_rpc_decode_resp(): RESP vs NAK vs framing errors.
 *   - esp32_firmware_transport() / m1_esp32_active_transport(): AT / binary-SPI
 *     / RPC / none classification.
 *
 * The client's SPI-HD transport (spi_AT_send_recv_bin) and the capability cache
 * (m1_esp32_caps_get_bitmap) are stubbed here so the pure request/response
 * logic runs without hardware.
 */

#include <string.h>
#include "unity.h"
#include "m1_esp32_rpc.h"

/* ------------------------------------------------------------------ */
/* Stubs for the on-target symbols the client links against.          */
/* ------------------------------------------------------------------ */

/* Selectable capability bitmap for the transport-classifier tests. */
static uint64_t g_bitmap;
uint64_t m1_esp32_caps_get_bitmap(void) { return g_bitmap; }

/* Controllable capability-probe state for the self-priming regression tests
 * (m1_esp32_active_transport() below).  Existing tests set g_bitmap directly
 * and expect no re-probe, so default g_queried true (bitmap already
 * "current") and only flip it for the dedicated priming tests. */
static bool     g_queried;
static bool     g_hal_ready;
static uint64_t g_caps_init_bitmap;
static int      g_caps_init_calls;

bool m1_esp32_caps_is_queried(void) { return g_queried; }
uint8_t m1_esp32_get_init_status(void) { return g_hal_ready ? 1u : 0u; }
void m1_esp32_caps_init(void)
{
    g_caps_init_calls++;
    g_bitmap  = g_caps_init_bitmap;
    g_queried = true;
}

/* The default transport pointer references this symbol; provide a stub so the
 * module links.  Tests install a fake via m1_esp32_rpc_set_transport(). */
uint8_t spi_AT_send_recv_bin(const uint8_t *tx_buf, int tx_len,
                             uint8_t *rx_buf, int rx_buf_size,
                             int *out_len, int timeout_sec)
{
    (void)tx_buf; (void)tx_len; (void)rx_buf; (void)rx_buf_size;
    (void)timeout_sec;
    if (out_len) *out_len = 0;
    return 1; /* non-zero == transport error */
}

/* The default transport is now the full-duplex M1 Link path; stub it too so the
 * module links.  Tests install a fake via m1_esp32_rpc_set_transport(). */
uint8_t spi_m1link_send_recv_bin(const uint8_t *tx_buf, int tx_len,
                                 uint8_t *rx_buf, int rx_buf_size,
                                 int *out_len, int timeout_sec)
{
    (void)tx_buf; (void)tx_len; (void)rx_buf; (void)rx_buf_size;
    (void)timeout_sec;
    if (out_len) *out_len = 0;
    return 1; /* non-zero == transport error */
}

/* Controllable elapsed-time stub for the Phase 6 "wall-clock diagnostic"
 * tests (issue #719) -- on-target this is populated by
 * spi_m1link_send_recv_bin() from HAL_GetTick(). */
static uint32_t g_last_elapsed_ms;
uint32_t m1_esp32_m1link_last_elapsed_ms(void) { return g_last_elapsed_ms; }

/* ------------------------------------------------------------------ */
/* Fake transport: replays a canned response frame.                   */
/* ------------------------------------------------------------------ */

static uint8_t g_canned[256];
static int     g_canned_len;
static uint8_t g_ret;             /* value the fake returns (0 == SUCCESS) */
static uint8_t g_last_tx[256];    /* copy of the last request frame sent */
static int     g_last_tx_len;

static uint8_t fake_transport(const uint8_t *tx_buf, int tx_len,
                              uint8_t *rx_buf, int rx_buf_size,
                              int *out_len, int timeout_sec)
{
    (void)timeout_sec;
    g_last_tx_len = (tx_len < (int)sizeof(g_last_tx)) ? tx_len
                                                      : (int)sizeof(g_last_tx);
    memcpy(g_last_tx, tx_buf, (size_t)g_last_tx_len);

    if (g_ret != 0) {
        if (out_len) *out_len = 0;
        return g_ret;
    }
    int n = (g_canned_len < rx_buf_size) ? g_canned_len : rx_buf_size;
    memcpy(rx_buf, g_canned, (size_t)n);
    if (out_len) *out_len = n;
    return 0;
}

/* Build an arbitrary-msg_type M1_RPC frame (REQ builder is REQ-only). */
static int make_frame(uint8_t *buf, uint8_t msg_type, uint16_t msg_id,
                      const uint8_t *payload, uint16_t plen)
{
    buf[0] = (uint8_t)(M1_ESP32_RPC_MAGIC & 0xFFu);
    buf[1] = (uint8_t)((M1_ESP32_RPC_MAGIC >> 8u) & 0xFFu);
    buf[2] = M1_ESP32_RPC_VERSION;
    buf[3] = msg_type;
    buf[4] = (uint8_t)(msg_id & 0xFFu);
    buf[5] = (uint8_t)((msg_id >> 8u) & 0xFFu);
    buf[6] = (uint8_t)(plen & 0xFFu);
    buf[7] = (uint8_t)((plen >> 8u) & 0xFFu);
    if (payload && plen)
        memcpy(buf + M1_ESP32_RPC_HDR_SIZE, payload, plen);
    uint16_t crc = m1_esp32_rpc_crc16(buf, M1_ESP32_RPC_HDR_SIZE + plen);
    buf[M1_ESP32_RPC_HDR_SIZE + plen]      = (uint8_t)(crc & 0xFFu);
    buf[M1_ESP32_RPC_HDR_SIZE + plen + 1u] = (uint8_t)((crc >> 8u) & 0xFFu);
    return (int)(M1_ESP32_RPC_HDR_SIZE + plen + M1_ESP32_RPC_CRC_SIZE);
}

void setUp(void)
{
    g_bitmap = 0u;
    g_queried = true;
    g_hal_ready = true;
    g_caps_init_bitmap = 0u;
    g_caps_init_calls = 0;
    g_canned_len = 0;
    g_ret = 0;
    g_last_tx_len = 0;
    g_last_elapsed_ms = 0u;
    memset(g_canned, 0, sizeof(g_canned));
    m1_esp32_rpc_set_transport(fake_transport);
}

void tearDown(void)
{
    m1_esp32_rpc_set_transport(NULL); /* restore default */
}

/* ================================================================== */
/* m1_esp32_rpc_call()                                                */
/* ================================================================== */

void test_call_resp_ok_copies_payload(void)
{
    const uint8_t body[] = {0x00, 0xAA, 0xBB, 0xCC};
    g_canned_len = make_frame(g_canned, M1_ESP32_RPC_RESP,
                              M1_ESP32_RPC_NOW_ANNOUNCE, body, sizeof(body));

    uint8_t  resp[16];
    uint16_t rlen = 0;
    m1_esp32_rpc_status_t st =
        m1_esp32_rpc_call(M1_ESP32_RPC_NOW_ANNOUNCE, NULL, 0,
                          resp, sizeof(resp), &rlen, 1);

    TEST_ASSERT_EQUAL_UINT8(M1_ESP32_RPC_OK, st);
    TEST_ASSERT_EQUAL_UINT16(sizeof(body), rlen);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(body, resp, sizeof(body));
}

void test_call_builds_valid_request_frame(void)
{
    const uint8_t req[] = {0x07, 'M', '1'};
    g_canned_len = make_frame(g_canned, M1_ESP32_RPC_RESP,
                              M1_ESP32_RPC_NOW_START, NULL, 0);

    (void)m1_esp32_rpc_call(M1_ESP32_RPC_NOW_START, req, sizeof(req),
                            NULL, 0, NULL, 1);

    /* The frame the client emitted must be a valid REQ the ESP32 can parse. */
    const uint8_t *pl = NULL;
    uint16_t pl_len = 0;
    TEST_ASSERT_EQUAL_INT(M1_ESP32_RPC_HDR_SIZE + (int)sizeof(req) +
                          M1_ESP32_RPC_CRC_SIZE, g_last_tx_len);
    TEST_ASSERT_EQUAL_UINT8(M1_ESP32_RPC_REQ, g_last_tx[3]);
    /* parse_resp only accepts RESP, so validate via decode against a REQ is
     * not meaningful; instead re-check CRC/magic by decoding as a generic
     * frame: flip type to RESP in a scratch copy and decode. */
    uint8_t scratch[64];
    memcpy(scratch, g_last_tx, (size_t)g_last_tx_len);
    scratch[3] = M1_ESP32_RPC_RESP;
    uint16_t crc = m1_esp32_rpc_crc16(scratch,
                                      M1_ESP32_RPC_HDR_SIZE + sizeof(req));
    scratch[M1_ESP32_RPC_HDR_SIZE + sizeof(req)]      = (uint8_t)(crc & 0xFFu);
    scratch[M1_ESP32_RPC_HDR_SIZE + sizeof(req) + 1u] = (uint8_t)((crc >> 8) & 0xFFu);
    m1_esp32_rpc_status_t st = m1_esp32_rpc_decode_resp(
        scratch, (uint16_t)g_last_tx_len, M1_ESP32_RPC_NOW_START, &pl, &pl_len);
    TEST_ASSERT_EQUAL_UINT8(M1_ESP32_RPC_OK, st);
    TEST_ASSERT_EQUAL_UINT16(sizeof(req), pl_len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(req, pl, sizeof(req));
}

void test_call_nak_returns_status_code(void)
{
    const uint8_t nak_body[] = { M1_ESP32_RPC_ERR_UNSUPPORTED };
    g_canned_len = make_frame(g_canned, M1_ESP32_RPC_NAK,
                              M1_ESP32_RPC_WIFI_SCAN, nak_body, sizeof(nak_body));

    uint8_t  resp[16];
    uint16_t rlen = 99;
    m1_esp32_rpc_status_t st =
        m1_esp32_rpc_call(M1_ESP32_RPC_WIFI_SCAN, NULL, 0,
                          resp, sizeof(resp), &rlen, 1);

    TEST_ASSERT_EQUAL_UINT8(M1_ESP32_RPC_ERR_UNSUPPORTED, st);
    TEST_ASSERT_EQUAL_UINT16(0, rlen);   /* no payload copied on NAK */
}

void test_call_transport_failure(void)
{
    g_ret = 1; /* fake reports a transport error */
    m1_esp32_rpc_status_t st =
        m1_esp32_rpc_call(M1_ESP32_RPC_NOW_STOP, NULL, 0, NULL, 0, NULL, 1);
    TEST_ASSERT_EQUAL_UINT8(M1_ESP32_RPC_ERR_TRANSPORT, st);
}

void test_call_msgid_mismatch_is_bad_frame(void)
{
    g_canned_len = make_frame(g_canned, M1_ESP32_RPC_RESP,
                              M1_ESP32_RPC_NOW_STOP, NULL, 0);
    /* Ask for a different opcode than the reply carries. */
    m1_esp32_rpc_status_t st =
        m1_esp32_rpc_call(M1_ESP32_RPC_NOW_START, NULL, 0, NULL, 0, NULL, 1);
    TEST_ASSERT_EQUAL_UINT8(M1_ESP32_RPC_ERR_BAD_FRAME, st);
}

void test_call_oversize_request_rejected(void)
{
    static uint8_t big[M1_ESP32_RPC_PAYLOAD_MAX + 1];
    memset(big, 0x5A, sizeof(big));
    m1_esp32_rpc_status_t st =
        m1_esp32_rpc_call(M1_ESP32_RPC_OFF_RAW_TX, big, sizeof(big),
                          NULL, 0, NULL, 1);
    TEST_ASSERT_EQUAL_UINT8(M1_ESP32_RPC_ERR_INVALID, st);
}

void test_call_truncates_payload_to_capacity(void)
{
    uint8_t body[8];
    for (unsigned i = 0; i < sizeof(body); i++) body[i] = (uint8_t)i;
    g_canned_len = make_frame(g_canned, M1_ESP32_RPC_RESP,
                              M1_ESP32_RPC_BLE_HID_STATUS, body, sizeof(body));

    uint8_t  resp[3];
    uint16_t rlen = 0;
    m1_esp32_rpc_status_t st =
        m1_esp32_rpc_call(M1_ESP32_RPC_BLE_HID_STATUS, NULL, 0,
                          resp, sizeof(resp), &rlen, 1);

    TEST_ASSERT_EQUAL_UINT8(M1_ESP32_RPC_OK, st);
    TEST_ASSERT_EQUAL_UINT16(sizeof(resp), rlen); /* clamped to capacity */
    TEST_ASSERT_EQUAL_UINT8_ARRAY(body, resp, sizeof(resp));
}

/* ================================================================== */
/* m1_esp32_rpc_call() call diagnostics (Phase 2, issue #719)         */
/* ================================================================== */

void test_diag_no_call_yet_before_any_call(void)
{
    m1_esp32_rpc_call_diag_t d;
    m1_esp32_rpc_get_call_diag(&d);
    TEST_ASSERT_EQUAL_UINT8(0u, d.attempted);

    char buf[40];
    m1_esp32_rpc_call_diag_format(&d, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("no call yet", buf);
}

void test_diag_records_ok_call_with_payload_len(void)
{
    const uint8_t body[] = {0x00, 0xAA, 0xBB, 0xCC};
    g_canned_len = make_frame(g_canned, M1_ESP32_RPC_RESP,
                              M1_ESP32_RPC_WIFI_SCAN, body, sizeof(body));

    uint8_t  resp[16];
    uint16_t rlen = 0;
    m1_esp32_rpc_status_t st =
        m1_esp32_rpc_call(M1_ESP32_RPC_WIFI_SCAN, NULL, 0,
                          resp, sizeof(resp), &rlen, 1);
    TEST_ASSERT_EQUAL_UINT8(M1_ESP32_RPC_OK, st);

    m1_esp32_rpc_call_diag_t d;
    m1_esp32_rpc_get_call_diag(&d);
    TEST_ASSERT_EQUAL_UINT8(1u, d.attempted);
    TEST_ASSERT_EQUAL_UINT16(M1_ESP32_RPC_WIFI_SCAN, d.msg_id);
    TEST_ASSERT_EQUAL_UINT8(M1_ESP32_RPC_OK, d.status);
    TEST_ASSERT_EQUAL_UINT16(sizeof(body), d.resp_len);
    TEST_ASSERT_EQUAL_INT(g_canned_len, d.rx_len);

    char buf[40];
    m1_esp32_rpc_call_diag_format(&d, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("op0103 ok st0 r14 p4", buf);
}

void test_diag_records_transport_failure_as_no_reply(void)
{
    g_ret = 1; /* fake reports a transport error, 0 bytes returned */
    g_last_elapsed_ms = 10000u; /* transport genuinely waited the full ~10s */
    (void)m1_esp32_rpc_call(M1_ESP32_RPC_WIFI_SCAN, NULL, 0, NULL, 0, NULL, 10);

    m1_esp32_rpc_call_diag_t d;
    m1_esp32_rpc_get_call_diag(&d);
    TEST_ASSERT_EQUAL_UINT8(1u, d.attempted);
    TEST_ASSERT_EQUAL_UINT8(M1_ESP32_RPC_ERR_TRANSPORT, d.status);
    TEST_ASSERT_EQUAL_INT(0, d.rx_len);
    TEST_ASSERT_EQUAL_UINT16(0u, d.resp_len);
    TEST_ASSERT_EQUAL_UINT16(10000u, d.elapsed_ms);

    char buf[40];
    m1_esp32_rpc_call_diag_format(&d, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("op0103 no-reply st253 r0 p0 t10s", buf);
}

/* Regression guard (issue #719 Phase 6): a repeat "no-reply" field report
 * after the Phase 5 timeout widening cannot by itself tell apart "the scan
 * genuinely needs longer than the budget" from "something is giving up
 * early" -- both used to render the exact same line. Confirm the elapsed
 * suffix reflects a short wait distinctly from a full one. */
void test_diag_no_reply_reports_short_elapsed_when_transport_gives_up_early(void)
{
    g_ret = 1;
    g_last_elapsed_ms = 1000u; /* gave up after ~1s despite a 10s budget */
    (void)m1_esp32_rpc_call(M1_ESP32_RPC_WIFI_SCAN, NULL, 0, NULL, 0, NULL, 10);

    m1_esp32_rpc_call_diag_t d;
    m1_esp32_rpc_get_call_diag(&d);
    TEST_ASSERT_EQUAL_UINT16(1000u, d.elapsed_ms);

    char buf[40];
    m1_esp32_rpc_call_diag_format(&d, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("op0103 no-reply st253 r0 p0 t1s", buf);
}

void test_diag_records_msgid_mismatch_as_bad_frame(void)
{
    g_canned_len = make_frame(g_canned, M1_ESP32_RPC_RESP,
                              M1_ESP32_RPC_NOW_STOP, NULL, 0);
    (void)m1_esp32_rpc_call(M1_ESP32_RPC_NOW_START, NULL, 0, NULL, 0, NULL, 1);

    m1_esp32_rpc_call_diag_t d;
    m1_esp32_rpc_get_call_diag(&d);
    TEST_ASSERT_EQUAL_UINT8(M1_ESP32_RPC_ERR_BAD_FRAME, d.status);
    TEST_ASSERT_TRUE(d.rx_len > 0);

    char buf[40];
    m1_esp32_rpc_call_diag_format(&d, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("op0600 bad-frame st254 r10 p0", buf);
}

void test_diag_records_nak_status_code(void)
{
    const uint8_t nak_body[] = { M1_ESP32_RPC_ERR_UNSUPPORTED };
    g_canned_len = make_frame(g_canned, M1_ESP32_RPC_NAK,
                              M1_ESP32_RPC_WIFI_SCAN, nak_body, sizeof(nak_body));
    (void)m1_esp32_rpc_call(M1_ESP32_RPC_WIFI_SCAN, NULL, 0, NULL, 0, NULL, 1);

    m1_esp32_rpc_call_diag_t d;
    m1_esp32_rpc_get_call_diag(&d);
    TEST_ASSERT_EQUAL_UINT8(M1_ESP32_RPC_ERR_UNSUPPORTED, d.status);
    TEST_ASSERT_EQUAL_UINT16(0u, d.resp_len);

    char buf[40];
    m1_esp32_rpc_call_diag_format(&d, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("op0103 nak st10 r11 p0", buf);
}

void test_diag_set_transport_resets_snapshot(void)
{
    const uint8_t body[] = {0x00};
    g_canned_len = make_frame(g_canned, M1_ESP32_RPC_RESP,
                              M1_ESP32_RPC_WIFI_SCAN, body, sizeof(body));
    (void)m1_esp32_rpc_call(M1_ESP32_RPC_WIFI_SCAN, NULL, 0, NULL, 0, NULL, 1);

    m1_esp32_rpc_call_diag_t d;
    m1_esp32_rpc_get_call_diag(&d);
    TEST_ASSERT_EQUAL_UINT8(1u, d.attempted);

    m1_esp32_rpc_set_transport(fake_transport); /* re-install: resets snapshot */
    m1_esp32_rpc_get_call_diag(&d);
    TEST_ASSERT_EQUAL_UINT8(0u, d.attempted);
}

void test_diag_format_null_snapshot_is_no_call(void)
{
    char buf[40];
    m1_esp32_rpc_call_diag_format(NULL, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("no call yet", buf);
}

void test_diag_format_null_buffer_is_safe(void)
{
    m1_esp32_rpc_call_diag_t d = { .attempted = 1u };
    m1_esp32_rpc_call_diag_format(&d, NULL, 0u);
    char buf[40];
    m1_esp32_rpc_call_diag_format(&d, buf, 0u);
}

/* Host-side local failures (invalid args, no-mem) are distinct from an ESP32
 * NAK: m1_esp32_rpc_call() records them with rx_len==-1 before any transport
 * call, so the dashboard correctly says "bad-arg"/"no-mem" rather than "nak"
 * (which would imply the ESP32 explicitly rejected a request that was never
 * sent). */
void test_diag_format_invalid_args_shows_bad_arg(void)
{
    m1_esp32_rpc_call_diag_t d;
    char buf[40];
    memset(&d, 0, sizeof(d));
    d.attempted = 1u;
    d.status    = (uint8_t)M1_ESP32_RPC_ERR_INVALID;
    d.rx_len    = -1;
    m1_esp32_rpc_call_diag_format(&d, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("op0000 bad-arg st2 r-1 p0", buf);
}

void test_diag_format_no_mem_shows_no_mem(void)
{
    m1_esp32_rpc_call_diag_t d;
    char buf[40];
    memset(&d, 0, sizeof(d));
    d.attempted = 1u;
    d.msg_id    = (uint16_t)M1_ESP32_RPC_WIFI_SCAN;
    d.status    = (uint8_t)M1_ESP32_RPC_ERR_NO_MEM;
    d.rx_len    = -1;
    m1_esp32_rpc_call_diag_format(&d, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("op0103 no-mem st5 r-1 p0", buf);
}

/* ================================================================== */
/* m1_esp32_rpc_decode_resp() framing errors                          */
/* ================================================================== */

void test_decode_bad_magic(void)
{
    uint8_t f[16];
    int n = make_frame(f, M1_ESP32_RPC_RESP, M1_ESP32_RPC_NOW_STOP, NULL, 0);
    f[0] ^= 0xFF; /* corrupt magic */
    const uint8_t *pl; uint16_t pl_len;
    TEST_ASSERT_EQUAL_UINT8(M1_ESP32_RPC_ERR_BAD_FRAME,
        m1_esp32_rpc_decode_resp(f, (uint16_t)n, M1_ESP32_RPC_NOW_STOP,
                                 &pl, &pl_len));
}

void test_decode_bad_crc(void)
{
    uint8_t f[16];
    int n = make_frame(f, M1_ESP32_RPC_RESP, M1_ESP32_RPC_NOW_STOP, NULL, 0);
    f[n - 1] ^= 0xFF; /* corrupt CRC high byte */
    const uint8_t *pl; uint16_t pl_len;
    TEST_ASSERT_EQUAL_UINT8(M1_ESP32_RPC_ERR_BAD_FRAME,
        m1_esp32_rpc_decode_resp(f, (uint16_t)n, M1_ESP32_RPC_NOW_STOP,
                                 &pl, &pl_len));
}

void test_decode_short_buffer(void)
{
    uint8_t f[4] = {0x31, 0x4D, 0x01, 0x02};
    const uint8_t *pl; uint16_t pl_len;
    TEST_ASSERT_EQUAL_UINT8(M1_ESP32_RPC_ERR_BAD_FRAME,
        m1_esp32_rpc_decode_resp(f, sizeof(f), M1_ESP32_RPC_NOW_STOP,
                                 &pl, &pl_len));
}

void test_decode_nak_empty_payload_is_unknown(void)
{
    uint8_t f[16];
    int n = make_frame(f, M1_ESP32_RPC_NAK, M1_ESP32_RPC_WIFI_CONNECT, NULL, 0);
    const uint8_t *pl; uint16_t pl_len;
    TEST_ASSERT_EQUAL_UINT8(M1_ESP32_RPC_ERR_UNKNOWN,
        m1_esp32_rpc_decode_resp(f, (uint16_t)n, M1_ESP32_RPC_WIFI_CONNECT,
                                 &pl, &pl_len));
}

/* ================================================================== */
/* Transport classifier                                               */
/* ================================================================== */

void test_transport_none_for_zero_bitmap(void)
{
    TEST_ASSERT_EQUAL_INT(ESP32_TRANSPORT_NONE, esp32_firmware_transport(0u));
    g_bitmap = 0u;
    TEST_ASSERT_EQUAL_INT(ESP32_TRANSPORT_NONE, m1_esp32_active_transport());
}

void test_transport_rpc_for_cd3(void)
{
    /* CD3 discriminator: HANDSHAKE + a canonical CD3-only bit (802154_TX). */
    uint64_t cd3 = M1_ESP32_CAP_HANDSHAKE | M1_ESP32_CAP_802154_TX |
                   M1_ESP32_CAP_WIFI_JOIN | M1_ESP32_CAP_BLE_HID;
    TEST_ASSERT_EQUAL_INT(ESP32_TRANSPORT_RPC, esp32_firmware_transport(cd3));
    g_bitmap = cd3;
    TEST_ASSERT_EQUAL_INT(ESP32_TRANSPORT_RPC, m1_esp32_active_transport());
}

void test_transport_binary_spi_for_sin360(void)
{
    /* SiN360 discriminator: BLE_HID present, WIFI_JOIN absent, no CD3 bits. */
    uint64_t sin = M1_ESP32_CAP_BLE_HID | M1_ESP32_CAP_BLE_SCAN;
    TEST_ASSERT_EQUAL_INT(ESP32_TRANSPORT_BINARY_SPI,
                          esp32_firmware_transport(sin));
}

void test_transport_at_for_generic_at_firmware(void)
{
    /* AT firmware: WIFI_JOIN set but not the CD3 HANDSHAKE + 802154_TX/BLE_SPAM
     * combination. */
    uint64_t at = M1_ESP32_CAP_WIFI_JOIN | M1_ESP32_CAP_DEAUTH |
                  M1_ESP32_CAP_802154;
    TEST_ASSERT_EQUAL_INT(ESP32_TRANSPORT_AT, esp32_firmware_transport(at));
}

void test_transport_at_for_legacy_cd3_at(void)
{
    /* Legacy CD3-AT: advertises WIFI_JOIN (+ BLE_HID, 802154) but NOT the
     * brain-CD3 HANDSHAKE + 802154_TX/BLE_SPAM combination, so it must stay on
     * the AT path — the compatibility layer never re-routes CD3-AT to M1_RPC. */
    uint64_t cd3_at = M1_ESP32_CAP_WIFI_JOIN | M1_ESP32_CAP_BLE_HID |
                      M1_ESP32_CAP_802154 | M1_ESP32_CAP_DEAUTH;
    TEST_ASSERT_FALSE(esp32_firmware_is_cd3(cd3_at));
    TEST_ASSERT_EQUAL_INT(ESP32_TRANSPORT_AT,
                          esp32_firmware_transport(cd3_at));
    g_bitmap = cd3_at;
    TEST_ASSERT_EQUAL_INT(ESP32_TRANSPORT_AT, m1_esp32_active_transport());
}

void test_transport_self_primes_when_not_yet_queried(void)
{
    /* Regression: before m1_esp32_caps_init() has ever run in a session (e.g.
     * the very first ESP32 feature entered is WiFi Scan / "Scan & Connect",
     * which is not capability-gated -- see scan_connect_on_enter() in
     * m1_wifi_scene_menu.c), m1_esp32_active_transport() used to read back the
     * still-zero cached bitmap and misclassify a brain-CD3 device as
     * ESP32_TRANSPORT_NONE.  wifi_do_scan() then fell through to the legacy
     * binary-SPI scan path instead of m1_esp32_rpc_wifi_scan(), so
     * m1_esp32_rpc_call() was never invoked and the Dashboard's "Last feature
     * RPC:" line read "no call yet" even after a scan was attempted. */
    uint64_t cd3 = M1_ESP32_CAP_HANDSHAKE | M1_ESP32_CAP_802154_TX;
    g_queried = false;
    g_hal_ready = true;
    g_caps_init_bitmap = cd3;
    g_caps_init_calls = 0;

    TEST_ASSERT_EQUAL_INT(ESP32_TRANSPORT_RPC, m1_esp32_active_transport());
    TEST_ASSERT_EQUAL_INT(1, g_caps_init_calls);
    TEST_ASSERT_TRUE(g_queried);

    /* A second call must not re-probe -- the bitmap is now cached. */
    TEST_ASSERT_EQUAL_INT(ESP32_TRANSPORT_RPC, m1_esp32_active_transport());
    TEST_ASSERT_EQUAL_INT(1, g_caps_init_calls);
}

void test_transport_none_and_no_probe_when_hal_not_initialised(void)
{
    /* Priming must never run (and would time out) against a transport that
     * isn't up yet -- mirrors m1_esp32_has_cap()'s own guard. */
    g_queried = false;
    g_hal_ready = false;
    g_caps_init_bitmap = M1_ESP32_CAP_HANDSHAKE | M1_ESP32_CAP_802154_TX;
    g_caps_init_calls = 0;
    g_bitmap = 0u;

    TEST_ASSERT_EQUAL_INT(ESP32_TRANSPORT_NONE, m1_esp32_active_transport());
    TEST_ASSERT_EQUAL_INT(0, g_caps_init_calls);
    TEST_ASSERT_FALSE(g_queried);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_call_resp_ok_copies_payload);
    RUN_TEST(test_call_builds_valid_request_frame);
    RUN_TEST(test_call_nak_returns_status_code);
    RUN_TEST(test_call_transport_failure);
    RUN_TEST(test_call_msgid_mismatch_is_bad_frame);
    RUN_TEST(test_call_oversize_request_rejected);
    RUN_TEST(test_call_truncates_payload_to_capacity);
    RUN_TEST(test_diag_no_call_yet_before_any_call);
    RUN_TEST(test_diag_records_ok_call_with_payload_len);
    RUN_TEST(test_diag_records_transport_failure_as_no_reply);
    RUN_TEST(test_diag_no_reply_reports_short_elapsed_when_transport_gives_up_early);
    RUN_TEST(test_diag_records_msgid_mismatch_as_bad_frame);
    RUN_TEST(test_diag_records_nak_status_code);
    RUN_TEST(test_diag_set_transport_resets_snapshot);
    RUN_TEST(test_diag_format_null_snapshot_is_no_call);
    RUN_TEST(test_diag_format_null_buffer_is_safe);
    RUN_TEST(test_diag_format_invalid_args_shows_bad_arg);
    RUN_TEST(test_diag_format_no_mem_shows_no_mem);
    RUN_TEST(test_decode_bad_magic);
    RUN_TEST(test_decode_bad_crc);
    RUN_TEST(test_decode_short_buffer);
    RUN_TEST(test_decode_nak_empty_payload_is_unknown);
    RUN_TEST(test_transport_none_for_zero_bitmap);
    RUN_TEST(test_transport_rpc_for_cd3);
    RUN_TEST(test_transport_binary_spi_for_sin360);
    RUN_TEST(test_transport_at_for_generic_at_firmware);
    RUN_TEST(test_transport_at_for_legacy_cd3_at);
    RUN_TEST(test_transport_self_primes_when_not_yet_queried);
    RUN_TEST(test_transport_none_and_no_probe_when_hal_not_initialised);
    return UNITY_END();
}
