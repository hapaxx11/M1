/* See COPYING.txt for license details. */

/*
 * test_esp32_capture.c
 *
 * Host-side unit tests for the MtkCore Native M1 SPI v1 CAPTURE service codec
 * + client driver (m1_csrc/m1_esp32_capture.c).  The 1024-byte SPI exchange
 * primitive is injected as a fake canned-cell queue (same harness as
 * test_esp32_native.c), so START / POLL_READ / STOP run entirely on the host.
 */

#include <string.h>
#include "unity.h"
#include "m1_esp32_capture.h"
#include "wifi_pcapng.h"

void setUp(void) {}
void tearDown(void) {}

/* ---- little-endian readback helpers ------------------------------------ */
static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* ======================================================================
 * Request body builders
 * ======================================================================*/

void test_build_start_req_layout(void)
{
    mtk_capture_cfg_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.mode         = MTK_CAP_MODE_POLL;         /* 1 */
    cfg.snap_len     = 256u;
    cfg.duration_ms  = 0x11223344u;
    cfg.chan_mode    = MTK_CAP_CHANPLAN_HOP;      /* 1 */
    cfg.channel      = 6u;
    cfg.band         = MTK_CAP_BAND_2GHZ;         /* 0 */
    cfg.hop_dwell_ms = 300u;
    const uint8_t bssid[6] = { 0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02 };
    memcpy(cfg.filter_bssid, bssid, 6);

    uint8_t req[MTK_CAP_START_REQ_LEN];
    size_t n = mtk_capture_build_start_req(req, sizeof(req), &cfg);
    TEST_ASSERT_EQUAL_size_t(MTK_CAP_START_REQ_LEN, n);
    TEST_ASSERT_EQUAL_size_t(18u, n);

    TEST_ASSERT_EQUAL_UINT8(1u, req[0]);               /* mode */
    TEST_ASSERT_EQUAL_UINT16(256u, rd16(req + 1));     /* snap_len */
    TEST_ASSERT_EQUAL_UINT32(0x11223344u, rd32(req + 3)); /* duration_ms */
    TEST_ASSERT_EQUAL_UINT8(1u, req[7]);               /* chan_mode */
    TEST_ASSERT_EQUAL_UINT8(6u, req[8]);               /* channel */
    TEST_ASSERT_EQUAL_UINT8(0u, req[9]);               /* band */
    TEST_ASSERT_EQUAL_UINT16(300u, rd16(req + 10));    /* hop_dwell_ms */
    TEST_ASSERT_EQUAL_MEMORY(bssid, req + 12, 6);      /* filter bssid */
}

void test_build_start_req_rejects_bad_args(void)
{
    mtk_capture_cfg_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    uint8_t req[MTK_CAP_START_REQ_LEN];
    TEST_ASSERT_EQUAL_size_t(0u, mtk_capture_build_start_req(NULL, sizeof(req), &cfg));
    TEST_ASSERT_EQUAL_size_t(0u, mtk_capture_build_start_req(req, sizeof(req), NULL));
    TEST_ASSERT_EQUAL_size_t(0u, mtk_capture_build_start_req(req, 17u, &cfg));
}

void test_build_poll_read_req(void)
{
    uint8_t req[MTK_CAP_POLL_READ_REQ_LEN];
    size_t n = mtk_capture_build_poll_read_req(req, sizeof(req), 0xCAFEBABEu);
    TEST_ASSERT_EQUAL_size_t(4u, n);
    TEST_ASSERT_EQUAL_UINT32(0xCAFEBABEu, rd32(req));
    TEST_ASSERT_EQUAL_size_t(0u, mtk_capture_build_poll_read_req(req, 3u, 1u));
}

void test_build_stop_req(void)
{
    uint8_t req[MTK_CAP_STOP_REQ_LEN];
    size_t n = mtk_capture_build_stop_req(req, sizeof(req), 0x01020304u, 7u);
    TEST_ASSERT_EQUAL_size_t(5u, n);
    TEST_ASSERT_EQUAL_UINT32(0x01020304u, rd32(req));
    TEST_ASSERT_EQUAL_UINT8(7u, req[4]);
    TEST_ASSERT_EQUAL_size_t(0u, mtk_capture_build_stop_req(req, 4u, 1u, 0u));
}

/* ======================================================================
 * Response parsers
 * ======================================================================*/

void test_parse_start_resp(void)
{
    const uint8_t body[4] = { 0x78, 0x56, 0x34, 0x12 };
    uint32_t token = 0;
    TEST_ASSERT_TRUE(mtk_capture_parse_start_resp(body, sizeof(body), &token));
    TEST_ASSERT_EQUAL_UINT32(0x12345678u, token);
    TEST_ASSERT_FALSE(mtk_capture_parse_start_resp(body, 3u, &token));
    TEST_ASSERT_FALSE(mtk_capture_parse_start_resp(NULL, 4u, &token));
}

/* Build a 20-byte record header + frame into @p buf; returns total length. */
static size_t build_record(uint8_t *buf, uint32_t seq, uint64_t ts,
                           uint8_t ch, int8_t rssi, uint8_t flags,
                           uint16_t origlen, const uint8_t *frame, uint16_t caplen)
{
    buf[0] = (uint8_t)seq; buf[1] = (uint8_t)(seq >> 8);
    buf[2] = (uint8_t)(seq >> 16); buf[3] = (uint8_t)(seq >> 24);
    for (int i = 0; i < 8; i++) buf[4 + i] = (uint8_t)(ts >> (8 * i));
    buf[12] = MTK_CAP_LINKTYPE_IEEE80211;
    buf[13] = ch;
    buf[14] = (uint8_t)rssi;
    buf[15] = flags;
    buf[16] = (uint8_t)origlen; buf[17] = (uint8_t)(origlen >> 8);
    buf[18] = (uint8_t)caplen;  buf[19] = (uint8_t)(caplen >> 8);
    if (caplen) memcpy(buf + 20, frame, caplen);
    return (size_t)20u + caplen;
}

void test_parse_poll_empty(void)
{
    mtk_capture_record_t rec;
    const uint8_t *frame = (const uint8_t *)1;
    uint16_t flen = 99;
    TEST_ASSERT_EQUAL_INT(MTK_CAP_POLL_EMPTY,
        mtk_capture_parse_poll_record(NULL, 0u, &rec, &frame, &flen));
    TEST_ASSERT_NULL(frame);
    TEST_ASSERT_EQUAL_UINT16(0u, flen);
}

void test_parse_poll_record(void)
{
    const uint8_t frame[4] = { 0x80, 0x00, 0xAB, 0xCD };
    uint8_t body[20 + 4];
    size_t n = build_record(body, 0x01020304u, 0x1122334455667788ull,
                            11u, -63, MTK_CAP_FLAG_TRUNCATED, 1500u, frame, 4u);

    mtk_capture_record_t rec;
    const uint8_t *fp = NULL;
    uint16_t flen = 0;
    TEST_ASSERT_EQUAL_INT(MTK_CAP_POLL_RECORD,
        mtk_capture_parse_poll_record(body, n, &rec, &fp, &flen));
    TEST_ASSERT_EQUAL_UINT32(0x01020304u, rec.sequence);
    TEST_ASSERT_EQUAL_UINT64(0x1122334455667788ull, rec.timestamp_us);
    TEST_ASSERT_EQUAL_UINT8(MTK_CAP_LINKTYPE_IEEE80211, rec.link_type);
    TEST_ASSERT_EQUAL_UINT8(11u, rec.channel);
    TEST_ASSERT_EQUAL_INT8(-63, rec.rssi);
    TEST_ASSERT_EQUAL_UINT8(MTK_CAP_FLAG_TRUNCATED, rec.flags);
    TEST_ASSERT_EQUAL_UINT16(1500u, rec.original_len);
    TEST_ASSERT_EQUAL_UINT16(4u, rec.captured_len);
    TEST_ASSERT_EQUAL_UINT16(4u, flen);
    TEST_ASSERT_EQUAL_MEMORY(frame, fp, 4);
}

void test_parse_poll_malformed(void)
{
    uint8_t body[19] = {0};  /* shorter than the 20-byte header */
    TEST_ASSERT_EQUAL_INT(MTK_CAP_POLL_MALFORMED,
        mtk_capture_parse_poll_record(body, sizeof(body), NULL, NULL, NULL));

    /* header claims captured_len=10 but only 2 frame bytes present */
    uint8_t body2[20 + 2];
    memset(body2, 0, sizeof(body2));
    body2[18] = 10u; body2[19] = 0u;   /* captured_len = 10 */
    TEST_ASSERT_EQUAL_INT(MTK_CAP_POLL_MALFORMED,
        mtk_capture_parse_poll_record(body2, sizeof(body2), NULL, NULL, NULL));
}

/* ======================================================================
 * Capability gate
 * ======================================================================*/

/* Build a one-entry GET_CAPABILITIES page advertising (svc,op) at @p state. */
static size_t build_caps_page(uint8_t *p, uint16_t svc, uint16_t op, uint8_t state)
{
    size_t n = 0;
    p[n++] = 1; p[n++] = 0; p[n++] = 0; p[n++] = 0;      /* count = 1 */
    p[n++] = (uint8_t)svc; p[n++] = (uint8_t)(svc >> 8); /* service_id */
    p[n++] = (uint8_t)op;  p[n++] = (uint8_t)(op >> 8);  /* opcode */
    p[n++] = 0x05; p[n++] = 0x00;                        /* capability_id */
    p[n++] = 1;                                          /* service_major */
    p[n++] = 0;                                          /* service_minor */
    p[n++] = state;                                      /* state */
    p[n++] = 1;                                          /* max_concurrent */
    p[n++] = 0xD8; p[n++] = 0x03;                        /* max_payload 984 */
    for (int i = 0; i < 12; i++) p[n++] = 0;            /* dep_count + items */
    p[n++] = 0; p[n++] = 0;                              /* next_index = 0 */
    return n;
}

void test_cap_gate_supported(void)
{
    uint8_t p[64];
    size_t n = build_caps_page(p, MTK_SVC_CAPTURE, MTK_CAP_OP_START,
                               MTK_CAP_STATE_SUPPORTED);
    TEST_ASSERT_TRUE(mtk_capture_caps_page_supported(p, (uint16_t)n));
}

void test_cap_gate_rejects_wrong_service_or_state(void)
{
    uint8_t p[64];
    size_t n;

    n = build_caps_page(p, MTK_SVC_WIFI, MTK_CAP_OP_START, MTK_CAP_STATE_SUPPORTED);
    TEST_ASSERT_FALSE(mtk_capture_caps_page_supported(p, (uint16_t)n));

    n = build_caps_page(p, MTK_SVC_CAPTURE, MTK_CAP_OP_STOP, MTK_CAP_STATE_SUPPORTED);
    TEST_ASSERT_FALSE(mtk_capture_caps_page_supported(p, (uint16_t)n));

    n = build_caps_page(p, MTK_SVC_CAPTURE, MTK_CAP_OP_START, MTK_CAP_STATE_DISABLED);
    TEST_ASSERT_FALSE(mtk_capture_caps_page_supported(p, (uint16_t)n));
}

/* ======================================================================
 * PCAPNG glue
 * ======================================================================*/

void test_record_to_epb(void)
{
    const uint8_t frame[3] = { 0x48, 0x00, 0x11 };
    mtk_capture_record_t rec;
    memset(&rec, 0, sizeof(rec));
    rec.channel      = 6u;
    rec.rssi         = -70;
    rec.timestamp_us = 123456789ull;
    rec.original_len = 3u;
    rec.captured_len = 3u;

    uint8_t epb[64];
    size_t n = mtk_capture_record_to_epb(epb, sizeof(epb), &rec, frame);
    /* caplen = 15 + 3 = 18, pad = 2, total = 32 + 18 + 2 = 52 */
    TEST_ASSERT_EQUAL_size_t(52u, n);
    TEST_ASSERT_EQUAL_UINT32(WIFI_PCAPNG_BT_EPB, rd32(epb));
    TEST_ASSERT_EQUAL_UINT32(18u, rd32(epb + 20));     /* captured len */
    TEST_ASSERT_EQUAL_MEMORY(frame, epb + 28 + WIFI_RADIOTAP_LEN, 3);
    TEST_ASSERT_EQUAL_size_t(0u, mtk_capture_record_to_epb(NULL, 0u, &rec, frame));
}

/* ======================================================================
 * Client driver: fake exchange primitive (mirrors test_esp32_native.c)
 * ======================================================================*/

typedef struct {
    const uint8_t *cells;
    int            count;
    int            index;
    int            fail_at;
    int            calls;
} fake_ctx_t;

static int fake_xfer(const uint8_t *tx, uint8_t *rx, uint16_t cell_size, void *ctx)
{
    fake_ctx_t *f = (fake_ctx_t *)ctx;
    (void)tx;
    if (f->fail_at == f->calls) { f->calls++; return 1; }
    f->calls++;

    if (f->index < f->count) {
        memcpy(rx, f->cells + (size_t)f->index * MTK_SPI_NATIVE_CELL_SIZE,
               cell_size);
        f->index++;
    } else {
        mtk_spi_native_header_t h;
        memset(&h, 0, sizeof(h));
        h.magic = MTK_SPI_NATIVE_MAGIC; h.major = 1; h.minor = 0;
        h.msg_class = MTK_SPI_CLASS_IDLE;
        h.flags = MTK_SPI_FLAG_FIRST | MTK_SPI_FLAG_LAST;
        mtk_native_build_cell(rx, cell_size, &h, NULL, 0);
    }
    return 0;
}

static void build_response_cell(uint8_t *cell, uint32_t rid, uint16_t status,
                                const uint8_t *payload, uint16_t plen)
{
    mtk_spi_native_header_t h;
    memset(&h, 0, sizeof(h));
    h.magic = MTK_SPI_NATIVE_MAGIC; h.major = 1; h.minor = 0;
    h.msg_class = MTK_SPI_CLASS_RESPONSE;
    h.flags = MTK_SPI_FLAG_FIRST | MTK_SPI_FLAG_LAST;
    h.status = status; h.request_id = rid;
    h.message_len = plen; h.fragment_offset = 0;
    mtk_native_build_cell(cell, MTK_SPI_NATIVE_CELL_SIZE, &h, payload, plen);
}

void test_driver_start_accepts_token(void)
{
    mtk_native_reset_request_id(0);  /* next id = 1 */
    uint8_t reply[MTK_SPI_NATIVE_CELL_SIZE];
    const uint8_t token_le[4] = { 0x0D, 0xF0, 0xAD, 0x0B }; /* 0x0BADF00D */
    build_response_cell(reply, 1, MTK_STATUS_ACCEPTED, token_le, 4);

    fake_ctx_t f = { reply, 1, 0, -1, 0 };
    mtk_capture_cfg_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.mode = MTK_CAP_MODE_POLL; cfg.snap_len = 512u; cfg.channel = 6u;

    uint32_t token = 0; uint16_t status = 0xFFFF;
    mtk_native_result_t r = mtk_capture_start(fake_xfer, &f, &cfg, &token,
                                              &status, 0x1234u, 4);
    TEST_ASSERT_EQUAL_INT(MTK_NATIVE_OK, r);
    TEST_ASSERT_EQUAL_UINT16(MTK_STATUS_ACCEPTED, status);
    TEST_ASSERT_EQUAL_UINT32(0x0BADF00Du, token);
}

void test_driver_start_rejects_zero_token(void)
{
    mtk_native_reset_request_id(0);
    uint8_t reply[MTK_SPI_NATIVE_CELL_SIZE];
    const uint8_t token_le[4] = { 0, 0, 0, 0 };
    build_response_cell(reply, 1, MTK_STATUS_ACCEPTED, token_le, 4);

    fake_ctx_t f = { reply, 1, 0, -1, 0 };
    mtk_capture_cfg_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    uint32_t token = 0xDEADBEEFu;
    mtk_native_result_t r = mtk_capture_start(fake_xfer, &f, &cfg, &token,
                                              NULL, 0, 4);
    TEST_ASSERT_EQUAL_INT(MTK_NATIVE_ERR_PROTOCOL, r);
    TEST_ASSERT_EQUAL_UINT32(0u, token);
}

void test_driver_start_rejects_null_args(void)
{
    mtk_capture_cfg_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    uint32_t token = 0;
    TEST_ASSERT_EQUAL_INT(MTK_NATIVE_ERR_INVALID,
        mtk_capture_start(NULL, NULL, &cfg, &token, NULL, 0, 4));
    TEST_ASSERT_EQUAL_INT(MTK_NATIVE_ERR_INVALID,
        mtk_capture_start(fake_xfer, NULL, NULL, &token, NULL, 0, 4));
}

void test_driver_poll_returns_frame(void)
{
    mtk_native_reset_request_id(0);
    const uint8_t frame[5] = { 0x80, 0x00, 0x01, 0x02, 0x03 };
    uint8_t body[20 + 5];
    size_t blen = build_record(body, 7u, 999ull, 6u, -50, 0u, 5u, frame, 5u);

    uint8_t reply[MTK_SPI_NATIVE_CELL_SIZE];
    build_response_cell(reply, 1, MTK_STATUS_OK, body, (uint16_t)blen);

    fake_ctx_t f = { reply, 1, 0, -1, 0 };
    mtk_capture_record_t rec;
    uint8_t fbuf[32]; uint16_t flen = 0; bool have = false; uint16_t status = 0;
    mtk_native_result_t r = mtk_capture_poll(fake_xfer, &f, 1u, &rec,
                                             fbuf, sizeof(fbuf), &flen, &have,
                                             &status, 0, 4);
    TEST_ASSERT_EQUAL_INT(MTK_NATIVE_OK, r);
    TEST_ASSERT_TRUE(have);
    TEST_ASSERT_EQUAL_UINT16(5u, flen);
    TEST_ASSERT_EQUAL_MEMORY(frame, fbuf, 5);
    TEST_ASSERT_EQUAL_UINT8(6u, rec.channel);
    TEST_ASSERT_EQUAL_INT8(-50, rec.rssi);
}

void test_driver_poll_empty_is_success(void)
{
    mtk_native_reset_request_id(0);
    uint8_t reply[MTK_SPI_NATIVE_CELL_SIZE];
    build_response_cell(reply, 1, MTK_STATUS_OK, NULL, 0);

    fake_ctx_t f = { reply, 1, 0, -1, 0 };
    mtk_capture_record_t rec;
    uint8_t fbuf[32]; uint16_t flen = 7; bool have = true; uint16_t status = 0;
    mtk_native_result_t r = mtk_capture_poll(fake_xfer, &f, 1u, &rec,
                                             fbuf, sizeof(fbuf), &flen, &have,
                                             &status, 0, 4);
    TEST_ASSERT_EQUAL_INT(MTK_NATIVE_OK, r);
    TEST_ASSERT_FALSE(have);
    TEST_ASSERT_EQUAL_UINT16(0u, flen);
}

void test_driver_stop_ok(void)
{
    mtk_native_reset_request_id(0);
    uint8_t reply[MTK_SPI_NATIVE_CELL_SIZE];
    const uint8_t body[3] = { 0, 0, 0 };  /* final_state/status/reason */
    build_response_cell(reply, 1, MTK_STATUS_OK, body, sizeof(body));

    fake_ctx_t f = { reply, 1, 0, -1, 0 };
    uint16_t status = 0xFFFF;
    mtk_native_result_t r = mtk_capture_stop(fake_xfer, &f, 1u, 0u,
                                             &status, 0, 4);
    TEST_ASSERT_EQUAL_INT(MTK_NATIVE_OK, r);
    TEST_ASSERT_EQUAL_UINT16(MTK_STATUS_OK, status);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_build_start_req_layout);
    RUN_TEST(test_build_start_req_rejects_bad_args);
    RUN_TEST(test_build_poll_read_req);
    RUN_TEST(test_build_stop_req);

    RUN_TEST(test_parse_start_resp);
    RUN_TEST(test_parse_poll_empty);
    RUN_TEST(test_parse_poll_record);
    RUN_TEST(test_parse_poll_malformed);

    RUN_TEST(test_cap_gate_supported);
    RUN_TEST(test_cap_gate_rejects_wrong_service_or_state);

    RUN_TEST(test_record_to_epb);

    RUN_TEST(test_driver_start_accepts_token);
    RUN_TEST(test_driver_start_rejects_zero_token);
    RUN_TEST(test_driver_start_rejects_null_args);
    RUN_TEST(test_driver_poll_returns_frame);
    RUN_TEST(test_driver_poll_empty_is_success);
    RUN_TEST(test_driver_stop_ok);
    return UNITY_END();
}
