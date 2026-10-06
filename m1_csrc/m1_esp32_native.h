/* See COPYING.txt for license details. */

/*
 * m1_esp32_native.h
 *
 * MtkCore "Native M1 SPI v1" transport — codec / framing layer (host side).
 *
 * Background
 * ----------
 * Monstatek's MtkCore ESP32 firmware (Monstatek/MonstaTek-Esp32-Core) ships
 * THREE SPI transport adapters: Factory UART, a "Legacy SPI Compatibility"
 * adapter that speaks our existing m1_link binary RPC (magic 0x4D31 — handled by
 * the Tier-A MtkCore detection in m1_esp32_caps.c / m1_esp32_rpc.c), and the
 * canonical "Native M1 SPI v1" transport implemented here.
 *
 * The native transport is a different, richer wire protocol:
 *   - 1024-byte full-duplex cells (vs. the legacy 512-byte m1_link cell),
 *   - a 40-byte little-endian header carrying an explicit (service, opcode)
 *     address, message class, flags, a 32-bit request_id for correlation,
 *     fragmentation fields (packet_seq / message_len / fragment_offset), and a
 *     CRC32C (Castagnoli) over header[0..35] + payload,
 *   - a HELLO / HELLO_ACK session handshake and a CREDIT flow-control class,
 *   - capability negotiation via a *paginated* GET_CAPABILITIES opcode (there is
 *     NO capability bitmap on the native transport — the legacy cap_bitmap is a
 *     compat-adapter-only concept).
 *
 * This header is the pure-logic, host-testable codec for that wire format: it
 * has no HAL / RTOS / SPI dependencies, so every helper is exercised directly by
 * tests/test_esp32_native.c.  The runtime client (HELLO handshake + request /
 * response reassembly over a transport function pointer) lives in
 * m1_esp32_native.c.
 *
 * Authority
 * ---------
 * Every constant, offset and struct below is mirrored verbatim from the MtkCore
 * firmware source (Monstatek/MonstaTek-Esp32-Core @ main, commit 3e21a6a).  The
 * firmware's own headers state the C source "is authoritative for byte-level
 * framing" (docs/SPI_PROTOCOL_V1.md).  Citations are given per-item as
 * "<component path>:<lines>".
 *
 *   Framing constants / header struct / classes / flags:
 *     components/mtek_transport_spi_native/include/mtek_spi_native_frame.h:11-55
 *   On-wire byte layout (serialize_header, LE, no struct cast):
 *     components/mtek_transport_spi_native/mtek_spi_native_frame.c:35-67
 *   CRC32C implementation + coverage:
 *     components/mtek_transport_spi_native/mtek_spi_native_frame.c:8-18, 60-106
 *   Service IDs / opcodes / status codes:
 *     components/mtek_schema/generated/mtek_opcode_registry.c:8-120,
 *     components/mtek_schema/include/mtek_schema_constants.h:4-52
 *
 * NOT implemented here (explicitly disclosed GAPS in the firmware contract — see
 * the module comment in m1_esp32_native.c): the HELLO negotiation payload
 * (HELLO_ACK is a bare, empty acknowledgement), the physical 512->1024 cell-size
 * negotiation handshake, and the CREDIT/CANCEL payloads beyond the 4-byte LE
 * credit amount.  These are flagged "disclosed wire choice" / "not defined by an
 * exact confirmed byte layout" in the firmware and cannot be validated here
 * without real hardware, so the live SPI primitive and probe activation are a
 * follow-up on-hardware step.
 *
 * M1 Project
 */

#ifndef M1_ESP32_NATIVE_H_
#define M1_ESP32_NATIVE_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * Framing constants
 * (mtek_spi_native_frame.h:11-55)
 * =========================================================================*/

/** Native cell magic.  Stored little-endian; wire bytes are 4D 31 53 31
 *  ("M1S1").  (frame.h:11) */
#define MTK_SPI_NATIVE_MAGIC        UINT32_C(0x3153314D)

#define MTK_SPI_NATIVE_CELL_SIZE    1024u   /**< Full cell size (frame.h)          */
#define MTK_SPI_NATIVE_HEADER_SIZE  40u     /**< Fixed header size (frame.h)       */
#define MTK_SPI_NATIVE_MAX_PAYLOAD  984u    /**< 1024 - 40 (frame.h)               */
#define MTK_SPI_NATIVE_MAJOR        1u      /**< Protocol major (frame.h)          */
#define MTK_SPI_NATIVE_MINOR        0u      /**< Protocol minor (frame.h)          */

/** Byte offset of the CRC32C field inside the header; the CRC covers the first
 *  36 header bytes (0..35) immediately followed by the payload.  (frame.c:60-66) */
#define MTK_SPI_NATIVE_CRC_OFFSET   36u

/** Maximum reassembled message size; reduced from 65536 to fit the ESP32-C6
 *  DIRAM budget on the firmware side (frame.h:139).  The host mirrors the same
 *  ceiling so reassembly rejects over-long messages identically. */
#define MTK_SPI_NATIVE_MAX_MESSAGE  8192u

/* Message classes — header byte 6 "msg_class" (frame.h:18-29) */
#define MTK_SPI_CLASS_IDLE          0u
#define MTK_SPI_CLASS_HELLO         1u
#define MTK_SPI_CLASS_HELLO_ACK     2u
#define MTK_SPI_CLASS_REQUEST       3u
#define MTK_SPI_CLASS_RESPONSE      4u
#define MTK_SPI_CLASS_EVENT         5u
#define MTK_SPI_CLASS_STREAM        6u
#define MTK_SPI_CLASS_CREDIT        7u
#define MTK_SPI_CLASS_CANCEL        8u
#define MTK_SPI_CLASS_LINK_ERROR    9u

/* Flags — header byte 7 (frame.h:31-35) */
#define MTK_SPI_FLAG_FIRST          0x01u
#define MTK_SPI_FLAG_LAST           0x02u
#define MTK_SPI_FLAG_RETRY          0x04u
#define MTK_SPI_FLAG_ACK_REQUIRED   0x08u
#define MTK_SPI_FLAG_RESERVED_MASK  0xF0u  /**< any reserved bit set => bad flags */

/* =========================================================================
 * Service IDs (mtek_opcode_registry.c / CAPABILITY_MANIFEST.md)
 * =========================================================================*/
#define MTK_SVC_SYSTEM              UINT16_C(0x0000)
#define MTK_SVC_WIFI                UINT16_C(0x0001)
#define MTK_SVC_BLE                 UINT16_C(0x0002)
#define MTK_SVC_GATT                UINT16_C(0x0003)
#define MTK_SVC_CAPTURE             UINT16_C(0x0004)
#define MTK_SVC_DIAG                UINT16_C(0x0005)
#define MTK_SVC_ESPNOW              UINT16_C(0x0006)
#define MTK_SVC_IEEE802154          UINT16_C(0x0007)

/* System service (0x0000) opcodes (registry.c:8-16,120) */
#define MTK_SYS_OP_PING                 UINT16_C(0x0001)
#define MTK_SYS_OP_GET_VERSION          UINT16_C(0x0002)
#define MTK_SYS_OP_GET_LIMITS           UINT16_C(0x0003)
#define MTK_SYS_OP_GET_CAPABILITIES     UINT16_C(0x0004)
#define MTK_SYS_OP_GET_OPERATION_STATUS UINT16_C(0x0005)
#define MTK_SYS_OP_RESET_INTENT         UINT16_C(0x0006)
#define MTK_SYS_OP_GET_RESET_REASON     UINT16_C(0x0007)
#define MTK_SYS_OP_TIME_SYNC_START      UINT16_C(0x0008)
#define MTK_SYS_OP_TIME_SYNC_STOP       UINT16_C(0x0009)
#define MTK_SYS_OP_GET_API_IDENTITY     UINT16_C(0x000A)

/* =========================================================================
 * Status codes — header "status" field (mtek_schema_constants.h:4-21)
 * =========================================================================*/
#define MTK_STATUS_OK                   0u
#define MTK_STATUS_ACCEPTED             1u
#define MTK_STATUS_INVALID_ARGUMENT     2u
#define MTK_STATUS_UNSUPPORTED          3u
#define MTK_STATUS_BUSY                 4u
#define MTK_STATUS_NOT_READY            5u
#define MTK_STATUS_TIMEOUT              6u
#define MTK_STATUS_CANCELLED            7u
#define MTK_STATUS_NO_MEMORY            8u
#define MTK_STATUS_IO_ERROR             9u
#define MTK_STATUS_RADIO_CONFLICT       10u
#define MTK_STATUS_NOT_FOUND            11u
#define MTK_STATUS_PROTOCOL_ERROR       12u
#define MTK_STATUS_INTERNAL_ERROR       13u
#define MTK_STATUS_OVERFLOW             14u
#define MTK_STATUS_AUTHORIZATION_REQUIRED 15u

/* Native API identity constants (mtek_schema_constants.h) */
#define MTK_CORE_API_MAJOR          1u
#define MTK_CORE_API_MINOR          0u

/* =========================================================================
 * Header struct (in-memory mirror; never cast onto the wire — use the
 * serialize/parse helpers which apply the explicit LE byte layout).
 * (mtek_spi_native_frame.h:40-56)
 * =========================================================================*/
typedef struct {
    uint32_t magic;            /**< MTK_SPI_NATIVE_MAGIC                        */
    uint8_t  major;            /**< MTK_SPI_NATIVE_MAJOR                        */
    uint8_t  minor;            /**< MTK_SPI_NATIVE_MINOR                        */
    uint8_t  msg_class;        /**< MTK_SPI_CLASS_*                             */
    uint8_t  flags;            /**< MTK_SPI_FLAG_*                              */
    uint16_t service;          /**< MTK_SVC_*                                   */
    uint16_t opcode;           /**< per-service opcode                          */
    uint16_t status;           /**< MTK_STATUS_* (response); 0 on request       */
    uint16_t payload_len;      /**< 0..984                                      */
    uint32_t request_id;       /**< correlation id                             */
    uint32_t packet_seq;       /**< per-transaction sequence                   */
    uint32_t boot_epoch;       /**< sender boot nonce                          */
    uint32_t message_len;      /**< total logical message length               */
    uint32_t fragment_offset;  /**< bytes preceding this cell's payload        */
    uint32_t crc32c;           /**< CRC32C over header[0..35] + payload        */
} mtk_spi_native_header_t;

/** Result of parsing a received cell. */
typedef enum {
    MTK_PARSE_OK = 0,          /**< valid cell                                 */
    MTK_PARSE_SHORT,           /**< fewer than HEADER_SIZE bytes available     */
    MTK_PARSE_BAD_MAGIC,       /**< magic mismatch                             */
    MTK_PARSE_BAD_VERSION,     /**< major/minor mismatch                       */
    MTK_PARSE_BAD_FLAGS,       /**< a reserved flag bit was set                */
    MTK_PARSE_BAD_LENGTH,      /**< payload_len > 984 or overruns the buffer   */
    MTK_PARSE_BAD_CRC,         /**< CRC32C mismatch                            */
} mtk_native_parse_result_t;

/* =========================================================================
 * Little-endian scalar load/store (local, header-only)
 * =========================================================================*/
static inline void mtk_le_store16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8u) & 0xFFu);
}

static inline void mtk_le_store32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8u) & 0xFFu);
    p[2] = (uint8_t)((v >> 16u) & 0xFFu);
    p[3] = (uint8_t)((v >> 24u) & 0xFFu);
}

static inline uint16_t mtk_le_load16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8u));
}

static inline uint32_t mtk_le_load32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8u) |
           ((uint32_t)p[2] << 16u) | ((uint32_t)p[3] << 24u);
}

/* =========================================================================
 * CRC-32C (Castagnoli, reflected, init 0xFFFFFFFF, final XOR 0xFFFFFFFF)
 *
 * Bit-exact mirror of mtk_crc32c() (frame.c:8-18).  mtk_crc32c_accumulate()
 * exposes the running state so a frame CRC can span the two non-contiguous
 * regions the firmware concatenates — header[0..35] and the payload — with a
 * single logical pass (frame.c:60-66, 100-106).
 * =========================================================================*/
static inline uint32_t mtk_crc32c_accumulate(uint32_t crc,
                                             const uint8_t *data, size_t len)
{
    for (size_t i = 0u; i < len; i++)
    {
        crc ^= data[i];
        for (int b = 0; b < 8; b++)
        {
            uint32_t mask = (uint32_t)(-(int32_t)(crc & 1u));
            crc = (crc >> 1) ^ (0x82F63B78u & mask);
        }
    }
    return crc;
}

static inline uint32_t mtk_crc32c(const uint8_t *data, size_t len)
{
    return ~mtk_crc32c_accumulate(0xFFFFFFFFu, data, len);
}

/**
 * Compute the frame CRC32C exactly as the firmware does: over the first 36
 * header bytes concatenated with @p payload_len payload bytes.  (frame.c:60-66)
 */
static inline uint32_t mtk_native_frame_crc(const uint8_t *header36,
                                            const uint8_t *payload,
                                            uint16_t payload_len)
{
    uint32_t crc = mtk_crc32c_accumulate(0xFFFFFFFFu, header36,
                                         MTK_SPI_NATIVE_CRC_OFFSET);
    if (payload && payload_len > 0u)
        crc = mtk_crc32c_accumulate(crc, payload, payload_len);
    return ~crc;
}

/* =========================================================================
 * Header serialize / parse (explicit LE offsets — serialize_header,
 * frame.c:35-51)
 * =========================================================================*/

/**
 * Serialize @p h into the first 40 bytes of @p out (LE, no padding).  The
 * crc32c field (offset 36) is written from h->crc32c as-is; callers that build
 * a full cell should use mtk_native_build_cell(), which computes the CRC.
 *
 * @return true on success; false if @p out is NULL.
 */
static inline bool mtk_native_serialize_header(uint8_t *out,
                                               const mtk_spi_native_header_t *h)
{
    if (!out || !h)
        return false;
    mtk_le_store32(out + 0,  h->magic);
    out[4] = h->major;
    out[5] = h->minor;
    out[6] = h->msg_class;
    out[7] = h->flags;
    mtk_le_store16(out + 8,  h->service);
    mtk_le_store16(out + 10, h->opcode);
    mtk_le_store16(out + 12, h->status);
    mtk_le_store16(out + 14, h->payload_len);
    mtk_le_store32(out + 16, h->request_id);
    mtk_le_store32(out + 20, h->packet_seq);
    mtk_le_store32(out + 24, h->boot_epoch);
    mtk_le_store32(out + 28, h->message_len);
    mtk_le_store32(out + 32, h->fragment_offset);
    mtk_le_store32(out + 36, h->crc32c);
    return true;
}

/**
 * Parse the 40-byte header at @p in into @p h (does NOT validate magic / CRC;
 * use mtk_native_parse_cell() for a fully-checked parse).
 *
 * @return true on success; false if either pointer is NULL.
 */
static inline bool mtk_native_parse_header(const uint8_t *in,
                                           mtk_spi_native_header_t *h)
{
    if (!in || !h)
        return false;
    h->magic           = mtk_le_load32(in + 0);
    h->major           = in[4];
    h->minor           = in[5];
    h->msg_class       = in[6];
    h->flags           = in[7];
    h->service         = mtk_le_load16(in + 8);
    h->opcode          = mtk_le_load16(in + 10);
    h->status          = mtk_le_load16(in + 12);
    h->payload_len     = mtk_le_load16(in + 14);
    h->request_id      = mtk_le_load32(in + 16);
    h->packet_seq      = mtk_le_load32(in + 20);
    h->boot_epoch      = mtk_le_load32(in + 24);
    h->message_len     = mtk_le_load32(in + 28);
    h->fragment_offset = mtk_le_load32(in + 32);
    h->crc32c          = mtk_le_load32(in + 36);
    return true;
}

/* =========================================================================
 * Cell build / verify
 * =========================================================================*/

/**
 * Build a complete, zero-padded 1024-byte native cell into @p buf.
 *
 * Writes the 40-byte header from @p h (overriding h->payload_len with
 * @p payload_len and h->crc32c with the computed CRC), copies the payload at
 * offset 40, computes the CRC32C over header[0..35] + payload, stores it LE at
 * offset 36, and zero-pads the remainder to MTK_SPI_NATIVE_CELL_SIZE.
 *
 * @param buf          Output buffer (>= MTK_SPI_NATIVE_CELL_SIZE bytes)
 * @param buf_size     Capacity of @p buf
 * @param h            Header template (magic/major/minor/class/flags/service/
 *                     opcode/status/ids...); payload_len and crc32c are set here
 * @param payload      Payload bytes (may be NULL when payload_len == 0)
 * @param payload_len  0..984
 * @return MTK_SPI_NATIVE_CELL_SIZE on success, 0 on error (bad args / overflow).
 */
static inline uint16_t
mtk_native_build_cell(uint8_t *buf, size_t buf_size,
                      const mtk_spi_native_header_t *h,
                      const uint8_t *payload, uint16_t payload_len)
{
    if (!buf || !h || buf_size < MTK_SPI_NATIVE_CELL_SIZE)
        return 0u;
    if (payload_len > MTK_SPI_NATIVE_MAX_PAYLOAD)
        return 0u;
    if (payload_len > 0u && !payload)
        return 0u;

    mtk_spi_native_header_t hdr = *h;
    hdr.payload_len = payload_len;
    hdr.crc32c      = 0u;  /* placeholder; CRC covers bytes 0..35 only */

    memset(buf, 0, MTK_SPI_NATIVE_CELL_SIZE);
    mtk_native_serialize_header(buf, &hdr);

    if (payload && payload_len > 0u)
        memcpy(buf + MTK_SPI_NATIVE_HEADER_SIZE, payload, payload_len);

    uint32_t crc = mtk_native_frame_crc(buf, payload, payload_len);
    mtk_le_store32(buf + MTK_SPI_NATIVE_CRC_OFFSET, crc);

    return (uint16_t)MTK_SPI_NATIVE_CELL_SIZE;
}

/**
 * Validate and parse a received native cell.
 *
 * Checks (in order): minimum length, magic, major/minor, reserved flag bits,
 * payload_len bound + buffer fit, and CRC32C.  On MTK_PARSE_OK fills @p h and
 * points @p *payload_out at the in-buffer payload (NULL when payload_len == 0).
 *
 * @param buf             Received buffer
 * @param buf_len         Valid bytes in @p buf (>= 40 for a header; a full cell
 *                        is 1024 but a shorter 512-byte discovery cell is also
 *                        accepted as long as it contains the whole frame)
 * @param h               Parsed header out (may be NULL)
 * @param payload_out     Set to the payload start on success (may be NULL)
 * @param payload_len_out Payload length on success (may be NULL)
 * @return MTK_PARSE_* result code.
 */
static inline mtk_native_parse_result_t
mtk_native_parse_cell(const uint8_t *buf, size_t buf_len,
                      mtk_spi_native_header_t *h,
                      const uint8_t **payload_out,
                      uint16_t *payload_len_out)
{
    if (payload_out)     *payload_out = NULL;
    if (payload_len_out) *payload_len_out = 0u;

    if (!buf || buf_len < MTK_SPI_NATIVE_HEADER_SIZE)
        return MTK_PARSE_SHORT;

    mtk_spi_native_header_t hdr;
    mtk_native_parse_header(buf, &hdr);

    if (hdr.magic != MTK_SPI_NATIVE_MAGIC)
        return MTK_PARSE_BAD_MAGIC;
    if (hdr.major != MTK_SPI_NATIVE_MAJOR || hdr.minor != MTK_SPI_NATIVE_MINOR)
        return MTK_PARSE_BAD_VERSION;
    if ((hdr.flags & MTK_SPI_FLAG_RESERVED_MASK) != 0u)
        return MTK_PARSE_BAD_FLAGS;
    if (hdr.payload_len > MTK_SPI_NATIVE_MAX_PAYLOAD)
        return MTK_PARSE_BAD_LENGTH;
    if ((size_t)MTK_SPI_NATIVE_HEADER_SIZE + (size_t)hdr.payload_len > buf_len)
        return MTK_PARSE_BAD_LENGTH;

    const uint8_t *payload = buf + MTK_SPI_NATIVE_HEADER_SIZE;
    uint32_t expected = mtk_native_frame_crc(buf, payload, hdr.payload_len);
    if (expected != hdr.crc32c)
        return MTK_PARSE_BAD_CRC;

    if (h)               *h = hdr;
    if (payload_out)     *payload_out = (hdr.payload_len > 0u) ? payload : NULL;
    if (payload_len_out) *payload_len_out = hdr.payload_len;
    return MTK_PARSE_OK;
}

/* =========================================================================
 * Fragment reassembly (frame.c:117-125, dispatch rules)
 *
 * Multi-cell messages correlate by request_id; each cell's fragment_offset must
 * equal the number of payload bytes already received, and message_len is the
 * total.  FIRST marks the opening cell, LAST the final one.  The host assembles
 * into a caller-provided buffer bounded by MTK_SPI_NATIVE_MAX_MESSAGE.
 * =========================================================================*/
typedef struct {
    uint8_t  active;        /**< 1 once a FIRST cell has been accepted         */
    uint8_t  complete;      /**< 1 once the LAST cell has been accepted        */
    uint32_t request_id;    /**< correlation id of the message in progress     */
    uint32_t message_len;   /**< total expected length (from the FIRST cell)   */
    uint32_t received;      /**< payload bytes accumulated so far              */
    uint8_t *buf;           /**< caller-owned assembly buffer                  */
    size_t   buf_cap;       /**< capacity of @p buf                            */
} mtk_native_reasm_t;

/** Result of feeding one cell to the reassembler. */
typedef enum {
    MTK_REASM_NEED_MORE = 0, /**< cell accepted; more fragments expected       */
    MTK_REASM_COMPLETE,      /**< message complete (LAST cell accepted)        */
    MTK_REASM_ERROR,         /**< protocol violation / overflow — state reset  */
} mtk_native_reasm_status_t;

/** Reset a reassembler to point at a (possibly reused) assembly buffer. */
static inline void mtk_native_reasm_init(mtk_native_reasm_t *r,
                                         uint8_t *buf, size_t buf_cap)
{
    if (!r)
        return;
    r->active      = 0u;
    r->complete    = 0u;
    r->request_id  = 0u;
    r->message_len = 0u;
    r->received    = 0u;
    r->buf         = buf;
    r->buf_cap     = buf_cap;
}

/**
 * Feed one parsed cell (header @p h + payload) to the reassembler.
 *
 * Enforces: a FIRST cell starts a new message (and, while one is already active
 * for a different request_id, is rejected as a BUSY-style violation mirroring
 * the firmware's single-inbound-reassembly rule); continuation cells must match
 * the active request_id and have fragment_offset == received; message_len must
 * be consistent and within both the caller buffer and MTK_SPI_NATIVE_MAX_MESSAGE.
 *
 * On MTK_REASM_COMPLETE, @p r->buf holds @p r->received (== message_len) bytes.
 * On MTK_REASM_ERROR the reassembler is reset (active = 0).
 */
static inline mtk_native_reasm_status_t
mtk_native_reasm_feed(mtk_native_reasm_t *r,
                      const mtk_spi_native_header_t *h,
                      const uint8_t *payload)
{
    if (!r || !h || !r->buf)
        return MTK_REASM_ERROR;

    const bool is_first = (h->flags & MTK_SPI_FLAG_FIRST) != 0u;
    const bool is_last  = (h->flags & MTK_SPI_FLAG_LAST) != 0u;

    if (is_first)
    {
        /* A FIRST cell for a different request while one is already in
         * progress is a protocol violation (firmware: MTK_SPI_REASM_BUSY). */
        if (r->active && h->request_id != r->request_id)
        {
            mtk_native_reasm_init(r, r->buf, r->buf_cap);
            return MTK_REASM_ERROR;
        }
        if (h->message_len > MTK_SPI_NATIVE_MAX_MESSAGE ||
            h->message_len > r->buf_cap ||
            h->fragment_offset != 0u ||
            h->payload_len > h->message_len)
        {
            mtk_native_reasm_init(r, r->buf, r->buf_cap);
            return MTK_REASM_ERROR;
        }
        r->active      = 1u;
        r->complete    = 0u;
        r->request_id  = h->request_id;
        r->message_len = h->message_len;
        r->received    = 0u;
    }
    else
    {
        /* Continuation without an active message, or mismatched id/offset. */
        if (!r->active ||
            h->request_id != r->request_id ||
            h->fragment_offset != r->received ||
            h->message_len != r->message_len)
        {
            mtk_native_reasm_init(r, r->buf, r->buf_cap);
            return MTK_REASM_ERROR;
        }
    }

    /* Bounds-check this fragment against the declared total and the buffer. */
    if ((uint32_t)r->received + (uint32_t)h->payload_len > r->message_len ||
        (size_t)r->received + (size_t)h->payload_len > r->buf_cap)
    {
        mtk_native_reasm_init(r, r->buf, r->buf_cap);
        return MTK_REASM_ERROR;
    }

    if (payload && h->payload_len > 0u)
        memcpy(r->buf + r->received, payload, h->payload_len);
    r->received += h->payload_len;

    if (is_last)
    {
        if (r->received != r->message_len)
        {
            mtk_native_reasm_init(r, r->buf, r->buf_cap);
            return MTK_REASM_ERROR;
        }
        r->complete = 1u;
        return MTK_REASM_COMPLETE;
    }
    return MTK_REASM_NEED_MORE;
}

/* =========================================================================
 * Payload decoders — system service (structs.h)
 * =========================================================================*/

/** PING req/resp body: a single 32-bit nonce (structs.h:62-68). */
static inline bool mtk_native_decode_ping(const uint8_t *payload,
                                          uint16_t len, uint32_t *nonce_out)
{
    if (!payload || len < 4u)
        return false;
    if (nonce_out)
        *nonce_out = mtk_le_load32(payload);
    return true;
}

/**
 * GET_API_IDENTITY response head (structs.h:1335-1341):
 *   { u8 api_major; u8 api_minor; u8 variant_id; u16 capability_count;
 *     { u16 len; u8 data[24] } variant_name }
 * Decodes the fixed-position scalar fields and the variant-name length prefix.
 */
typedef struct {
    uint8_t  api_major;
    uint8_t  api_minor;
    uint8_t  variant_id;
    uint16_t capability_count;
    uint16_t variant_name_len;        /**< bounded to <= 24 on decode          */
    char     variant_name[25];        /**< null-terminated copy                 */
} mtk_native_api_identity_t;

static inline bool
mtk_native_decode_api_identity(const uint8_t *payload, uint16_t len,
                               mtk_native_api_identity_t *out)
{
    /* 3 scalar bytes + u16 count + u16 name_len = 7 fixed bytes minimum. */
    if (!payload || !out || len < 7u)
        return false;
    out->api_major        = payload[0];
    out->api_minor        = payload[1];
    out->variant_id       = payload[2];
    out->capability_count = mtk_le_load16(payload + 3);
    uint16_t nlen         = mtk_le_load16(payload + 5);
    if (nlen > 24u)
        nlen = 24u;
    if ((size_t)7u + (size_t)nlen > len)
        return false;
    memcpy(out->variant_name, payload + 7, nlen);
    out->variant_name[nlen]  = '\0';
    out->variant_name_len    = nlen;
    return true;
}

/**
 * One GET_CAPABILITIES entry (structs.h:104-124), decoded from the flat LE
 * byte stream.  Layout per entry:
 *   u16 service_id, u16 opcode, u16 capability_id,
 *   u8 service_major, u8 service_minor, u8 state, u8 max_concurrent,
 *   u16 max_payload, { u32 count; u16 items[4] } dependencies.
 * Fixed size = 2+2+2+1+1+1+1+2 + (4 + 4*2) = 24 bytes.
 */
#define MTK_NATIVE_CAP_ENTRY_SIZE 24u

typedef struct {
    uint16_t service_id;
    uint16_t opcode;
    uint16_t capability_id;
    uint8_t  service_major;
    uint8_t  service_minor;
    uint8_t  state;             /**< mtk_capability_state_t                     */
    uint8_t  max_concurrent;
    uint16_t max_payload;
    uint32_t dep_count;
    uint16_t dep_items[4];
} mtk_native_cap_entry_t;

/* Capability state values (mtek_schema_constants.h:46-52). */
#define MTK_CAP_STATE_SUPPORTED     0u
#define MTK_CAP_STATE_DISABLED      1u
#define MTK_CAP_STATE_BUSY          2u
#define MTK_CAP_STATE_UNAVAILABLE   3u
#define MTK_CAP_STATE_UNSUPPORTED   4u

static inline bool
mtk_native_decode_cap_entry(const uint8_t *p, uint16_t len,
                            mtk_native_cap_entry_t *out)
{
    if (!p || !out || len < MTK_NATIVE_CAP_ENTRY_SIZE)
        return false;
    out->service_id     = mtk_le_load16(p + 0);
    out->opcode         = mtk_le_load16(p + 2);
    out->capability_id  = mtk_le_load16(p + 4);
    out->service_major  = p[6];
    out->service_minor  = p[7];
    out->state          = p[8];
    out->max_concurrent = p[9];
    out->max_payload    = mtk_le_load16(p + 10);
    out->dep_count      = mtk_le_load32(p + 12);
    for (unsigned i = 0u; i < 4u; i++)
        out->dep_items[i] = mtk_le_load16(p + 16 + (i * 2u));
    return true;
}

/**
 * Decode the GET_CAPABILITIES response envelope (structs.h:153-156):
 *   { { u32 count; entry items[32] } entries; u16 next_index }
 * Reports the entry count, a pointer to the first entry's bytes, and the
 * next_index pagination cursor.  Individual entries are decoded with
 * mtk_native_decode_cap_entry() (fixed MTK_NATIVE_CAP_ENTRY_SIZE stride).
 *
 * @param payload        GET_CAPABILITIES response payload
 * @param len            Payload length
 * @param count_out      Number of entries in this page (<= 32)
 * @param entries_out    Points at the first entry's bytes (may be NULL)
 * @param next_index_out Pagination cursor; 0 means "no more pages"
 * @return true on a well-formed envelope; false on truncation / overflow.
 */
static inline bool
mtk_native_decode_capabilities_page(const uint8_t *payload, uint16_t len,
                                    uint32_t *count_out,
                                    const uint8_t **entries_out,
                                    uint16_t *next_index_out)
{
    if (entries_out)    *entries_out = NULL;
    if (count_out)      *count_out = 0u;
    if (next_index_out) *next_index_out = 0u;
    if (!payload || len < 4u)
        return false;

    uint32_t count = mtk_le_load32(payload);
    if (count > 32u)
        return false;

    size_t entries_bytes = (size_t)count * MTK_NATIVE_CAP_ENTRY_SIZE;
    /* 4-byte count prefix + entries + 2-byte next_index trailer. */
    if ((size_t)4u + entries_bytes + 2u > len)
        return false;

    if (count_out)      *count_out = count;
    if (entries_out)    *entries_out = payload + 4;
    if (next_index_out) *next_index_out = mtk_le_load16(payload + 4 + entries_bytes);
    return true;
}

/* =========================================================================
 * Runtime client (implemented in m1_esp32_native.c)
 *
 * A thin request/response client over a caller-supplied single-cell exchange
 * primitive.  The exchange primitive clocks exactly MTK_SPI_NATIVE_CELL_SIZE
 * bytes in each direction in one SPI transaction (on-target: CS + HANDSHAKE on
 * the 1024-byte native SPI link; host tests: a fake canned-cell queue).  The
 * client drives the HELLO handshake, builds a single-cell request, polls IDLE
 * cells for the correlated RESPONSE, and reassembles multi-cell responses.
 *
 * NOTE (scope): the physical 1024-byte SPI primitive and probe-time activation
 * are a deliberate follow-up on-hardware step and are NOT wired here — the
 * firmware authors disclose the physical 512->1024 cell-size negotiation and the
 * HELLO negotiation payload as undefined in the contract package, so they cannot
 * be validated without real MtkCore hardware.  Everything in THIS module is
 * pure logic or driven through an injectable transport, hence fully host-tested.
 * =========================================================================*/

/**
 * Single fixed-size (1024-byte) full-duplex exchange primitive.
 * @return 0 on success, non-zero on transport error / timeout.
 */
typedef int (*mtk_native_xfer_fn)(const uint8_t *tx, uint8_t *rx,
                                  uint16_t cell_size, void *ctx);

/** Client result codes. */
typedef enum {
    MTK_NATIVE_OK = 0,        /**< response received, device status == OK       */
    MTK_NATIVE_ERR_INVALID,   /**< bad arguments                               */
    MTK_NATIVE_ERR_NO_MEM,    /**< scratch/reassembly allocation failed         */
    MTK_NATIVE_ERR_TRANSPORT, /**< exchange primitive reported an error         */
    MTK_NATIVE_ERR_PROTOCOL,  /**< malformed cell / reassembly violation        */
    MTK_NATIVE_ERR_TIMEOUT,   /**< no correlated response within the poll budget*/
    MTK_NATIVE_ERR_STATUS,    /**< response received, device status != OK       */
} mtk_native_result_t;

/** Monotonic non-zero request-id generator (wraps past UINT32_MAX skipping 0). */
uint32_t mtk_native_next_request_id(void);

/** Reset the request-id sequence (host tests only). */
void mtk_native_reset_request_id(uint32_t seed);

/**
 * Perform the HELLO / HELLO_ACK handshake.
 *
 * Sends a HELLO cell stamped with @p host_boot_epoch and polls up to
 * @p max_polls IDLE cells for a HELLO_ACK echoing the request_id.  Per the
 * firmware contract HELLO_ACK carries an empty payload, so success is a bare
 * acknowledgement (no negotiation payload is parsed).
 *
 * @return MTK_NATIVE_OK on a matching HELLO_ACK; otherwise an error code.
 */
mtk_native_result_t mtk_native_hello(mtk_native_xfer_fn xfer, void *ctx,
                                     uint32_t host_boot_epoch, int max_polls);

/**
 * Issue a single-cell request and return the (possibly multi-cell) response.
 *
 * The request payload must be <= MTK_SPI_NATIVE_MAX_PAYLOAD (984 bytes); larger
 * request bodies (only RAW_TX / large GATT writes) require request-side
 * fragmentation, which is a documented follow-up.  Responses of any size up to
 * MTK_SPI_NATIVE_MAX_MESSAGE are reassembled.
 *
 * @param xfer            Single-cell exchange primitive
 * @param ctx             Opaque context for @p xfer
 * @param service         MTK_SVC_*
 * @param opcode          per-service opcode
 * @param req             request payload (may be NULL when req_len == 0)
 * @param req_len         request payload length (0..984)
 * @param resp            response payload out (may be NULL to discard)
 * @param resp_cap        capacity of @p resp
 * @param resp_len        [out] response payload bytes available (set even when
 *                        > resp_cap, so callers can detect truncation)
 * @param status_out      [out] device status code from the response header
 * @param host_boot_epoch epoch stamped into the request header
 * @param max_polls       follow-up IDLE transactions to issue (>= 1)
 * @return MTK_NATIVE_OK (status OK) / MTK_NATIVE_ERR_STATUS (status != OK) /
 *         other error codes on transport / protocol / timeout failures.
 */
mtk_native_result_t mtk_native_call(mtk_native_xfer_fn xfer, void *ctx,
                                    uint16_t service, uint16_t opcode,
                                    const uint8_t *req, uint16_t req_len,
                                    uint8_t *resp, size_t resp_cap,
                                    size_t *resp_len, uint16_t *status_out,
                                    uint32_t host_boot_epoch, int max_polls);

#ifdef __cplusplus
}
#endif

#endif /* M1_ESP32_NATIVE_H_ */
