/* See COPYING.txt for license details. */

/*
 * test_cc1101_fec.c
 *
 * Unit tests for the CC1101 FEC encode / decode utility (cc1101_fec.c).
 *
 * Tests encode and decode of a known payload using the test vector from the
 * original SpaceTeddy URH plugin comment:
 *
 *   input payload  (6 bytes): 03 01 00 01 02 03
 *   FEC-encoded   (20 bytes): 88 c8 3c 00 0c 33 30 12 4c f0
 *                             30 10 b8 dc a3 53 40 34 7f e3
 *
 * Additionally exercises:
 *   - cc1101_fec_encoded_size() / cc1101_fec_decoded_size()
 *   - cc1101_fec_crc16()
 *   - encode → decode roundtrip (various payload lengths)
 *   - edge cases: NULL pointers, oversized payload
 *
 * Build (from repo root):
 *   cmake -B build-tests -S tests && cmake --build build-tests
 *   ctest --test-dir build-tests --output-on-failure
 */

#include "unity.h"
#include "cc1101_fec.h"
#include <string.h>

/* ======================================================================= */
/* Known test vector (from SpaceTeddy plugin source comments)              */
/* ======================================================================= */

static const uint8_t tv_payload[6] = {0x03, 0x01, 0x00, 0x01, 0x02, 0x03};

static const uint8_t tv_encoded[20] = {
    0x88, 0xc8, 0x3c, 0x00,
    0x0c, 0x33, 0x30, 0x12,
    0x4c, 0xf0, 0x30, 0x10,
    0xb8, 0xdc, 0xa3, 0x53,
    0x40, 0x34, 0x7f, 0xe3
};

/* ======================================================================= */
/* Unity hooks                                                              */
/* ======================================================================= */

void setUp(void)   {}
void tearDown(void) {}

/* ======================================================================= */
/* Size-helper tests                                                        */
/* ======================================================================= */

void test_encoded_size_matches_tv(void)
{
    /* 6 payload bytes → 20 encoded bytes */
    TEST_ASSERT_EQUAL_UINT16(20u, cc1101_fec_encoded_size(6u));
}

void test_decoded_size_from_20_encoded(void)
{
    /* 20 encoded bytes → 9 decoded bytes (1 length + 6 data + 2 CRC) */
    TEST_ASSERT_EQUAL_UINT16(9u, cc1101_fec_decoded_size(20u));
}

void test_size_symmetry_various_lengths(void)
{
    /* For every supported payload length n, verify the decoded size matches
     * the formula: decoded = 2 * ((n+3) / 2) + 1
     * (integer division).  For even n this equals n+3; for odd n it equals
     * n+4 — the extra byte carries a partial trellis terminator symbol. */
    uint8_t n;
    for (n = 1u; n <= 30u; n++) {
        uint16_t enc     = cc1101_fec_encoded_size(n);
        uint16_t dec     = cc1101_fec_decoded_size(enc);
        uint16_t inputNum = (uint16_t)(n + 3u);
        uint16_t expected = 2u * (inputNum / 2u) + 1u;
        TEST_ASSERT_EQUAL_UINT16(expected, dec);
    }
}

void test_decoded_size_zero_on_tiny_input(void)
{
    /* Inputs below 8 or not a multiple of 4 must return 0 */
    TEST_ASSERT_EQUAL_UINT16(0u, cc1101_fec_decoded_size(0u));
    TEST_ASSERT_EQUAL_UINT16(0u, cc1101_fec_decoded_size(3u));
    TEST_ASSERT_EQUAL_UINT16(0u, cc1101_fec_decoded_size(4u));
    TEST_ASSERT_EQUAL_UINT16(0u, cc1101_fec_decoded_size(7u));
    TEST_ASSERT_EQUAL_UINT16(0u, cc1101_fec_decoded_size(9u));
}

/* ======================================================================= */
/* CRC-16/IBM tests                                                         */
/* ======================================================================= */

void test_crc16_single_byte(void)
{
    /* CRC-16/IBM(0x00, init=0xFFFF) = 0xFD02 (verified by computation).
     * This confirms the polynomial 0x8005 is applied correctly. */
    TEST_ASSERT_EQUAL_UINT16(0xFD02u, cc1101_fec_crc16(0x00u, 0xFFFFu));
}

void test_crc16_all_ones_byte(void)
{
    /* CRC-16/IBM(0xFF, init=0xFFFF) = 0xFF00 */
    TEST_ASSERT_EQUAL_UINT16(0xFF00u, cc1101_fec_crc16(0xFFu, 0xFFFFu));
}

void test_crc16_known_sequence(void)
{
    /*
     * CRC-16/IBM over the pre-FEC buffer for the test vector:
     *   buf[0] = 0x06 (length)
     *   buf[1..6] = 03 01 00 01 02 03 (payload)
     *
     * The encoded test vector was produced by the CC1101 encode path, so
     * decoding it back and verifying the CRC is an implicit check.  Here we
     * just confirm that crc16 is deterministic and matches itself.
     */
    uint16_t crc = 0xFFFFu;
    crc = cc1101_fec_crc16(0x06u, crc);
    crc = cc1101_fec_crc16(0x03u, crc);
    crc = cc1101_fec_crc16(0x01u, crc);
    crc = cc1101_fec_crc16(0x00u, crc);
    crc = cc1101_fec_crc16(0x01u, crc);
    crc = cc1101_fec_crc16(0x02u, crc);
    crc = cc1101_fec_crc16(0x03u, crc);

    /* Must be non-zero and reproducible */
    TEST_ASSERT_NOT_EQUAL(0u, crc);

    /* Recompute and verify identical */
    uint16_t crc2 = 0xFFFFu;
    crc2 = cc1101_fec_crc16(0x06u, crc2);
    crc2 = cc1101_fec_crc16(0x03u, crc2);
    crc2 = cc1101_fec_crc16(0x01u, crc2);
    crc2 = cc1101_fec_crc16(0x00u, crc2);
    crc2 = cc1101_fec_crc16(0x01u, crc2);
    crc2 = cc1101_fec_crc16(0x02u, crc2);
    crc2 = cc1101_fec_crc16(0x03u, crc2);
    TEST_ASSERT_EQUAL_UINT16(crc, crc2);
}

/* ======================================================================= */
/* Encode tests                                                             */
/* ======================================================================= */

void test_encode_matches_known_vector(void)
{
    uint8_t  enc[32];
    uint16_t len;

    len = cc1101_fec_encode(enc, tv_payload, sizeof(tv_payload));

    TEST_ASSERT_EQUAL_UINT16(20u, len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(tv_encoded, enc, 20u);
}

void test_encode_null_output_returns_zero(void)
{
    TEST_ASSERT_EQUAL_UINT16(0u, cc1101_fec_encode(NULL, tv_payload, 6u));
}

void test_encode_null_input_returns_zero(void)
{
    uint8_t enc[32];
    TEST_ASSERT_EQUAL_UINT16(0u, cc1101_fec_encode(enc, NULL, 6u));
}

void test_encode_oversized_payload_returns_zero(void)
{
    uint8_t enc[256];
    uint8_t big[CC1101_FEC_MAX_PAYLOAD + 1u];
    memset(big, 0xABu, sizeof(big));
    TEST_ASSERT_EQUAL_UINT16(0u, cc1101_fec_encode(enc, big, CC1101_FEC_MAX_PAYLOAD + 1u));
}

/* ======================================================================= */
/* Decode (high-level) tests                                                */
/* ======================================================================= */

void test_decode_packet_known_vector(void)
{
    uint8_t  dec[16];
    uint16_t len;

    len = cc1101_fec_decode_packet(dec, tv_encoded, sizeof(tv_encoded));

    /* Decoded: 9 bytes = length(1) + payload(6) + CRC(2) */
    TEST_ASSERT_EQUAL_UINT16(9u, len);

    /* First decoded byte is the payload length */
    TEST_ASSERT_EQUAL_UINT8(6u, dec[0]);

    /* Next 6 bytes are the original payload */
    TEST_ASSERT_EQUAL_UINT8_ARRAY(tv_payload, &dec[1], 6u);
}

void test_decode_packet_crc_bytes_present(void)
{
    uint8_t  dec[16];
    uint16_t len;

    len = cc1101_fec_decode_packet(dec, tv_encoded, sizeof(tv_encoded));
    TEST_ASSERT_EQUAL_UINT16(9u, len);

    /* CRC bytes at positions 7 and 8 must match what cc1101_fec_crc16 produces */
    uint16_t crc = 0xFFFFu;
    uint8_t  i;
    for (i = 0u; i <= dec[0]; i++)     /* bytes [0 .. length] inclusive */
        crc = cc1101_fec_crc16(dec[i], crc);

    TEST_ASSERT_EQUAL_UINT8((uint8_t)(crc >> 8u),   dec[7]);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)(crc & 0xFFu), dec[8]);
}

void test_decode_packet_null_output_returns_zero(void)
{
    TEST_ASSERT_EQUAL_UINT16(0u,
        cc1101_fec_decode_packet(NULL, tv_encoded, sizeof(tv_encoded)));
}

void test_decode_packet_null_input_returns_zero(void)
{
    uint8_t dec[16];
    TEST_ASSERT_EQUAL_UINT16(0u,
        cc1101_fec_decode_packet(dec, NULL, 20u));
}

void test_decode_packet_too_short_returns_zero(void)
{
    uint8_t dec[16];
    /* Need at least 8 bytes and a multiple of 4 */
    TEST_ASSERT_EQUAL_UINT16(0u,
        cc1101_fec_decode_packet(dec, tv_encoded, 4u));
    TEST_ASSERT_EQUAL_UINT16(0u,
        cc1101_fec_decode_packet(dec, tv_encoded, 7u));
}

void test_decode_packet_unaligned_length_returns_zero(void)
{
    uint8_t dec[16];
    /* encoded_len must be a multiple of 4 */
    TEST_ASSERT_EQUAL_UINT16(0u,
        cc1101_fec_decode_packet(dec, tv_encoded, 9u));
}

/* ======================================================================= */
/* Roundtrip tests (encode then decode)                                     */
/* ======================================================================= */

void test_roundtrip_single_byte_payload(void)
{
    const uint8_t payload[1] = {0x42u};
    uint8_t enc[128];
    uint8_t dec[32];
    uint16_t enc_len, dec_len;

    enc_len = cc1101_fec_encode(enc, payload, 1u);
    TEST_ASSERT_EQUAL_UINT16(cc1101_fec_encoded_size(1u), enc_len);

    dec_len = cc1101_fec_decode_packet(dec, enc, enc_len);
    /* Decoded output includes length byte, payload, CRC, and possibly one
     * partial trellis-terminator byte (when payload length is odd). */
    TEST_ASSERT_GREATER_THAN_UINT16(0u, dec_len);
    TEST_ASSERT_EQUAL_UINT8(1u,    dec[0]); /* length byte */
    TEST_ASSERT_EQUAL_UINT8(0x42u, dec[1]); /* payload */
}

void test_roundtrip_all_zeros(void)
{
    const uint8_t payload[8] = {0};
    uint8_t enc[128];
    uint8_t dec[32];
    uint16_t enc_len, dec_len;

    enc_len = cc1101_fec_encode(enc, payload, sizeof(payload));
    dec_len = cc1101_fec_decode_packet(dec, enc, enc_len);

    TEST_ASSERT_EQUAL_UINT8((uint8_t)sizeof(payload), dec[0]);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, &dec[1], sizeof(payload));
    (void)dec_len;
}

void test_roundtrip_all_ones(void)
{
    const uint8_t payload[8] = {
        0xFFu, 0xFFu, 0xFFu, 0xFFu,
        0xFFu, 0xFFu, 0xFFu, 0xFFu
    };
    uint8_t enc[128];
    uint8_t dec[32];
    uint16_t enc_len, dec_len;

    enc_len = cc1101_fec_encode(enc, payload, sizeof(payload));
    dec_len = cc1101_fec_decode_packet(dec, enc, enc_len);

    TEST_ASSERT_EQUAL_UINT8((uint8_t)sizeof(payload), dec[0]);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, &dec[1], sizeof(payload));
    (void)dec_len;
}

void test_roundtrip_alternating_bits(void)
{
    const uint8_t payload[4] = {0xAAu, 0x55u, 0xAAu, 0x55u};
    uint8_t enc[128];
    uint8_t dec[32];
    uint16_t enc_len, dec_len;

    enc_len = cc1101_fec_encode(enc, payload, sizeof(payload));
    dec_len = cc1101_fec_decode_packet(dec, enc, enc_len);

    TEST_ASSERT_EQUAL_UINT8((uint8_t)sizeof(payload), dec[0]);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, &dec[1], sizeof(payload));
    (void)dec_len;
}

void test_roundtrip_max_supported_payload(void)
{
    uint8_t payload[CC1101_FEC_MAX_PAYLOAD];
    uint8_t enc[512];
    uint8_t dec[256];
    uint8_t i;
    uint16_t enc_len, dec_len;

    for (i = 0u; i < CC1101_FEC_MAX_PAYLOAD; i++)
        payload[i] = i;

    enc_len = cc1101_fec_encode(enc, payload, CC1101_FEC_MAX_PAYLOAD);
    TEST_ASSERT_GREATER_THAN_UINT16(0u, enc_len);

    dec_len = cc1101_fec_decode_packet(dec, enc, enc_len);
    TEST_ASSERT_EQUAL_UINT8(CC1101_FEC_MAX_PAYLOAD, dec[0]);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, &dec[1], CC1101_FEC_MAX_PAYLOAD);
    (void)dec_len;
}

/* ======================================================================= */
/* Low-level decode (cc1101_fec_decode) tests                              */
/* ======================================================================= */

void test_decode_init_zeroes_context(void)
{
    CC1101_FEC_DecCtx_t ctx;
    /* Fill with garbage */
    memset(&ctx, 0xABu, sizeof(ctx));
    cc1101_fec_decode_init(&ctx);

    /* State 0 cost must be 0; all others must be 100 */
    TEST_ASSERT_EQUAL_UINT8(0u,   ctx.nCost[0][0]);
    TEST_ASSERT_EQUAL_UINT8(100u, ctx.nCost[0][1]);
    TEST_ASSERT_EQUAL_UINT8(100u, ctx.nCost[0][7]);
    TEST_ASSERT_EQUAL_UINT8(0u,   ctx.iLastBuf);
    TEST_ASSERT_EQUAL_UINT8(1u,   ctx.iCurrBuf);
    TEST_ASSERT_EQUAL_UINT8(0u,   ctx.nPathBits);
}

void test_decode_null_ctx_returns_zero(void)
{
    uint8_t out[8];
    TEST_ASSERT_EQUAL_UINT16(0u,
        cc1101_fec_decode(NULL, out, tv_encoded, 9u));
}

void test_decode_null_output_returns_zero(void)
{
    CC1101_FEC_DecCtx_t ctx;
    cc1101_fec_decode_init(&ctx);
    TEST_ASSERT_EQUAL_UINT16(0u,
        cc1101_fec_decode(&ctx, NULL, tv_encoded, 9u));
}

void test_decode_null_input_returns_zero(void)
{
    CC1101_FEC_DecCtx_t ctx;
    uint8_t out[8];
    cc1101_fec_decode_init(&ctx);
    TEST_ASSERT_EQUAL_UINT16(0u,
        cc1101_fec_decode(&ctx, out, NULL, 9u));
}

/* ======================================================================= */
/* Main                                                                     */
/* ======================================================================= */

int main(void)
{
    UNITY_BEGIN();

    /* Size helpers */
    RUN_TEST(test_encoded_size_matches_tv);
    RUN_TEST(test_decoded_size_from_20_encoded);
    RUN_TEST(test_size_symmetry_various_lengths);
    RUN_TEST(test_decoded_size_zero_on_tiny_input);

    /* CRC */
    RUN_TEST(test_crc16_single_byte);
    RUN_TEST(test_crc16_all_ones_byte);
    RUN_TEST(test_crc16_known_sequence);

    /* Encode */
    RUN_TEST(test_encode_matches_known_vector);
    RUN_TEST(test_encode_null_output_returns_zero);
    RUN_TEST(test_encode_null_input_returns_zero);
    RUN_TEST(test_encode_oversized_payload_returns_zero);

    /* Decode (high-level) */
    RUN_TEST(test_decode_packet_known_vector);
    RUN_TEST(test_decode_packet_crc_bytes_present);
    RUN_TEST(test_decode_packet_null_output_returns_zero);
    RUN_TEST(test_decode_packet_null_input_returns_zero);
    RUN_TEST(test_decode_packet_too_short_returns_zero);
    RUN_TEST(test_decode_packet_unaligned_length_returns_zero);

    /* Roundtrips */
    RUN_TEST(test_roundtrip_single_byte_payload);
    RUN_TEST(test_roundtrip_all_zeros);
    RUN_TEST(test_roundtrip_all_ones);
    RUN_TEST(test_roundtrip_alternating_bits);
    RUN_TEST(test_roundtrip_max_supported_payload);

    /* Low-level decode */
    RUN_TEST(test_decode_init_zeroes_context);
    RUN_TEST(test_decode_null_ctx_returns_zero);
    RUN_TEST(test_decode_null_output_returns_zero);
    RUN_TEST(test_decode_null_input_returns_zero);

    return UNITY_END();
}
