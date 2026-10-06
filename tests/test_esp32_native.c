/* See COPYING.txt for license details. */

/*
 * test_esp32_native.c
 *
 * Host-side unit tests for the MtkCore "Native M1 SPI v1" transport codec and
 * client (m1_esp32_native.h / m1_esp32_native.c).
 *
 * Everything exercised here is pure logic or driven through an injectable
 * exchange primitive, so no HAL / SPI / hardware is involved.  The CRC32C is
 * anchored to the published CRC-32C/iSCSI check value (0xE3069283 for the ASCII
 * string "123456789"), giving an independent correctness anchor for the whole
 * frame codec.
 */

#include <string.h>
#include "unity.h"
#include "m1_esp32_native.h"

void setUp(void) {}
void tearDown(void) {}

/* ======================================================================
 * CRC-32C
 * ======================================================================*/

void test_crc32c_known_check_vector(void)
{
    /* CRC-32C (Castagnoli) check value for "123456789" is 0xE3069283. */
    const uint8_t msg[] = { '1','2','3','4','5','6','7','8','9' };
    TEST_ASSERT_EQUAL_HEX32(0xE3069283u, mtk_crc32c(msg, sizeof(msg)));
}

void test_crc32c_empty_is_zero(void)
{
    /* CRC of zero bytes: ~(0xFFFFFFFF) == 0x00000000. */
    TEST_ASSERT_EQUAL_HEX32(0x00000000u, mtk_crc32c(NULL, 0));
}

void test_crc32c_accumulate_matches_oneshot(void)
{
    const uint8_t a[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02 };
    uint32_t one_shot = mtk_crc32c(a, sizeof(a));
    uint32_t run = mtk_crc32c_accumulate(0xFFFFFFFFu, a, 3);
    run = mtk_crc32c_accumulate(run, a + 3, sizeof(a) - 3);
    TEST_ASSERT_EQUAL_HEX32(one_shot, ~run);
}

/* ======================================================================
 * Header serialize / parse — exact LE offsets
 * ======================================================================*/

void test_header_serialize_byte_offsets(void)
{
    mtk_spi_native_header_t h;
    memset(&h, 0, sizeof(h));
    h.magic           = MTK_SPI_NATIVE_MAGIC;
    h.major           = 1;
    h.minor           = 0;
    h.msg_class       = MTK_SPI_CLASS_REQUEST;
    h.flags           = MTK_SPI_FLAG_FIRST | MTK_SPI_FLAG_LAST;
    h.service         = 0x0102;
    h.opcode          = 0x0304;
    h.status          = 0x0506;
    h.payload_len     = 0x0708;
    h.request_id      = 0x11223344u;
    h.packet_seq      = 0x55667788u;
    h.boot_epoch      = 0x99AABBCCu;
    h.message_len     = 0xDDEEFF00u;
    h.fragment_offset = 0x12345678u;
    h.crc32c          = 0x9ABCDEF0u;

    uint8_t buf[MTK_SPI_NATIVE_HEADER_SIZE];
    TEST_ASSERT_TRUE(mtk_native_serialize_header(buf, &h));

    /* magic 4D 31 53 31 (LE of 0x3153314D) */
    TEST_ASSERT_EQUAL_HEX8(0x4D, buf[0]);
    TEST_ASSERT_EQUAL_HEX8(0x31, buf[1]);
    TEST_ASSERT_EQUAL_HEX8(0x53, buf[2]);
    TEST_ASSERT_EQUAL_HEX8(0x31, buf[3]);
    TEST_ASSERT_EQUAL_HEX8(1,    buf[4]);   /* major */
    TEST_ASSERT_EQUAL_HEX8(0,    buf[5]);   /* minor */
    TEST_ASSERT_EQUAL_HEX8(MTK_SPI_CLASS_REQUEST, buf[6]);
    TEST_ASSERT_EQUAL_HEX8(MTK_SPI_FLAG_FIRST | MTK_SPI_FLAG_LAST, buf[7]);
    TEST_ASSERT_EQUAL_HEX8(0x02, buf[8]);   /* service LE */
    TEST_ASSERT_EQUAL_HEX8(0x01, buf[9]);
    TEST_ASSERT_EQUAL_HEX8(0x04, buf[10]);  /* opcode LE */
    TEST_ASSERT_EQUAL_HEX8(0x03, buf[11]);
    TEST_ASSERT_EQUAL_HEX8(0x06, buf[12]);  /* status LE */
    TEST_ASSERT_EQUAL_HEX8(0x05, buf[13]);
    TEST_ASSERT_EQUAL_HEX8(0x08, buf[14]);  /* payload_len LE */
    TEST_ASSERT_EQUAL_HEX8(0x07, buf[15]);
    TEST_ASSERT_EQUAL_HEX8(0x44, buf[16]);  /* request_id LE */
    TEST_ASSERT_EQUAL_HEX8(0x33, buf[17]);
    TEST_ASSERT_EQUAL_HEX8(0x22, buf[18]);
    TEST_ASSERT_EQUAL_HEX8(0x11, buf[19]);
    TEST_ASSERT_EQUAL_HEX8(0x78, buf[32]);  /* fragment_offset LE */
    TEST_ASSERT_EQUAL_HEX8(0x56, buf[33]);
    TEST_ASSERT_EQUAL_HEX8(0x34, buf[34]);
    TEST_ASSERT_EQUAL_HEX8(0x12, buf[35]);
    TEST_ASSERT_EQUAL_HEX8(0xF0, buf[36]);  /* crc32c LE */
    TEST_ASSERT_EQUAL_HEX8(0xDE, buf[37]);
    TEST_ASSERT_EQUAL_HEX8(0xBC, buf[38]);
    TEST_ASSERT_EQUAL_HEX8(0x9A, buf[39]);
}

void test_header_round_trip(void)
{
    mtk_spi_native_header_t h, out;
    memset(&h, 0, sizeof(h));
    h.magic = MTK_SPI_NATIVE_MAGIC; h.major = 1; h.minor = 0;
    h.msg_class = MTK_SPI_CLASS_RESPONSE; h.flags = MTK_SPI_FLAG_LAST;
    h.service = MTK_SVC_WIFI; h.opcode = 0x0003; h.status = MTK_STATUS_OK;
    h.payload_len = 123; h.request_id = 0xABCDEF01u; h.packet_seq = 7;
    h.boot_epoch = 0x600DF00Du; h.message_len = 123; h.fragment_offset = 0;
    h.crc32c = 0xCAFEBABEu;

    uint8_t buf[MTK_SPI_NATIVE_HEADER_SIZE];
    mtk_native_serialize_header(buf, &h);
    memset(&out, 0, sizeof(out));
    TEST_ASSERT_TRUE(mtk_native_parse_header(buf, &out));

    TEST_ASSERT_EQUAL_HEX32(h.magic, out.magic);
    TEST_ASSERT_EQUAL_UINT8(h.msg_class, out.msg_class);
    TEST_ASSERT_EQUAL_UINT16(h.service, out.service);
    TEST_ASSERT_EQUAL_UINT16(h.opcode, out.opcode);
    TEST_ASSERT_EQUAL_UINT16(h.status, out.status);
    TEST_ASSERT_EQUAL_UINT16(h.payload_len, out.payload_len);
    TEST_ASSERT_EQUAL_HEX32(h.request_id, out.request_id);
    TEST_ASSERT_EQUAL_HEX32(h.boot_epoch, out.boot_epoch);
    TEST_ASSERT_EQUAL_HEX32(h.message_len, out.message_len);
    TEST_ASSERT_EQUAL_HEX32(h.crc32c, out.crc32c);
}

/* ======================================================================
 * Cell build / verify
 * ======================================================================*/

static mtk_spi_native_header_t make_req_hdr(uint16_t service, uint16_t opcode,
                                            uint32_t rid)
{
    mtk_spi_native_header_t h;
    memset(&h, 0, sizeof(h));
    h.magic = MTK_SPI_NATIVE_MAGIC; h.major = 1; h.minor = 0;
    h.msg_class = MTK_SPI_CLASS_REQUEST;
    h.flags = MTK_SPI_FLAG_FIRST | MTK_SPI_FLAG_LAST;
    h.service = service; h.opcode = opcode; h.request_id = rid;
    return h;
}

void test_build_cell_is_full_size_and_zero_padded(void)
{
    uint8_t cell[MTK_SPI_NATIVE_CELL_SIZE];
    const uint8_t payload[] = { 1, 2, 3, 4 };
    mtk_spi_native_header_t h = make_req_hdr(MTK_SVC_SYSTEM, MTK_SYS_OP_PING, 42);

    uint16_t n = mtk_native_build_cell(cell, sizeof(cell), &h, payload,
                                       sizeof(payload));
    TEST_ASSERT_EQUAL_UINT16(MTK_SPI_NATIVE_CELL_SIZE, n);

    /* payload at offset 40; everything after it is zero padding. */
    TEST_ASSERT_EQUAL_MEMORY(payload, cell + MTK_SPI_NATIVE_HEADER_SIZE,
                             sizeof(payload));
    for (size_t i = MTK_SPI_NATIVE_HEADER_SIZE + sizeof(payload);
         i < MTK_SPI_NATIVE_CELL_SIZE; i++)
        TEST_ASSERT_EQUAL_HEX8(0, cell[i]);
}

void test_build_then_parse_round_trip(void)
{
    uint8_t cell[MTK_SPI_NATIVE_CELL_SIZE];
    const uint8_t payload[] = { 0xAA, 0xBB, 0xCC };
    mtk_spi_native_header_t h = make_req_hdr(MTK_SVC_WIFI, 0x000A, 7);
    h.message_len = sizeof(payload);

    TEST_ASSERT_EQUAL_UINT16(MTK_SPI_NATIVE_CELL_SIZE,
        mtk_native_build_cell(cell, sizeof(cell), &h, payload, sizeof(payload)));

    mtk_spi_native_header_t ph;
    const uint8_t *pp = NULL; uint16_t pl = 0;
    TEST_ASSERT_EQUAL_INT(MTK_PARSE_OK,
        mtk_native_parse_cell(cell, sizeof(cell), &ph, &pp, &pl));
    TEST_ASSERT_EQUAL_UINT16(sizeof(payload), pl);
    TEST_ASSERT_EQUAL_MEMORY(payload, pp, sizeof(payload));
    TEST_ASSERT_EQUAL_UINT16(MTK_SVC_WIFI, ph.service);
    TEST_ASSERT_EQUAL_UINT16(0x000A, ph.opcode);
}

void test_parse_rejects_bad_magic(void)
{
    uint8_t cell[MTK_SPI_NATIVE_CELL_SIZE];
    mtk_spi_native_header_t h = make_req_hdr(MTK_SVC_SYSTEM, MTK_SYS_OP_PING, 1);
    mtk_native_build_cell(cell, sizeof(cell), &h, NULL, 0);
    cell[0] ^= 0xFF;  /* corrupt magic */
    TEST_ASSERT_EQUAL_INT(MTK_PARSE_BAD_MAGIC,
        mtk_native_parse_cell(cell, sizeof(cell), NULL, NULL, NULL));
}

void test_parse_rejects_bad_version(void)
{
    uint8_t cell[MTK_SPI_NATIVE_CELL_SIZE];
    mtk_spi_native_header_t h = make_req_hdr(MTK_SVC_SYSTEM, MTK_SYS_OP_PING, 1);
    mtk_native_build_cell(cell, sizeof(cell), &h, NULL, 0);
    cell[4] = 2;  /* major = 2 */
    TEST_ASSERT_EQUAL_INT(MTK_PARSE_BAD_VERSION,
        mtk_native_parse_cell(cell, sizeof(cell), NULL, NULL, NULL));
}

void test_parse_rejects_reserved_flags(void)
{
    uint8_t cell[MTK_SPI_NATIVE_CELL_SIZE];
    mtk_spi_native_header_t h = make_req_hdr(MTK_SVC_SYSTEM, MTK_SYS_OP_PING, 1);
    h.flags = 0x10;  /* a reserved bit (MTK_SPI_FLAG_RESERVED_MASK) */
    mtk_native_build_cell(cell, sizeof(cell), &h, NULL, 0);
    TEST_ASSERT_EQUAL_INT(MTK_PARSE_BAD_FLAGS,
        mtk_native_parse_cell(cell, sizeof(cell), NULL, NULL, NULL));
}

void test_parse_rejects_corrupted_crc(void)
{
    uint8_t cell[MTK_SPI_NATIVE_CELL_SIZE];
    const uint8_t payload[] = { 9, 9, 9 };
    mtk_spi_native_header_t h = make_req_hdr(MTK_SVC_SYSTEM, MTK_SYS_OP_PING, 1);
    mtk_native_build_cell(cell, sizeof(cell), &h, payload, sizeof(payload));
    cell[MTK_SPI_NATIVE_HEADER_SIZE] ^= 0x01;  /* flip a payload bit */
    TEST_ASSERT_EQUAL_INT(MTK_PARSE_BAD_CRC,
        mtk_native_parse_cell(cell, sizeof(cell), NULL, NULL, NULL));
}

void test_parse_rejects_short_buffer(void)
{
    uint8_t cell[16] = {0};
    TEST_ASSERT_EQUAL_INT(MTK_PARSE_SHORT,
        mtk_native_parse_cell(cell, sizeof(cell), NULL, NULL, NULL));
}

void test_build_rejects_oversize_payload(void)
{
    uint8_t cell[MTK_SPI_NATIVE_CELL_SIZE];
    static uint8_t big[MTK_SPI_NATIVE_MAX_PAYLOAD + 1];
    mtk_spi_native_header_t h = make_req_hdr(MTK_SVC_SYSTEM, MTK_SYS_OP_PING, 1);
    TEST_ASSERT_EQUAL_UINT16(0,
        mtk_native_build_cell(cell, sizeof(cell), &h, big, sizeof(big)));
}

/* ======================================================================
 * Fragment reassembly
 * ======================================================================*/

static mtk_spi_native_header_t frag_hdr(uint32_t rid, uint32_t msg_len,
                                        uint32_t offset, uint16_t plen,
                                        uint8_t flags)
{
    mtk_spi_native_header_t h;
    memset(&h, 0, sizeof(h));
    h.magic = MTK_SPI_NATIVE_MAGIC; h.major = 1; h.minor = 0;
    h.msg_class = MTK_SPI_CLASS_RESPONSE; h.flags = flags;
    h.request_id = rid; h.message_len = msg_len; h.fragment_offset = offset;
    h.payload_len = plen;
    return h;
}

void test_reasm_single_cell_complete(void)
{
    uint8_t buf[256];
    mtk_native_reasm_t r;
    mtk_native_reasm_init(&r, buf, sizeof(buf));

    const uint8_t data[] = { 1, 2, 3, 4, 5 };
    mtk_spi_native_header_t h = frag_hdr(10, sizeof(data), 0, sizeof(data),
                                         MTK_SPI_FLAG_FIRST | MTK_SPI_FLAG_LAST);
    TEST_ASSERT_EQUAL_INT(MTK_REASM_COMPLETE, mtk_native_reasm_feed(&r, &h, data));
    TEST_ASSERT_EQUAL_UINT32(sizeof(data), r.received);
    TEST_ASSERT_EQUAL_MEMORY(data, buf, sizeof(data));
}

void test_reasm_multi_cell_complete(void)
{
    uint8_t buf[256];
    mtk_native_reasm_t r;
    mtk_native_reasm_init(&r, buf, sizeof(buf));

    const uint8_t a[] = { 0x10, 0x11, 0x12 };
    const uint8_t b[] = { 0x20, 0x21 };
    const uint8_t c[] = { 0x30, 0x31, 0x32, 0x33 };
    uint32_t total = sizeof(a) + sizeof(b) + sizeof(c);

    mtk_spi_native_header_t h1 = frag_hdr(5, total, 0, sizeof(a), MTK_SPI_FLAG_FIRST);
    mtk_spi_native_header_t h2 = frag_hdr(5, total, sizeof(a), sizeof(b), 0);
    mtk_spi_native_header_t h3 = frag_hdr(5, total, sizeof(a) + sizeof(b),
                                          sizeof(c), MTK_SPI_FLAG_LAST);

    TEST_ASSERT_EQUAL_INT(MTK_REASM_NEED_MORE, mtk_native_reasm_feed(&r, &h1, a));
    TEST_ASSERT_EQUAL_INT(MTK_REASM_NEED_MORE, mtk_native_reasm_feed(&r, &h2, b));
    TEST_ASSERT_EQUAL_INT(MTK_REASM_COMPLETE, mtk_native_reasm_feed(&r, &h3, c));

    uint8_t expect[9];
    memcpy(expect, a, sizeof(a));
    memcpy(expect + sizeof(a), b, sizeof(b));
    memcpy(expect + sizeof(a) + sizeof(b), c, sizeof(c));
    TEST_ASSERT_EQUAL_UINT32(total, r.received);
    TEST_ASSERT_EQUAL_MEMORY(expect, buf, total);
}

void test_reasm_rejects_offset_gap(void)
{
    uint8_t buf[256];
    mtk_native_reasm_t r;
    mtk_native_reasm_init(&r, buf, sizeof(buf));

    const uint8_t a[] = { 1, 2, 3 };
    const uint8_t b[] = { 4, 5 };
    mtk_spi_native_header_t h1 = frag_hdr(1, 5, 0, sizeof(a), MTK_SPI_FLAG_FIRST);
    mtk_spi_native_header_t h2 = frag_hdr(1, 5, 99 /* wrong offset */, sizeof(b),
                                          MTK_SPI_FLAG_LAST);
    TEST_ASSERT_EQUAL_INT(MTK_REASM_NEED_MORE, mtk_native_reasm_feed(&r, &h1, a));
    TEST_ASSERT_EQUAL_INT(MTK_REASM_ERROR, mtk_native_reasm_feed(&r, &h2, b));
    TEST_ASSERT_EQUAL_UINT8(0, r.active);  /* reset after error */
}

void test_reasm_rejects_busy_other_request(void)
{
    uint8_t buf[256];
    mtk_native_reasm_t r;
    mtk_native_reasm_init(&r, buf, sizeof(buf));

    const uint8_t a[] = { 1, 2, 3 };
    mtk_spi_native_header_t h1 = frag_hdr(1, 6, 0, sizeof(a), MTK_SPI_FLAG_FIRST);
    /* A new FIRST for a different request while one is active -> BUSY/ERROR. */
    mtk_spi_native_header_t h2 = frag_hdr(2, 3, 0, sizeof(a), MTK_SPI_FLAG_FIRST);
    TEST_ASSERT_EQUAL_INT(MTK_REASM_NEED_MORE, mtk_native_reasm_feed(&r, &h1, a));
    TEST_ASSERT_EQUAL_INT(MTK_REASM_ERROR, mtk_native_reasm_feed(&r, &h2, a));
}

void test_reasm_rejects_message_over_ceiling(void)
{
    uint8_t buf[256];
    mtk_native_reasm_t r;
    mtk_native_reasm_init(&r, buf, sizeof(buf));
    /* message_len beyond the 8192 ceiling must be rejected at the FIRST cell. */
    mtk_spi_native_header_t h = frag_hdr(1, MTK_SPI_NATIVE_MAX_MESSAGE + 1, 0, 0,
                                         MTK_SPI_FLAG_FIRST);
    TEST_ASSERT_EQUAL_INT(MTK_REASM_ERROR, mtk_native_reasm_feed(&r, &h, NULL));
}

/* ======================================================================
 * Payload decoders
 * ======================================================================*/

void test_decode_ping_nonce(void)
{
    const uint8_t p[] = { 0x78, 0x56, 0x34, 0x12 };
    uint32_t nonce = 0;
    TEST_ASSERT_TRUE(mtk_native_decode_ping(p, sizeof(p), &nonce));
    TEST_ASSERT_EQUAL_HEX32(0x12345678u, nonce);
}

void test_decode_ping_rejects_short(void)
{
    const uint8_t p[] = { 0x01, 0x02 };
    uint32_t nonce = 0;
    TEST_ASSERT_FALSE(mtk_native_decode_ping(p, sizeof(p), &nonce));
}

void test_decode_api_identity(void)
{
    /* api_major=1, api_minor=0, variant_id=2, capability_count=113,
     * variant_name_len=8, "mtkcore\0"? use "mtkcore" (7 chars). */
    uint8_t p[32];
    size_t n = 0;
    p[n++] = 1;              /* api_major */
    p[n++] = 0;              /* api_minor */
    p[n++] = 2;              /* variant_id */
    p[n++] = 113; p[n++] = 0;/* capability_count = 113 LE */
    p[n++] = 7;   p[n++] = 0;/* variant_name_len = 7 LE */
    memcpy(p + n, "mtkcore", 7); n += 7;

    mtk_native_api_identity_t id;
    TEST_ASSERT_TRUE(mtk_native_decode_api_identity(p, (uint16_t)n, &id));
    TEST_ASSERT_EQUAL_UINT8(1, id.api_major);
    TEST_ASSERT_EQUAL_UINT8(0, id.api_minor);
    TEST_ASSERT_EQUAL_UINT8(2, id.variant_id);
    TEST_ASSERT_EQUAL_UINT16(113, id.capability_count);
    TEST_ASSERT_EQUAL_UINT16(7, id.variant_name_len);
    TEST_ASSERT_EQUAL_STRING("mtkcore", id.variant_name);
}

void test_decode_capabilities_page(void)
{
    /* One capability entry: WiFi AP_SCAN_START supported. */
    uint8_t p[4 + MTK_NATIVE_CAP_ENTRY_SIZE + 2];
    size_t n = 0;
    /* entries.count = 1 (u32 LE) */
    p[n++] = 1; p[n++] = 0; p[n++] = 0; p[n++] = 0;
    /* entry */
    p[n++] = 0x01; p[n++] = 0x00;  /* service_id = 0x0001 */
    p[n++] = 0x01; p[n++] = 0x00;  /* opcode = 0x0001 */
    p[n++] = 0x05; p[n++] = 0x00;  /* capability_id = 5 */
    p[n++] = 1;                    /* service_major */
    p[n++] = 0;                    /* service_minor */
    p[n++] = MTK_CAP_STATE_SUPPORTED;
    p[n++] = 2;                    /* max_concurrent */
    p[n++] = 0xD8; p[n++] = 0x03;  /* max_payload = 984 LE */
    p[n++] = 0; p[n++] = 0; p[n++] = 0; p[n++] = 0;  /* dep_count = 0 */
    p[n++] = 0; p[n++] = 0; p[n++] = 0; p[n++] = 0;  /* dep_items[0..1] */
    p[n++] = 0; p[n++] = 0; p[n++] = 0; p[n++] = 0;  /* dep_items[2..3] */
    /* next_index = 0 (done) */
    p[n++] = 0; p[n++] = 0;

    uint32_t count = 0; const uint8_t *entries = NULL; uint16_t next = 0xFFFF;
    TEST_ASSERT_TRUE(mtk_native_decode_capabilities_page(p, (uint16_t)n,
                                                         &count, &entries, &next));
    TEST_ASSERT_EQUAL_UINT32(1, count);
    TEST_ASSERT_EQUAL_UINT16(0, next);
    TEST_ASSERT_NOT_NULL(entries);

    mtk_native_cap_entry_t e;
    TEST_ASSERT_TRUE(mtk_native_decode_cap_entry(entries, MTK_NATIVE_CAP_ENTRY_SIZE, &e));
    TEST_ASSERT_EQUAL_UINT16(MTK_SVC_WIFI, e.service_id);
    TEST_ASSERT_EQUAL_UINT16(0x0001, e.opcode);
    TEST_ASSERT_EQUAL_UINT8(MTK_CAP_STATE_SUPPORTED, e.state);
    TEST_ASSERT_EQUAL_UINT16(984, e.max_payload);
}

void test_decode_capabilities_page_rejects_truncation(void)
{
    uint8_t p[6] = { 2, 0, 0, 0, /* count=2 but no entries */ 0, 0 };
    uint32_t count = 0; const uint8_t *entries = NULL; uint16_t next = 0;
    TEST_ASSERT_FALSE(mtk_native_decode_capabilities_page(p, sizeof(p),
                                                          &count, &entries, &next));
}

/* ======================================================================
 * Client: fake exchange primitive
 * ======================================================================*/

typedef struct {
    const uint8_t *cells;   /* flat array of queued reply cells */
    int            count;   /* number of queued cells */
    int            index;   /* next cell to return */
    int            fail_at; /* return error on this call index (-1 = never) */
    int            calls;   /* exchange call counter */
} fake_ctx_t;

/* Returns queued reply cells in order; once exhausted, returns an IDLE cell. */
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
        /* IDLE filler */
        mtk_spi_native_header_t h;
        memset(&h, 0, sizeof(h));
        h.magic = MTK_SPI_NATIVE_MAGIC; h.major = 1; h.minor = 0;
        h.msg_class = MTK_SPI_CLASS_IDLE;
        h.flags = MTK_SPI_FLAG_FIRST | MTK_SPI_FLAG_LAST;
        mtk_native_build_cell(rx, cell_size, &h, NULL, 0);
    }
    return 0;
}

/* Build a RESPONSE reply cell correlated to request id @p rid. */
static void build_response_cell(uint8_t *cell, uint32_t rid, uint16_t status,
                                const uint8_t *payload, uint16_t plen,
                                uint8_t flags)
{
    mtk_spi_native_header_t h;
    memset(&h, 0, sizeof(h));
    h.magic = MTK_SPI_NATIVE_MAGIC; h.major = 1; h.minor = 0;
    h.msg_class = MTK_SPI_CLASS_RESPONSE; h.flags = flags;
    h.status = status; h.request_id = rid;
    h.message_len = plen; h.fragment_offset = 0;
    mtk_native_build_cell(cell, MTK_SPI_NATIVE_CELL_SIZE, &h, payload, plen);
}

void test_call_single_cell_ok(void)
{
    mtk_native_reset_request_id(100);  /* next id = 101 */
    uint8_t reply[MTK_SPI_NATIVE_CELL_SIZE];
    const uint8_t rpayload[] = { 0xDE, 0xAD, 0xBE, 0xEF };
    build_response_cell(reply, 101, MTK_STATUS_OK, rpayload, sizeof(rpayload),
                        MTK_SPI_FLAG_FIRST | MTK_SPI_FLAG_LAST);

    fake_ctx_t f = { reply, 1, 0, -1, 0 };
    uint8_t out[32]; size_t out_len = 0; uint16_t status = 0xFFFF;
    mtk_native_result_t r = mtk_native_call(fake_xfer, &f, MTK_SVC_SYSTEM,
                                            MTK_SYS_OP_GET_API_IDENTITY, NULL, 0,
                                            out, sizeof(out), &out_len, &status,
                                            0x1234u, 4);
    TEST_ASSERT_EQUAL_INT(MTK_NATIVE_OK, r);
    TEST_ASSERT_EQUAL_UINT16(MTK_STATUS_OK, status);
    TEST_ASSERT_EQUAL_size_t(sizeof(rpayload), out_len);
    TEST_ASSERT_EQUAL_MEMORY(rpayload, out, sizeof(rpayload));
}

void test_call_multi_cell_response_reassembled(void)
{
    mtk_native_reset_request_id(0);  /* next id = 1 */
    uint8_t cells[2 * MTK_SPI_NATIVE_CELL_SIZE];
    const uint8_t part1[] = { 1, 2, 3 };
    const uint8_t part2[] = { 4, 5 };
    uint16_t total = sizeof(part1) + sizeof(part2);

    /* FIRST cell */
    mtk_spi_native_header_t h1;
    memset(&h1, 0, sizeof(h1));
    h1.magic = MTK_SPI_NATIVE_MAGIC; h1.major = 1; h1.minor = 0;
    h1.msg_class = MTK_SPI_CLASS_RESPONSE; h1.flags = MTK_SPI_FLAG_FIRST;
    h1.status = MTK_STATUS_OK; h1.request_id = 1;
    h1.message_len = total; h1.fragment_offset = 0;
    mtk_native_build_cell(cells, MTK_SPI_NATIVE_CELL_SIZE, &h1, part1, sizeof(part1));
    /* LAST cell */
    mtk_spi_native_header_t h2 = h1;
    h2.flags = MTK_SPI_FLAG_LAST; h2.fragment_offset = sizeof(part1);
    mtk_native_build_cell(cells + MTK_SPI_NATIVE_CELL_SIZE,
                          MTK_SPI_NATIVE_CELL_SIZE, &h2, part2, sizeof(part2));

    fake_ctx_t f = { cells, 2, 0, -1, 0 };
    uint8_t out[16]; size_t out_len = 0; uint16_t status = 0xFFFF;
    mtk_native_result_t r = mtk_native_call(fake_xfer, &f, MTK_SVC_WIFI,
                                            0x0003, NULL, 0, out, sizeof(out),
                                            &out_len, &status, 0, 6);
    TEST_ASSERT_EQUAL_INT(MTK_NATIVE_OK, r);
    TEST_ASSERT_EQUAL_size_t(total, out_len);
    uint8_t expect[5] = { 1, 2, 3, 4, 5 };
    TEST_ASSERT_EQUAL_MEMORY(expect, out, total);
}

void test_call_non_ok_status_reports_err_status(void)
{
    mtk_native_reset_request_id(0);  /* next id = 1 */
    uint8_t reply[MTK_SPI_NATIVE_CELL_SIZE];
    build_response_cell(reply, 1, MTK_STATUS_UNSUPPORTED, NULL, 0,
                        MTK_SPI_FLAG_FIRST | MTK_SPI_FLAG_LAST);

    fake_ctx_t f = { reply, 1, 0, -1, 0 };
    uint16_t status = 0;
    mtk_native_result_t r = mtk_native_call(fake_xfer, &f, MTK_SVC_ESPNOW,
                                            0x0001, NULL, 0, NULL, 0, NULL,
                                            &status, 0, 4);
    TEST_ASSERT_EQUAL_INT(MTK_NATIVE_ERR_STATUS, r);
    TEST_ASSERT_EQUAL_UINT16(MTK_STATUS_UNSUPPORTED, status);
}

void test_call_timeout_when_no_reply(void)
{
    mtk_native_reset_request_id(0);
    fake_ctx_t f = { NULL, 0, 0, -1, 0 };  /* always IDLE */
    mtk_native_result_t r = mtk_native_call(fake_xfer, &f, MTK_SVC_SYSTEM,
                                            MTK_SYS_OP_PING, NULL, 0, NULL, 0,
                                            NULL, NULL, 0, 3);
    TEST_ASSERT_EQUAL_INT(MTK_NATIVE_ERR_TIMEOUT, r);
}

void test_call_transport_error(void)
{
    mtk_native_reset_request_id(0);
    fake_ctx_t f = { NULL, 0, 0, 0 /* fail on first call */, 0 };
    mtk_native_result_t r = mtk_native_call(fake_xfer, &f, MTK_SVC_SYSTEM,
                                            MTK_SYS_OP_PING, NULL, 0, NULL, 0,
                                            NULL, NULL, 0, 3);
    TEST_ASSERT_EQUAL_INT(MTK_NATIVE_ERR_TRANSPORT, r);
}

void test_call_ignores_uncorrelated_then_matches(void)
{
    mtk_native_reset_request_id(0);  /* our id = 1 */
    uint8_t cells[2 * MTK_SPI_NATIVE_CELL_SIZE];
    /* First queued cell: RESPONSE for a DIFFERENT request id (must be skipped). */
    build_response_cell(cells, 999, MTK_STATUS_OK, NULL, 0,
                        MTK_SPI_FLAG_FIRST | MTK_SPI_FLAG_LAST);
    /* Second queued cell: the real correlated response. */
    const uint8_t rp[] = { 0x5A };
    build_response_cell(cells + MTK_SPI_NATIVE_CELL_SIZE, 1, MTK_STATUS_OK,
                        rp, sizeof(rp), MTK_SPI_FLAG_FIRST | MTK_SPI_FLAG_LAST);

    fake_ctx_t f = { cells, 2, 0, -1, 0 };
    uint8_t out[8]; size_t out_len = 0;
    mtk_native_result_t r = mtk_native_call(fake_xfer, &f, MTK_SVC_SYSTEM,
                                            MTK_SYS_OP_PING, NULL, 0, out,
                                            sizeof(out), &out_len, NULL, 0, 5);
    TEST_ASSERT_EQUAL_INT(MTK_NATIVE_OK, r);
    TEST_ASSERT_EQUAL_size_t(1, out_len);
    TEST_ASSERT_EQUAL_HEX8(0x5A, out[0]);
}

void test_hello_handshake_ok(void)
{
    mtk_native_reset_request_id(0);  /* HELLO id = 1 */
    uint8_t ack[MTK_SPI_NATIVE_CELL_SIZE];
    mtk_spi_native_header_t h;
    memset(&h, 0, sizeof(h));
    h.magic = MTK_SPI_NATIVE_MAGIC; h.major = 1; h.minor = 0;
    h.msg_class = MTK_SPI_CLASS_HELLO_ACK;
    h.flags = MTK_SPI_FLAG_FIRST | MTK_SPI_FLAG_LAST;
    h.request_id = 1; h.boot_epoch = 0x600Du;
    mtk_native_build_cell(ack, sizeof(ack), &h, NULL, 0);

    fake_ctx_t f = { ack, 1, 0, -1, 0 };
    mtk_native_result_t r = mtk_native_hello(fake_xfer, &f, 0xABCDu, 4);
    TEST_ASSERT_EQUAL_INT(MTK_NATIVE_OK, r);
}

void test_hello_handshake_timeout(void)
{
    mtk_native_reset_request_id(0);
    fake_ctx_t f = { NULL, 0, 0, -1, 0 };  /* always IDLE */
    mtk_native_result_t r = mtk_native_hello(fake_xfer, &f, 0xABCDu, 3);
    TEST_ASSERT_EQUAL_INT(MTK_NATIVE_ERR_TIMEOUT, r);
}

/* ======================================================================
 * Runner
 * ======================================================================*/

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_crc32c_known_check_vector);
    RUN_TEST(test_crc32c_empty_is_zero);
    RUN_TEST(test_crc32c_accumulate_matches_oneshot);

    RUN_TEST(test_header_serialize_byte_offsets);
    RUN_TEST(test_header_round_trip);

    RUN_TEST(test_build_cell_is_full_size_and_zero_padded);
    RUN_TEST(test_build_then_parse_round_trip);
    RUN_TEST(test_parse_rejects_bad_magic);
    RUN_TEST(test_parse_rejects_bad_version);
    RUN_TEST(test_parse_rejects_reserved_flags);
    RUN_TEST(test_parse_rejects_corrupted_crc);
    RUN_TEST(test_parse_rejects_short_buffer);
    RUN_TEST(test_build_rejects_oversize_payload);

    RUN_TEST(test_reasm_single_cell_complete);
    RUN_TEST(test_reasm_multi_cell_complete);
    RUN_TEST(test_reasm_rejects_offset_gap);
    RUN_TEST(test_reasm_rejects_busy_other_request);
    RUN_TEST(test_reasm_rejects_message_over_ceiling);

    RUN_TEST(test_decode_ping_nonce);
    RUN_TEST(test_decode_ping_rejects_short);
    RUN_TEST(test_decode_api_identity);
    RUN_TEST(test_decode_capabilities_page);
    RUN_TEST(test_decode_capabilities_page_rejects_truncation);

    RUN_TEST(test_call_single_cell_ok);
    RUN_TEST(test_call_multi_cell_response_reassembled);
    RUN_TEST(test_call_non_ok_status_reports_err_status);
    RUN_TEST(test_call_timeout_when_no_reply);
    RUN_TEST(test_call_transport_error);
    RUN_TEST(test_call_ignores_uncorrelated_then_matches);
    RUN_TEST(test_hello_handshake_ok);
    RUN_TEST(test_hello_handshake_timeout);

    return UNITY_END();
}
