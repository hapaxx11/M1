/* See COPYING.txt for license details. */

/*
 * test_subghz_proto_pirate.c
 *
 * Host unit tests for Sub_Ghz/subghz_proto_pirate.c
 *
 * Verifies:
 *   - Catalog lookup (exact + case-insensitive)
 *   - is_supported() predicate
 *   - Kia V7 encoder packet structure, CRC, bit-inversion, repetition count
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "unity.h"
#include "bit_util.h"
#include "subghz_proto_pirate.h"
#include "subghz_kia_v6_encoder.h"
#include "subghz_keeloq.h"
#include "subghz_manchester_encoder.h"
#include "subghz_protopirate_keys_builtin.h"
#include "subghz_vag_encoder.h"

void setUp(void) {}
void tearDown(void) {}

/* Real mixer inverse from m1_kia_v5_decode.c (linked into this target). */
uint16_t m1_kia_v5_mixer_decode(uint32_t encrypted, uint64_t mfkey);

static SubGhzKeyParams make_params(const char *proto, uint64_t key, uint32_t bits)
{
    SubGhzKeyParams p;
    memset(&p, 0, sizeof(p));
    strncpy(p.protocol, proto, sizeof(p.protocol) - 1);
    p.key_value = key;
    p.bit_count = bits;
    return p;
}

/* ===================================================================
 * Catalog lookup
 * =================================================================== */

void test_find_by_name_exact(void)
{
    TEST_ASSERT_EQUAL(SubGhzProtoPirate_KiaV7,
                      subghz_proto_pirate_find_by_name("Kia V7"));
    TEST_ASSERT_EQUAL(SubGhzProtoPirate_MazdaV0,
                      subghz_proto_pirate_find_by_name("Mazda V0"));
    TEST_ASSERT_EQUAL(SubGhzProtoPirate_HondaStatic,
                      subghz_proto_pirate_find_by_name("Honda Static"));
}

void test_find_by_name_case_insensitive(void)
{
    TEST_ASSERT_EQUAL(SubGhzProtoPirate_KiaV7,
                      subghz_proto_pirate_find_by_name("kia v7"));
    TEST_ASSERT_EQUAL(SubGhzProtoPirate_FordV0,
                      subghz_proto_pirate_find_by_name("FORD V0"));
}

void test_find_by_name_unknown(void)
{
    TEST_ASSERT_EQUAL(SubGhzProtoPirate_Unknown,
                      subghz_proto_pirate_find_by_name("NotAProtocol"));
    TEST_ASSERT_EQUAL(SubGhzProtoPirate_Unknown,
                      subghz_proto_pirate_find_by_name(NULL));
}

void test_is_supported(void)
{
    TEST_ASSERT_TRUE(subghz_proto_pirate_is_supported("Kia V7"));
    TEST_ASSERT_TRUE(subghz_proto_pirate_is_supported("kia v7"));
    TEST_ASSERT_FALSE(subghz_proto_pirate_is_supported("Toyota"));
    TEST_ASSERT_FALSE(subghz_proto_pirate_is_supported(NULL));
}

/* ===================================================================
 * Kia V7 encoder
 * =================================================================== */

void test_kia_v7_required_pairs(void)
{
    /* Key 0 has alternating-ish pattern after inversion; use all-zeros for
     * a deterministic symbol count. */
    SubGhzKeyParams params = make_params("Kia V7", 0, 64);
    /* Upstream sends a 319-pair first pass and a 15-pair tail pass;
     * worst-case Manchester symbol count per pass is 2 per bit + markers. */
    const uint32_t expect = (319u + 1u + (64u * 2u) + 2u) +
                            (15u + 1u + (64u * 2u) + 2u);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(
        expect, subghz_proto_pirate_required_pairs(&params, 1));
    TEST_ASSERT_EQUAL_UINT32(subghz_proto_pirate_required_pairs(&params, 1) * 3,
                             subghz_proto_pirate_required_pairs(&params, 3));
}

void test_kia_v7_encode_structure(void)
{
    /* Build a known key from fields:
     * header=0xB3, counter=0x1234, serial=0x0ABCDEF, button=0x2
     * key_value layout: header<<56 | counter<<16 | serial<<4 | button
     */
    uint64_t key_value = ((uint64_t)0xB3 << 56) |
                         ((uint64_t)0x1234 << 16) |
                         ((uint64_t)0x0ABCDEF << 4) |
                         0x2ULL;
    SubGhzKeyParams params = make_params("Kia V7", key_value, 64);

    SubGhzRawPair out[1000];
    uint32_t count = subghz_proto_pirate_encode(&params, out, 1000, 1);
    TEST_ASSERT_GREATER_THAN_UINT32(0, count);

    /* First 319 pairs: preamble [HIGH 250, LOW 250] */
    for (uint32_t i = 0; i < 319; i++) {
        TEST_ASSERT_EQUAL_UINT32(250, out[i].high_us);
        TEST_ASSERT_EQUAL_UINT32(250, out[i].low_us);
    }

    /* Pair 319: extra sync-high [250,250] */
    TEST_ASSERT_EQUAL_UINT32(250, out[319].high_us);
    TEST_ASSERT_EQUAL_UINT32(250, out[319].low_us);

    /* Data section follows until trailing+gap. */
}

void test_kia_v7_repetitions(void)
{
    SubGhzKeyParams params = make_params("Kia V7", 0xB300000000000002ULL, 64);
    SubGhzRawPair out[2000];
    uint32_t count = subghz_proto_pirate_encode(&params, out, 2000, 2);
    uint32_t per_rep = count / 2;
    TEST_ASSERT_GREATER_THAN_UINT32(0, per_rep);

    /* Each repetition should start with the same preamble */
    TEST_ASSERT_EQUAL_UINT32(250, out[0].high_us);
    TEST_ASSERT_EQUAL_UINT32(250, out[per_rep].high_us);
}

void test_kia_v7_buffer_overflow(void)
{
    SubGhzKeyParams params = make_params("Kia V7", 0xB300000000000002ULL, 64);
    SubGhzRawPair out[100]; /* too small for one rep (896 pairs) */
    uint32_t count = subghz_proto_pirate_encode(&params, out, 100, 1);
    TEST_ASSERT_EQUAL_UINT32(0, count);
}

void test_unsupported_protocol_returns_zero(void)
{
    SubGhzKeyParams params = make_params("ToyotaProtoPirate", 0x12345678ULL, 32);
    SubGhzRawPair out[100];
    TEST_ASSERT_EQUAL_UINT32(0, subghz_proto_pirate_encode(&params, out, 100, 1));
    TEST_ASSERT_EQUAL_UINT32(0, subghz_proto_pirate_required_pairs(&params, 1));
}

void test_null_params(void)
{
    SubGhzRawPair out[100];
    TEST_ASSERT_EQUAL_UINT32(0, subghz_proto_pirate_encode(NULL, out, 100, 1));
    TEST_ASSERT_EQUAL_UINT32(0, subghz_proto_pirate_required_pairs(NULL, 1));
}

/* ===================================================================
 * Tier-B encoders — smoke tests
 * =================================================================== */

void test_honda_v1_encode_nonzero(void)
{
    SubGhzRawPair out[2048];
    SubGhzKeyParams params = make_params("Honda V1", 0x123456789ABCDEF0ULL, 68);
    uint32_t req = subghz_proto_pirate_required_pairs(&params, 1);
    TEST_ASSERT_GREATER_THAN_UINT32(0, req);

    uint32_t count = subghz_proto_pirate_encode(&params, out, 2048, 1);
    TEST_ASSERT_GREATER_THAN_UINT32(0, count);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(req, count);
    TEST_ASSERT_EQUAL_UINT32(1000, out[0].high_us);
}

void test_honda_v2_encode_nonzero(void)
{
    SubGhzRawPair out[2048];
    SubGhzKeyParams params = make_params("Honda V2", 0, 81);
    params.serial = 0x123456;
    params.btn = 0x02; /* Lock */
    params.cnt = 0x100;

    uint32_t req = subghz_proto_pirate_required_pairs(&params, 1);
    TEST_ASSERT_GREATER_THAN_UINT32(0, req);

    uint32_t count = subghz_proto_pirate_encode(&params, out, 2048, 1);
    TEST_ASSERT_GREATER_THAN_UINT32(0, count);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(req, count);
    /* Preamble starts with short HIGH/short LOW */
    TEST_ASSERT_EQUAL_UINT32(250, out[0].high_us);
    TEST_ASSERT_EQUAL_UINT32(250, out[0].low_us);
}

void test_ford_v2_encode_nonzero(void)
{
    SubGhzRawPair out[4096];
    SubGhzKeyParams params = make_params("Ford V2", 0, 104);
    /* Default sync word will be filled in; extra holds tail bytes */
    params.extra[0] = 0x12;
    params.extra[1] = 0x34;
    params.extra[2] = 0x56;
    params.extra[3] = 0x78;
    params.extra[4] = 0x9A;

    uint32_t req = subghz_proto_pirate_required_pairs(&params, 1);
    TEST_ASSERT_GREATER_THAN_UINT32(0, req);

    uint32_t count = subghz_proto_pirate_encode(&params, out, 4096, 1);
    TEST_ASSERT_GREATER_THAN_UINT32(0, count);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(req, count);
    /* Preamble starts LOW then HIGH */
    TEST_ASSERT_EQUAL_UINT32(0, out[0].high_us);
    TEST_ASSERT_EQUAL_UINT32(200, out[0].low_us);
}

void test_ford_v1_encode_nonzero(void)
{
    SubGhzRawPair out[8192];
    SubGhzKeyParams params = make_params("Ford V1", 0, 136);
    params.serial = 0x12345678;
    params.btn = 0x02;
    params.cnt = 0x12345;

    uint32_t req = subghz_proto_pirate_required_pairs(&params, 1);
    TEST_ASSERT_GREATER_THAN_UINT32(0, req);

    uint32_t count = subghz_proto_pirate_encode(&params, out, 8192, 1);
    TEST_ASSERT_GREATER_THAN_UINT32(0, count);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(req, count);
    /* Preamble starts with long HIGH/long LOW */
    TEST_ASSERT_EQUAL_UINT32(130, out[0].high_us);
}

static uint16_t test_pp_ford_v1_crc16(const uint8_t *raw)
{
    uint16_t remainder = 0;
    for (uint8_t byte = 3; byte < 15; byte++)
    {
        remainder ^= (uint16_t)raw[byte] << 8;
        for (uint8_t bit = 0; bit < 8; bit++)
            remainder = (remainder & 0x8000u)
                            ? (uint16_t)((remainder << 1) ^ 0x1021u)
                            : (uint16_t)(remainder << 1);
    }
    return remainder;
}

void test_ford_v1_encoder_accepts_natural_key_layout(void)
{
    uint8_t expected[17] = {
        0x00, 0x00, 0x00, 0x12, 0x08, 0x56, 0x78,
        0x22, 0x04, 0x66, 0x30, 0x10, 0x26, 0x30, 0x00,
        0x00, 0x00,
    };
    const uint16_t crc = test_pp_ford_v1_crc16(expected);
    expected[15] = crc >> 8;
    expected[16] = crc;

    SubGhzRawPair out[8192];
    SubGhzKeyParams params = make_params("Ford V1", 0x12085678ULL, 136);
    for (uint8_t i = 0; i < 8; i++)
        params.extra[i] = expected[7u + i];
    params.extra[8] = expected[15];
    params.extra[9] = expected[16];

    const uint32_t count = subghz_proto_pirate_encode(&params, out, 8192, 1);
    TEST_ASSERT_GREATER_THAN_UINT32(0, count);

    uint8_t actual[17] = {0};
    for (uint16_t bit = 0; bit < 136; bit++)
    {
        const SubGhzRawPair *pair = &out[401u + bit * 2u];
        TEST_ASSERT_TRUE((pair->high_us == 65 && pair->low_us == 0) ||
                         (pair->high_us == 0 && pair->low_us == 65));
        TEST_ASSERT_TRUE((pair[1].high_us == 65 && pair[1].low_us == 0) ||
                         (pair[1].high_us == 0 && pair[1].low_us == 65));
        if (pair->high_us == 65)
            actual[bit >> 3] |= (uint8_t)(1u << (7u - (bit & 7u)));
    }
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, actual, 17);
}

void test_kia_v3_encode_nonzero(void)
{
    SubGhzRawPair out[2048];
    SubGhzKeyParams params = make_params("Kia V3/V4", 0, 68);
    params.serial = 0x0123456;
    params.btn = 0x02;
    params.cnt = 0x1234;

    uint32_t req = subghz_proto_pirate_required_pairs(&params, 1);
    TEST_ASSERT_GREATER_THAN_UINT32(0, req);

    uint32_t count = subghz_proto_pirate_encode(&params, out, 2048, 1);
    TEST_ASSERT_GREATER_THAN_UINT32(0, count);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(req, count);
    /* Preamble starts HIGH for V3 */
    TEST_ASSERT_GREATER_THAN_UINT32(0, out[0].high_us);
}

void test_kia_v4_encode_nonzero(void)
{
    SubGhzRawPair out[2048];
    SubGhzKeyParams params = make_params("Kia V4", 0, 68);
    params.serial = 0x0123456;
    params.btn = 0x02;
    params.cnt = 0x1234;

    uint32_t req = subghz_proto_pirate_required_pairs(&params, 1);
    TEST_ASSERT_GREATER_THAN_UINT32(0, req);

    uint32_t count = subghz_proto_pirate_encode(&params, out, 2048, 1);
    TEST_ASSERT_GREATER_THAN_UINT32(0, count);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(req, count);
    /* Preamble starts LOW for V4 */
    TEST_ASSERT_EQUAL_UINT32(0, out[0].high_us);
}

void test_kia_v5_encode_nonzero(void)
{
    SubGhzRawPair out[2048];
    SubGhzKeyParams params = make_params("Kia V5", 0, 67);
    params.serial = 0x0123456;
    params.btn = 0x02;
    params.cnt = 0x1234;

    uint32_t req = subghz_proto_pirate_required_pairs(&params, 1);
    TEST_ASSERT_GREATER_THAN_UINT32(0, req);

    uint32_t count = subghz_proto_pirate_encode(&params, out, 2048, 1);
    TEST_ASSERT_GREATER_THAN_UINT32(0, count);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(req, count);
    /* Preamble starts HIGH */
    TEST_ASSERT_GREATER_THAN_UINT32(0, out[0].high_us);
}

void test_fiat_v1_encode_nonzero(void)
{
    SubGhzRawPair out[256];
    SubGhzKeyParams params = make_params("Fiat V1", 0, 104);
    params.serial = 0x12345678;
    params.btn = 0x02;
    params.cnt = 0x123;

    uint32_t req = subghz_proto_pirate_required_pairs(&params, 1);
    TEST_ASSERT_GREATER_THAN_UINT32(0, req);

    uint32_t count = subghz_proto_pirate_encode(&params, out, 256, 1);
    TEST_ASSERT_GREATER_THAN_UINT32(0, count);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(req, count);
    /* Lead-in pulse */
    TEST_ASSERT_EQUAL_UINT32(2033, out[0].high_us);
}

/* ===================================================================
 * Tier-A encoders — smoke tests
 * =================================================================== */

static void tier_a_smoke(const char *proto, uint32_t serial, uint8_t btn,
                         uint32_t cnt, uint32_t out_size)
{
    SubGhzRawPair *out = calloc(out_size, sizeof(SubGhzRawPair));
    TEST_ASSERT_NOT_NULL(out);
    SubGhzKeyParams params = make_params(proto, 0, 64);
    params.serial = serial;
    params.btn = btn;
    params.cnt = cnt;

    uint32_t req = subghz_proto_pirate_required_pairs(&params, 1);
    TEST_ASSERT_GREATER_THAN_UINT32(0, req);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(out_size, req);

    uint32_t count = subghz_proto_pirate_encode(&params, out, out_size, 1);
    TEST_ASSERT_GREATER_THAN_UINT32(0, count);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(req, count);
    free(out);
}

void test_ford_v0_encode_nonzero(void)
{
    tier_a_smoke("Ford V0", 0x12345678, 0x02, 0x1234, 8192);
}

void test_mazda_v0_encode_nonzero(void)
{
    tier_a_smoke("Mazda V0", 0x12345678, 0x02, 0x12345, 4096);
}

void test_honda_static_encode_nonzero(void)
{
    tier_a_smoke("Honda Static", 0x0ABCDEF, 0x02, 0x1234, 2048);
}

void test_kia_v0_encode_nonzero(void)
{
    tier_a_smoke("Kia V0", 0x0123456, 0x02, 0x1234, 8192);
}

/* ===================================================================
 * Kia V0 wire format — the encoder must emit the upstream ProtoPirate
 * waveform: symmetric PWM pairs (bit 1 = 500/500, bit 0 = 250/250),
 * CRC-8 poly 0x7F over bits 8..55, 1500/1500 copy separator and a lone
 * 1000 us high commit gap at the end of a Kia frame.
 * =================================================================== */

static uint64_t kia_v0_recover_data(const SubGhzRawPair *p, uint32_t first,
                                    uint8_t bits)
{
    uint64_t raw = 0;
    for (uint32_t i = 0; i < bits; i++)
    {
        const SubGhzRawPair *hi = &p[first + 2u * i];
        const SubGhzRawPair *lo = &p[first + 2u * i + 1u];
        TEST_ASSERT_EQUAL_UINT32(0, hi->low_us);
        TEST_ASSERT_EQUAL_UINT32(0, lo->high_us);
        TEST_ASSERT_EQUAL_UINT32(hi->high_us, lo->low_us);
        raw <<= 1;
        if (hi->high_us == 500u)
            raw |= 1u;
        else
            TEST_ASSERT_EQUAL_UINT32(250u, hi->high_us);
    }
    return raw;
}

static uint8_t kia_v0_test_crc8(const uint8_t *buf, size_t len)
{
    /* Independent CRC-8 poly 0x7F init 0x00 (MSB-first). */
    uint8_t crc = 0;
    for (size_t i = 0; i < len; i++)
    {
        crc ^= buf[i];
        for (uint8_t b = 0; b < 8; b++)
            crc = (crc & 0x80u) ? (uint8_t)((crc << 1) ^ 0x7Fu)
                                : (uint8_t)(crc << 1);
    }
    return crc;
}

void test_kia_v0_encode_wire_format(void)
{
    SubGhzKeyParams params = make_params("Kia V0", 0, 61);
    params.serial = 0x0123456;
    params.btn = 0x02;
    params.cnt = 0x1234;

    SubGhzRawPair out[1048];
    const uint32_t count = subghz_proto_pirate_encode(&params, out, 1048, 1);
    /* sync(2) + 638 preamble + 122 data + 2 separator + 30 tail + 122 + 1 */
    TEST_ASSERT_EQUAL_UINT32(917, count);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(
        917u, subghz_proto_pirate_required_pairs(&params, 1));

    /* Sync */
    TEST_ASSERT_EQUAL_UINT32(750, out[0].high_us);
    TEST_ASSERT_EQUAL_UINT32(0, out[0].low_us);
    TEST_ASSERT_EQUAL_UINT32(0, out[1].high_us);
    TEST_ASSERT_EQUAL_UINT32(750, out[1].low_us);

    /* 319 short preamble pairs = 638 split entries */
    for (uint32_t i = 2; i < 640; i += 2)
    {
        TEST_ASSERT_EQUAL_UINT32(250, out[i].high_us);
        TEST_ASSERT_EQUAL_UINT32(0, out[i].low_us);
        TEST_ASSERT_EQUAL_UINT32(0, out[i + 1].high_us);
        TEST_ASSERT_EQUAL_UINT32(250, out[i + 1].low_us);
    }

    /* 61-bit data word, symmetric pairs, valid wire CRC */
    const uint64_t raw = kia_v0_recover_data(out, 640, 61);
    TEST_ASSERT_EQUAL_UINT64(0x0Fu, (raw >> 56) & 0xFFu);
    TEST_ASSERT_EQUAL_UINT32(0x0123456u, (uint32_t)((raw >> 12) & 0x0FFFFFFFu));
    TEST_ASSERT_EQUAL_UINT32(0x02u, (uint32_t)((raw >> 8) & 0x0Fu));
    TEST_ASSERT_EQUAL_UINT32(0x1234u, (uint32_t)((raw >> 40) & 0xFFFFu));
    uint8_t crc_buf[6];
    for (uint8_t i = 0; i < 6; i++)
        crc_buf[i] = (uint8_t)(raw >> (48u - i * 8u));
    TEST_ASSERT_EQUAL_UINT8(kia_v0_test_crc8(crc_buf, 6),
                            (uint8_t)(raw & 0xFFu));

    /* 1500/1500 copy separator */
    TEST_ASSERT_EQUAL_UINT32(1500, out[762].high_us);
    TEST_ASSERT_EQUAL_UINT32(0, out[762].low_us);
    TEST_ASSERT_EQUAL_UINT32(0, out[763].high_us);
    TEST_ASSERT_EQUAL_UINT32(1500, out[763].low_us);

    /* Tail copy: 15 pairs then the same 61-bit word */
    const uint64_t tail = kia_v0_recover_data(out, 794, 61);
    TEST_ASSERT_EQUAL_UINT64(raw, tail);

    /* Lone 1000 us high commit gap ends the frame */
    TEST_ASSERT_EQUAL_UINT32(1000, out[916].high_us);
    TEST_ASSERT_EQUAL_UINT32(0, out[916].low_us);
}

void test_kia_v0_encode_suzuki_wire_format(void)
{
    SubGhzKeyParams params = make_params("Kia V0", 0, 64);
    params.serial = 0x0BADC0D;
    params.btn = 0x03;
    params.cnt = 0x4711;
    params.extra[0] = 2; /* Suzuki */

    SubGhzRawPair out[1048];
    const uint32_t count = subghz_proto_pirate_encode(&params, out, 1048, 1);
    /* 640 preamble + 128 data + 3 separator + 30 tail + 128 data */
    TEST_ASSERT_EQUAL_UINT32(929, count);

    const uint64_t raw = kia_v0_recover_data(out, 640, 64);
    TEST_ASSERT_EQUAL_UINT32(0xFu, (uint32_t)(raw >> 60));
    TEST_ASSERT_EQUAL_UINT32(0x0BADC0Du, (uint32_t)((raw >> 16) & 0x0FFFFFFFu));
    TEST_ASSERT_EQUAL_UINT32(0x03u, (uint32_t)((raw >> 12) & 0x0Fu));
    TEST_ASSERT_EQUAL_UINT32(0x4711u, (uint32_t)((raw >> 44) & 0xFFFFu));

    /* Shifted-layout CRC-8 (poly 0x7F) must validate. */
    const uint32_t lo = (uint32_t)raw;
    const uint32_t hi = (uint32_t)(raw >> 32);
    const uint32_t r3 = ((lo >> 16) | (hi << 16)) & ~0xF0000000u;
    const uint32_t r2 = (lo >> 12) & 0x0Fu;
    uint32_t r1 = (hi >> 12) & 0xFFFFu;
    r1 = ((r1 & 0xFFu) << 8) | ((r1 >> 8) & 0xFFu);
    const uint8_t buf[6] = {
        (uint8_t)(r1 & 0xFFu), (uint8_t)((r1 >> 8) & 0xFFu),
        (uint8_t)((r3 >> 20) & 0xFFu), (uint8_t)((r3 >> 12) & 0xFFu),
        (uint8_t)((r3 >> 4) & 0xFFu), (uint8_t)(((r3 << 4) | r2) & 0xFFu),
    };
    TEST_ASSERT_EQUAL_UINT8(kia_v0_test_crc8(buf, 6),
                            (uint8_t)((lo >> 4) & 0xFFu));

    /* Separator: low 2000, high 2000, low 250 */
    TEST_ASSERT_EQUAL_UINT32(0, out[768].high_us);
    TEST_ASSERT_EQUAL_UINT32(2000, out[768].low_us);
    TEST_ASSERT_EQUAL_UINT32(2000, out[769].high_us);
    TEST_ASSERT_EQUAL_UINT32(0, out[769].low_us);
    TEST_ASSERT_EQUAL_UINT32(0, out[770].high_us);
    TEST_ASSERT_EQUAL_UINT32(250, out[770].low_us);

    /* Tail copy repeats the word */
    const uint64_t tail = kia_v0_recover_data(out, 801, 64);
    TEST_ASSERT_EQUAL_UINT64(raw, tail);
}

void test_kia_v0_encode_honda_wire_format(void)
{
    SubGhzKeyParams params = make_params("Kia V0", 0, 72);
    params.serial = 0x0A1B2C;
    params.btn = 0x01;
    params.cnt = 0x4321;
    params.extra[0] = 3; /* Honda V0 */

    SubGhzRawPair out[256];
    const uint32_t count = subghz_proto_pirate_encode(&params, out, 256, 1);
    /* 80 short + 8 long preamble entries + 112 data + 1 commit gap */
    TEST_ASSERT_EQUAL_UINT32(201, count);

    /* 4 long pairs follow the 40 short pairs */
    for (uint32_t i = 80; i < 88; i += 2)
    {
        TEST_ASSERT_EQUAL_UINT32(500, out[i].high_us);
        TEST_ASSERT_EQUAL_UINT32(500, out[i + 1].low_us);
    }

    /* 56 wire bits transform back into a valid Honda key.  The top byte
     * (0x0F) rides the 4 long preamble pairs, so prepend it first. */
    const uint64_t wire = (0x0FuLL << 56) | kia_v0_recover_data(out, 88, 56);
    uint64_t key = 0;
    for (uint8_t i = 0; i < 8; i++)
    {
        uint8_t b = (uint8_t)(wire >> (56u - i * 8u));
        b = (uint8_t)(((b & 0xF0u) >> 4) | ((b & 0x0Fu) << 4));
        b = (uint8_t)(((b & 0xCCu) >> 2) | ((b & 0x33u) << 2));
        b = (uint8_t)(((b & 0xAAu) >> 1) | ((b & 0x55u) << 1));
        key = (key << 8) | b;
    }
    TEST_ASSERT_EQUAL_UINT64(0xF0u, (key >> 56) & 0xFFu);
    TEST_ASSERT_EQUAL_UINT32(0x0A1B2Cu,
                             ((uint32_t)((key >> 32) & 0xFFu) << 16) |
                                 ((uint32_t)((key >> 24) & 0xFFu) << 8) |
                                 (uint32_t)((key >> 16) & 0xFFu));
    TEST_ASSERT_EQUAL_UINT32(0x0Au, (uint32_t)((key >> 8) & 0x0Fu));

    /* Lone 1000 us high commit gap */
    TEST_ASSERT_EQUAL_UINT32(1000, out[200].high_us);
    TEST_ASSERT_EQUAL_UINT32(0, out[200].low_us);
}

void test_kia_v0_encode_mitsubishi_wire_format(void)
{
    SubGhzKeyParams params = make_params("Kia V0", 0, 61);
    params.serial = 0x0556677;
    params.btn = 0x02;
    params.cnt = 0x7777;
    params.extra[0] = 4; /* Mitsubishi */

    SubGhzRawPair out[1056];
    const uint32_t count = subghz_proto_pirate_encode(&params, out, 1056, 1);
    /* sync(2) + 638 + 122 + 2 + 160 tail + 122 + 2 terminator */
    TEST_ASSERT_EQUAL_UINT32(1048, count);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(
        subghz_proto_pirate_required_pairs(&params, 1), 1048);

    const uint64_t raw = kia_v0_recover_data(out, 640, 61);
    const uint64_t tail = kia_v0_recover_data(out, 924, 61);
    TEST_ASSERT_EQUAL_UINT64(raw, tail);

    /* Ends with the 1500/1500 terminator (not the Kia lone 1000). */
    TEST_ASSERT_EQUAL_UINT32(1500, out[1046].high_us);
    TEST_ASSERT_EQUAL_UINT32(0, out[1046].low_us);
    TEST_ASSERT_EQUAL_UINT32(0, out[1047].high_us);
    TEST_ASSERT_EQUAL_UINT32(1500, out[1047].low_us);
}

/* ===================================================================
 * Kia V1/V2 key-only counter fallback — the packed Key layout must
 * match what the RX decoders commit (split counter nibbles).
 * =================================================================== */

void test_kia_v1_key_only_matches_explicit_fields(void)
{
    const uint32_t serial = 0x12345678u;
    const uint8_t btn = 0x02u;
    const uint16_t cnt = 0x0ABCu;

    SubGhzKeyParams explicit_params = make_params("Kia V1", 0, 57);
    explicit_params.serial = serial;
    explicit_params.btn = btn;
    explicit_params.cnt = cnt;

    /* Decoder-committed layout: seed bit 56, serial bits 24..55, button
     * byte bits 16..23, counter low byte bits 8..15 + high nibble 4..7.
     * The CRC nibble is recomputed by the encoder, so 0 is fine here. */
    const uint64_t packed = (1uLL << 56) | ((uint64_t)serial << 24) |
                            ((uint64_t)btn << 16) |
                            ((uint64_t)(cnt & 0xFFu) << 8) |
                            ((uint64_t)((cnt >> 8) & 0x0Fu) << 4);
    SubGhzKeyParams key_only = make_params("Kia V1", packed, 57);

    SubGhzRawPair a[900], b[900];
    const uint32_t na = subghz_proto_pirate_encode(&explicit_params, a, 900, 1);
    const uint32_t nb = subghz_proto_pirate_encode(&key_only, b, 900, 1);
    TEST_ASSERT_GREATER_THAN_UINT32(0, na);
    TEST_ASSERT_EQUAL_UINT32(na, nb);
    TEST_ASSERT_EQUAL_MEMORY(a, b, na * sizeof(a[0]));
}

void test_kia_v2_key_only_matches_explicit_fields(void)
{
    const uint32_t serial = 0x0ABCDEF1u;
    const uint8_t btn = 0x03u;
    const uint16_t cnt = 0x0FEDu;

    SubGhzKeyParams explicit_params = make_params("Kia V2", 0, 53);
    explicit_params.serial = serial;
    explicit_params.btn = btn;
    explicit_params.cnt = cnt;

    /* Decoder-committed layout: seed bit 52, serial bits 20..51, button
     * nibble bits 16..19, counter low byte bits 8..15 + high nibble 4..7. */
    const uint64_t packed = (1uLL << 52) | ((uint64_t)serial << 20) |
                            ((uint64_t)btn << 16) |
                            ((uint64_t)(cnt & 0xFFu) << 8) |
                            ((uint64_t)(cnt >> 4) & 0xF0u);
    SubGhzKeyParams key_only = make_params("Kia V2", packed, 53);

    SubGhzRawPair a[1300], b[1300];
    const uint32_t na = subghz_proto_pirate_encode(&explicit_params, a, 1300, 1);
    const uint32_t nb = subghz_proto_pirate_encode(&key_only, b, 1300, 1);
    TEST_ASSERT_GREATER_THAN_UINT32(0, na);
    TEST_ASSERT_EQUAL_UINT32(na, nb);
    TEST_ASSERT_EQUAL_MEMORY(a, b, na * sizeof(a[0]));
}

/* ===================================================================
 * Kia V3/V4 — the encoder must encrypt with the manufacturer key
 * directly (upstream), sourced from extra[0..7] or the built-in
 * keystore injection (test stub: 0x0102030405060708).
 * =================================================================== */

static uint64_t bit_reverse64(uint64_t x)
{
    uint64_t r = 0;
    for (int i = 0; i < 64; i++)
        r = (r << 1) | ((x >> i) & 1uLL);
    return r;
}

static void kia_v3_v4_check_frame(const char *proto, bool v4)
{
    const uint32_t serial = 0x00ABCDE1u;
    const uint8_t btn = 0x02u;
    const uint16_t cnt = 0x1234u;

    SubGhzKeyParams params = make_params(proto, 0, 68);
    params.serial = serial;
    params.btn = btn;
    params.cnt = cnt;

    SubGhzRawPair out[200];
    const uint32_t count = subghz_proto_pirate_encode(&params, out, 200, 1);
    /* 12 preamble pairs + sync + 64 data bits + 4 CRC bits + end marker */
    TEST_ASSERT_EQUAL_UINT32(24 + 2 + 128 + 8 + 2, count);

    /* Recover the 64 emitted data bits (MSB first).  V3: pair = {d1,0},
     * {0,d2} with d1==400 => 1; V4: {0,d1},{d2,0} with d1==400 => 1. */
    uint64_t emitted = 0;
    for (uint32_t i = 0; i < 64; i++)
    {
        const SubGhzRawPair *e = &out[26 + 2 * i];
        uint32_t first = v4 ? e->low_us : e->high_us;
        emitted = (emitted << 1) | (first == 400u ? 1u : 0u);
    }

    const uint32_t plaintext = (cnt & 0xFFFFu) | ((serial & 0x3FFu) << 16) |
                               ((uint32_t)(btn & 0x0Fu) << 28);
    const uint32_t enc =
        keeloq_encrypt(plaintext, 0x0102030405060708ULL /* test stub key */);
    const uint64_t expected =
        bit_reverse64((((uint64_t)(serial & 0x0FFFFFFFu) |
                        ((uint64_t)(btn & 0x0Fu) << 28))
                       << 32) |
                      enc);
    TEST_ASSERT_EQUAL_UINT64(expected, emitted);
}

void test_kia_v3_encode_uses_mf_key_directly(void)
{
    kia_v3_v4_check_frame("Kia V3/V4", false);
}

void test_kia_v4_encode_uses_mf_key_directly(void)
{
    kia_v3_v4_check_frame("Kia V4", true);
}

void test_kia_v3_encode_builtin_key_fallback(void)
{
    /* extra[] carrying the key and zeroed extra[] (built-in fallback) must
     * produce byte-identical waveforms (test stub key 0x0102030405060708). */
    SubGhzKeyParams keyed = make_params("Kia V3/V4", 0, 68);
    keyed.serial = 0x00ABCDE1u;
    keyed.btn = 0x02u;
    keyed.cnt = 0x1234u;
    for (int i = 0; i < 8; i++)
        keyed.extra[i] = (uint8_t)(0x0102030405060708ULL >> (56 - i * 8));

    SubGhzKeyParams fallback = keyed;
    memset(fallback.extra, 0, sizeof(fallback.extra));

    SubGhzRawPair a[200], b[200];
    const uint32_t na = subghz_proto_pirate_encode(&keyed, a, 200, 1);
    const uint32_t nb = subghz_proto_pirate_encode(&fallback, b, 200, 1);
    TEST_ASSERT_GREATER_THAN_UINT32(0, na);
    TEST_ASSERT_EQUAL_UINT32(na, nb);
    TEST_ASSERT_EQUAL_MEMORY(a, b, na * sizeof(a[0]));
}

/* ===================================================================
 * Kia V7 — golden waveform: encode() output must be the merged-pulse
 * form of the exact stream the decoder suite proved decodable:
 * 319-pair + 15-pair passes, sync pair, Manchester of ~candidate.
 * =================================================================== */

static uint32_t s_v7_stream[1300];
static uint32_t s_v7_nstream;

static void v7_stream_push(bool high, uint32_t dur)
{
    if (s_v7_nstream && ((s_v7_stream[s_v7_nstream - 1] >> 31) == (uint32_t)high))
        s_v7_stream[s_v7_nstream - 1] += dur;
    else
        s_v7_stream[s_v7_nstream++] = dur | (high ? 0x80000000u : 0u);
}

static void v7_stream_pair(uint32_t high, uint32_t low)
{
    if (high) v7_stream_push(true, high);
    if (low) v7_stream_push(false, low);
}

static void v7_stream_symbol(ManchesterEncoderResult r)
{
    const bool level = (r == ManchesterEncoderResultLongHigh) ||
                       (r == ManchesterEncoderResultShortHigh);
    const bool is_long = (r == ManchesterEncoderResultLongLow) ||
                         (r == ManchesterEncoderResultLongHigh);
    v7_stream_push(level, is_long ? 500u : 250u);
}

void test_kia_v7_encoder_golden_waveform(void)
{
    const uint32_t serial = 0x00ABCDE1u;
    const uint8_t btn = 0x02u;
    const uint16_t cnt = 0x1234u;

    SubGhzKeyParams params = make_params("Kia V7", 0, 64);
    params.serial = serial;
    params.btn = btn;
    params.cnt = cnt;

    SubGhzRawPair out[1200];
    const uint32_t npairs = subghz_proto_pirate_encode(&params, out, 1200, 1);
    TEST_ASSERT_GREATER_THAN_UINT32(0, npairs);

    /* Actual: expand the pairs to a merged level stream. */
    s_v7_nstream = 0;
    for (uint32_t i = 0; i < npairs; i++)
        v7_stream_pair(out[i].high_us, out[i].low_us);
    const uint32_t actual_n = s_v7_nstream;
    uint32_t actual[1300];
    memcpy(actual, s_v7_stream, actual_n * sizeof(actual[0]));

    /* Expected: candidate word (decoder-committed layout) and its
     * on-air inverse per the M1 Manchester encoder. */
    uint8_t b[8];
    b[0] = 0x4Cu;
    b[1] = (uint8_t)(cnt >> 8);
    b[2] = (uint8_t)cnt;
    b[3] = (uint8_t)((serial >> 20) & 0xFFu);
    b[4] = (uint8_t)((serial >> 12) & 0xFFu);
    b[5] = (uint8_t)((serial >> 4) & 0xFFu);
    b[6] = (uint8_t)(((serial & 0x0Fu) << 4) | (btn & 0x0Fu));
    b[7] = crc8(b, 7, 0x7Fu, 0x4Cu);
    uint64_t candidate = 0;
    for (int i = 0; i < 8; i++)
        candidate = (candidate << 8) | b[i];
    const uint64_t wire = ~candidate;

    s_v7_nstream = 0;
    for (uint8_t pass = 0; pass < 2; pass++)
    {
        const uint32_t preamble = (pass == 0) ? 319u : 15u;
        for (uint32_t p = 0; p < preamble; p++)
            v7_stream_pair(250, 250);
        v7_stream_pair(250, 250); /* extra sync-high pair */

        ManchesterEncoderState state;
        manchester_encoder_reset(&state);
        for (int bit = 63; bit >= 0; bit--)
        {
            const bool bv = ((wire >> bit) & 1uLL) != 0uLL;
            ManchesterEncoderResult result;
            while (!manchester_encoder_advance(&state, bv, &result))
                v7_stream_symbol(result);
            v7_stream_symbol(result);
        }
        v7_stream_symbol(manchester_encoder_finish(&state));
        v7_stream_pair(250, 2000); /* trailing high + tail gap */
    }

    TEST_ASSERT_EQUAL_UINT32(s_v7_nstream, actual_n);
    TEST_ASSERT_EQUAL_UINT32_ARRAY(s_v7_stream, actual, actual_n);
}


/* ===================================================================
 * Kia V5 — built-in KIA_KEY4 fallback must produce byte-identical
 * output to the same key passed through extra[0..7].
 * =================================================================== */

void test_kia_v5_encode_builtin_key_fallback(void)
{
    SubGhzKeyParams keyed = make_params("Kia V5", 0, 64);
    keyed.serial = 0x0123456u;
    keyed.btn = 0x02u;
    keyed.cnt = 0x1234u;
    const uint64_t k = 0xA1B2C3D4E5F60718ULL; /* matches the test stub */
    for (int i = 0; i < 8; i++)
        keyed.extra[i] = (uint8_t)(k >> (56 - i * 8));

    SubGhzKeyParams fallback = keyed;
    memset(fallback.extra, 0, sizeof(fallback.extra));

    SubGhzRawPair a[600], b[600];
    const uint32_t na = subghz_proto_pirate_encode(&keyed, a, 600, 1);
    const uint32_t nb = subghz_proto_pirate_encode(&fallback, b, 600, 1);
    TEST_ASSERT_GREATER_THAN_UINT32(0, na);
    TEST_ASSERT_EQUAL_UINT32(na, nb);
    TEST_ASSERT_EQUAL_MEMORY(a, b, na * sizeof(a[0]));

    /* Recover the 64-bit wire word (Manchester: bit 1 = low->high) and
     * verify fields plus the counter mixer round trip through the real
     * decoder-side mixer inverse linked into this target. */
    uint64_t wire = 0;
    for (uint32_t i = 0; i < 64; i++)
        wire = (wire << 1) | (a[404 + 2 * i].low_us == 400u ? 1u : 0u);
    uint64_t yek = 0;
    for (int i = 0; i < 8; i++)
    {
        uint8_t byte = (uint8_t)(wire >> (i * 8));
        byte = (uint8_t)(((byte & 0xF0u) >> 4) | ((byte & 0x0Fu) << 4));
        byte = (uint8_t)(((byte & 0xCCu) >> 2) | ((byte & 0x33u) << 2));
        byte = (uint8_t)(((byte & 0xAAu) >> 1) | ((byte & 0x55u) << 1));
        yek |= ((uint64_t)byte << ((7 - i) * 8));
    }
    TEST_ASSERT_EQUAL_UINT32(0x0123456u, (uint32_t)((yek >> 32) & 0x0FFFFFFFu));
    TEST_ASSERT_EQUAL_UINT32(0x02u, (uint32_t)((yek >> 60) & 0x0Fu));
    TEST_ASSERT_EQUAL_UINT32(0x1234u,
                             m1_kia_v5_mixer_decode((uint32_t)(yek & 0xFFFFFFFFu), k));
}

void test_kia_v1_encode_nonzero(void)
{
    tier_a_smoke("Kia V1", 0x12345678, 0x02, 0x1234, 4096);
}

void test_kia_v2_encode_nonzero(void)
{
    tier_a_smoke("Kia V2", 0x12345678, 0x02, 0x1234, 4096);
}

void test_renault_v0_encode_nonzero(void)
{
    tier_a_smoke("Renault V0", 0x12345678, 0x0A, 0x12, 4096);
}

void test_chrysler_v0_encode_nonzero(void)
{
    SubGhzRawPair out[4096];
    SubGhzKeyParams params = make_params("Chrysler V0", 0x03, 64);
    params.btn = 0x01;
    params.cnt = 0x10;
    for (size_t i = 0; i < 9; i++) params.extra[i] = (uint8_t)(0xA0 + i);

    uint32_t req = subghz_proto_pirate_required_pairs(&params, 1);
    TEST_ASSERT_GREATER_THAN_UINT32(0, req);

    uint32_t count = subghz_proto_pirate_encode(&params, out, 4096, 1);
    TEST_ASSERT_GREATER_THAN_UINT32(0, count);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(req, count);
}

void test_fiat_v0_encode_nonzero(void)
{
    SubGhzRawPair out[8192];
    SubGhzKeyParams params = make_params("Fiat V0", 0x123456789ABCDEF0ULL, 64);
    params.btn = 0x02;

    uint32_t req = subghz_proto_pirate_required_pairs(&params, 1);
    TEST_ASSERT_GREATER_THAN_UINT32(0, req);

    uint32_t count = subghz_proto_pirate_encode(&params, out, 8192, 1);
    TEST_ASSERT_GREATER_THAN_UINT32(0, count);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(req, count);
}

void test_subaru_encode_nonzero(void)
{
    tier_a_smoke("Subaru", 0xABCDEF, 0x02, 0x1234, 4096);
}

void test_star_line_encode_nonzero(void)
{
    SubGhzRawPair out[2048];
    SubGhzKeyParams params = make_params("Star Line", 0, 64);
    params.serial = 0x12345678;
    params.btn = 0x02;
    params.cnt = 0x1234;

    uint32_t req = subghz_proto_pirate_required_pairs(&params, 1);
    TEST_ASSERT_GREATER_THAN_UINT32(0, req);

    uint32_t count = subghz_proto_pirate_encode(&params, out, 2048, 1);
    TEST_ASSERT_GREATER_THAN_UINT32(0, count);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(req, count);
}

/* ===================================================================
 * Runner
 * =================================================================== */

/* ===================================================================
 * Kia V6 (AES, keystore-gated) — full pipeline chain:
 * params → subghz_proto_pirate_encode() → pairs → pulse feed → the real
 * subghz_decode_kia_v6() must recover the fields.
 * =================================================================== */

void test_kia_v6_required_pairs(void)
{
    SubGhzKeyParams params =
        make_params("Kia V6", ((uint64_t)0x123456 << 32) | 0x003CF089u, 144);
    params.btn = 2;
    params.extra[0] = 0x34;

    SubGhzRawPair out[1000];
    const uint32_t count = subghz_proto_pirate_encode(&params, out, 1000, 1);
    TEST_ASSERT_GREATER_THAN_UINT32(0, count);
    /* required_pairs is an allocation bound: >= actual, per repetition. */
    const uint32_t bound1 = subghz_proto_pirate_required_pairs(&params, 1);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(count, bound1);
    /* And the required-size buffer is sufficient for repetitions. */
    const uint32_t need = subghz_proto_pirate_required_pairs(&params, 3);
    SubGhzRawPair *big = malloc(need * sizeof(*big));
    TEST_ASSERT_NOT_NULL(big);
    const uint32_t n3 = subghz_proto_pirate_encode(&params, big, need, 3);
    TEST_ASSERT_EQUAL_UINT32(count * 3u, n3);
    free(big);
}

void test_kia_v6_encode_matches_upload_builder(void)
{
    const uint32_t serial = 0x123456u;
    const uint32_t cnt = 0x003CF089u;
    SubGhzKeyParams params =
        make_params("Kia V6", ((uint64_t)serial << 32) | cnt, 144);
    params.btn = 2;
    params.extra[0] = 0x34;

    SubGhzRawPair pairs[1000];
    const uint32_t npairs = subghz_proto_pirate_encode(&params, pairs, 1000, 1);
    TEST_ASSERT_GREATER_THAN_UINT32(0, npairs);

    /* The pipeline output must be the merged-pair form of the upload
     * builder's waveform for the same frame (field mapping through
     * params -> frame -> upload -> pairs). */
    uint8_t frame[18];
    subghz_kia_v6_encrypt_frame(0x34u, serial, 2u, cnt + 1u,
                                m1_kia_v6_keys_builtin, frame);
    static subghz_upload_entry_t up[KIA_V6_ENC_UPLOAD_SIZE];
    const uint32_t n =
        subghz_kia_v6_build_upload(up, KIA_V6_ENC_UPLOAD_SIZE, frame);
    TEST_ASSERT_EQUAL_UINT32(KIA_V6_ENC_UPLOAD_SIZE, n);

    /* Rebuild merged durations from the upload and compare with pairs. */
    uint32_t merged[2048];
    uint32_t nm = 0;
    for (uint32_t i = 0; i < n; i++)
    {
        if (nm > 0 && (merged[nm - 1] & 0x80000000u) ==
                          (up[i].level ? 0x80000000u : 0u))
            merged[nm - 1] += up[i].duration;
        else
            merged[nm++] = up[i].duration |
                           (up[i].level ? 0x80000000u : 0u);
    }
    TEST_ASSERT_EQUAL_UINT32(nm % 2u, 0u);  /* must pair evenly */
    TEST_ASSERT_EQUAL_UINT32(nm / 2u, npairs);
    for (uint32_t i = 0; i < npairs; i++)
    {
        TEST_ASSERT_TRUE(merged[2 * i] & 0x80000000u);       /* high */
        TEST_ASSERT_FALSE(merged[2 * i + 1] & 0x80000000u);  /* low  */
        TEST_ASSERT_EQUAL_UINT32(merged[2 * i] & 0x7FFFFFFFu,
                                 pairs[i].high_us);
        TEST_ASSERT_EQUAL_UINT32(merged[2 * i + 1] & 0x7FFFFFFFu,
                                 pairs[i].low_us);
    }
}

void test_kia_v6_explicit_serial_and_packed_serial(void)
{
    const uint64_t packed_key = ((uint64_t)0x112233u << 32) | 0x1234u;
    SubGhzKeyParams packed = make_params("Kia V6", packed_key, 144);
    packed.btn = 2u;
    packed.cnt = 0x1234u;
    packed.cnt_present = true;
    packed.extra[0] = 0x34u;

    SubGhzKeyParams imported = packed;
    imported.serial = 0xA1B2C3u;
    imported.serial_present = true;

    SubGhzRawPair packed_pairs[1000], imported_pairs[1000];
    const uint32_t packed_count =
        subghz_proto_pirate_encode(&packed, packed_pairs, 1000u, 1u);
    const uint32_t imported_count =
        subghz_proto_pirate_encode(&imported, imported_pairs, 1000u, 1u);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, packed_count);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, imported_count);
    TEST_ASSERT_TRUE(packed_count != imported_count ||
                     memcmp(packed_pairs, imported_pairs,
                            (packed_count < imported_count ? packed_count :
                             imported_count) * sizeof(*packed_pairs)) != 0);
}

void test_vag_upstream_buttons_normalize_and_replay_counter_advances(void)
{
    const uint32_t serial = 0x12345678u;
    const uint32_t source_counter = 0x005634u;
    const uint64_t type2_key = 0xC0AABBCCDDEEFF00ULL;
    SubGhzRawPair first[800], second[800], repeated[1600];
    SubGhzKeyParams params = make_params("VAG", type2_key, 96u);
    params.btn = 0x10u;
    params.cnt = source_counter;
    params.cnt_present = true;
    params.extra[8] = (uint8_t)(serial >> 24);
    params.extra[9] = (uint8_t)(serial >> 16);
    params.extra[10] = (uint8_t)(serial >> 8);
    params.extra[11] = (uint8_t)serial;
    params.extra[12] = 2u;
    params.extra[13] = VAG_ENC_KEY_AUTO;

    const uint32_t first_count =
        subghz_proto_pirate_encode(&params, first, 800u, 1u);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, first_count);
    TEST_ASSERT_EQUAL_HEX8(0x10u, subghz_vag_button_byte(0x10u));

    params.btn = 1u;
    TEST_ASSERT_EQUAL_UINT32(
        first_count, subghz_proto_pirate_encode(&params, second, 800u, 1u));
    TEST_ASSERT_EQUAL_MEMORY(first, second, first_count * sizeof(*first));

    params.btn = 0x10u;
    TEST_ASSERT_EQUAL_UINT32(
        first_count * 2u,
        subghz_proto_pirate_encode(&params, repeated, 1600u, 2u));
    TEST_ASSERT_EQUAL_MEMORY(repeated, &repeated[first_count],
                             first_count * sizeof(*repeated));

    params.cnt = source_counter + 1u;
    const uint32_t next_count =
        subghz_proto_pirate_encode(&params, second, 800u, 1u);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, next_count);
    TEST_ASSERT_TRUE(first_count != next_count ||
                     memcmp(first, second,
                            (first_count < next_count ? first_count :
                             next_count) * sizeof(*first)) != 0);
}

void test_vag_passat_builds_type2_waveform(void)
{
    const uint32_t serial = 0x12345678u;
    SubGhzKeyParams passat = make_params("VAG", 0x00AABBCCDDEEFF00ULL, 96u);
    passat.btn = 1u;
    passat.cnt = 0x1234u;
    passat.cnt_present = true;
    passat.extra[8] = (uint8_t)(serial >> 24);
    passat.extra[9] = (uint8_t)(serial >> 16);
    passat.extra[10] = (uint8_t)(serial >> 8);
    passat.extra[11] = (uint8_t)serial;
    passat.extra[12] = 1u;
    passat.extra[13] = VAG_ENC_KEY_AUTO;

    SubGhzKeyParams type2 = passat;
    type2.key_value = passat.key_value;
    type2.extra[12] = 2u;

    SubGhzRawPair passat_pairs[800], type2_pairs[800];
    const uint32_t count =
        subghz_proto_pirate_encode(&passat, passat_pairs, 800u, 1u);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, count);
    TEST_ASSERT_EQUAL_UINT32(
        count, subghz_proto_pirate_encode(&type2, type2_pairs, 800u, 1u));
    TEST_ASSERT_EQUAL_MEMORY(passat_pairs, type2_pairs,
                             count * sizeof(*passat_pairs));
    TEST_ASSERT_EQUAL_UINT8(2u, subghz_vag_effective_type(1u, 0x00u));
}

void test_proto_pirate_counter_state_survives_capture_switches(void)
{
    subghz_proto_pirate_counter_state_t state = {0};
    TEST_ASSERT_EQUAL_UINT32(
        12u, subghz_proto_pirate_counter_get(
                 &state, "0:/SUBGHZ/a.sub", SubGhzProtoPirate_KiaV6, 12u,
                 0xAABBCCDDu));
    subghz_proto_pirate_counter_advance(&state, 0xFFFFFFFFu);
    TEST_ASSERT_EQUAL_UINT32(
        90u, subghz_proto_pirate_counter_get(
                 &state, "/SUBGHZ/b.sub", SubGhzProtoPirate_VAG, 90u,
                 0xAABBCCDDu));
    subghz_proto_pirate_counter_advance(&state, 0xFFFFFFu);
    TEST_ASSERT_EQUAL_UINT32(
        91u, subghz_proto_pirate_counter_get(
                 &state, "/SUBGHZ/b.sub", SubGhzProtoPirate_VAG, 90u,
                 0xAABBCCDDu));
    subghz_proto_pirate_counter_advance(&state, 0xFFFFFFu);
    TEST_ASSERT_EQUAL_UINT32(
        13u, subghz_proto_pirate_counter_get(
                 &state, "/SUBGHZ/a.sub", SubGhzProtoPirate_KiaV6, 12u,
                 0xAABBCCDDu));
    subghz_proto_pirate_counter_advance(&state, 0xFFFFFFFFu);
    TEST_ASSERT_EQUAL_UINT32(
        14u, subghz_proto_pirate_counter_get(
                 &state, "/SUBGHZ/a.sub", SubGhzProtoPirate_KiaV6, 12u,
                 0xAABBCCDDu));
    TEST_ASSERT_EQUAL_UINT32(
        92u, subghz_proto_pirate_counter_get(
                 &state, "/SUBGHZ/b.sub", SubGhzProtoPirate_VAG, 90u,
                 0xAABBCCDDu));
    TEST_ASSERT_EQUAL_UINT32(
        20u, subghz_proto_pirate_counter_get(
                 &state, "/SUBGHZ/a.sub", SubGhzProtoPirate_KiaV6, 20u,
                 0x11223344u));
}

void test_imported_kia_v6_serial_field_overrides_packed_key_serial(void)
{
    FILE *file = tmpfile();
    TEST_ASSERT_NOT_NULL(file);
    TEST_ASSERT_GREATER_THAN_INT(0, fputs("Serial: 10597059\n", file));
    rewind(file);
    char line[32];
    TEST_ASSERT_NOT_NULL(fgets(line, sizeof(line), file));
    fclose(file);

    SubGhzKeyParams m1 = make_params(
        "Kia V6", ((uint64_t)0x112233u << 32) | 0x1234u, 144u);
    m1.btn = 2u;
    m1.cnt = 0x1234u;
    m1.cnt_present = true;
    m1.extra[0] = 0x34u;
    SubGhzKeyParams imported = m1;
    TEST_ASSERT_TRUE(subghz_proto_pirate_parse_serial_line(line,
                                                          &imported.serial));
    imported.serial_present = true;

    SubGhzRawPair m1_pairs[1000], imported_pairs[1000];
    const uint32_t m1_count =
        subghz_proto_pirate_encode(&m1, m1_pairs, 1000u, 1u);
    const uint32_t imported_count =
        subghz_proto_pirate_encode(&imported, imported_pairs, 1000u, 1u);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, m1_count);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, imported_count);
    TEST_ASSERT_TRUE(m1_count != imported_count ||
                     memcmp(m1_pairs, imported_pairs,
                            (m1_count < imported_count ? m1_count :
                             imported_count) * sizeof(*m1_pairs)) != 0);
}

void test_vag_key2_parser_accepts_short_and_upstream_byte_lists(void)
{
    TEST_ASSERT_EQUAL_HEX16(0x4E2Au, subghz_vag_parse_key2("4E 2A"));
    TEST_ASSERT_EQUAL_HEX16(
        0x4E2Au, subghz_vag_parse_key2("00 00 00 00 00 00 4E 2A"));
    TEST_ASSERT_EQUAL_HEX16(0u, subghz_vag_parse_key2(NULL));
}

/* ===================================================================
 * VAG (type 2 = TEA, keystore-free path) — params → encode → pairs must
 * equal the merged-pair form of the upload builder's waveform.
 * =================================================================== */

void test_vag_encode_matches_upload_builder(void)
{
    const uint32_t serial = 0x12345678u;
    const uint32_t cnt = 0x005634u;
    const uint64_t orig_key1 = 0xC0AABBCCDDEEFF00ULL;

    SubGhzKeyParams params = make_params("VAG", orig_key1, 96);
    params.btn = 1;                       /* unlock */
    params.cnt = cnt;
    params.extra[8]  = (uint8_t)(serial >> 24);
    params.extra[9]  = (uint8_t)(serial >> 16);
    params.extra[10] = (uint8_t)(serial >> 8);
    params.extra[11] = (uint8_t)serial;
    params.extra[12] = 2u;                /* type 2 (TEA) */
    params.extra[13] = 0xFFu;             /* key auto (unused for type 2) */

    SubGhzRawPair pairs[400];
    const uint32_t npairs = subghz_proto_pirate_encode(&params, pairs, 400, 1);
    TEST_ASSERT_GREATER_THAN_UINT32(0, npairs);

    /* Counter auto-advances per encode (reference behaviour). */
    uint64_t key1 = 0;
    uint16_t key2 = 0;
    TEST_ASSERT_TRUE(subghz_vag_encrypt_frame(
        serial, (cnt + 1u) & 0xFFFFFFu, 1u, 2u, 0xC0u, 0xFFu,
        orig_key1, 0, NULL, false, &key1, &key2));

    static subghz_upload_entry_t up[VAG_ENC_UPLOAD_T12];
    const uint32_t n =
        subghz_vag_build_upload(up, VAG_ENC_UPLOAD_T12, 2u, key1, key2);
    TEST_ASSERT_EQUAL_UINT32(VAG_ENC_UPLOAD_T12, n);

    /* Rebuild merged durations from the upload and compare with pairs. */
    uint32_t merged[VAG_ENC_UPLOAD_T12];
    uint32_t nm = 0;
    for (uint32_t i = 0; i < n; i++)
    {
        if (nm > 0 && (merged[nm - 1] & 0x80000000u) ==
                          (up[i].level ? 0x80000000u : 0u))
            merged[nm - 1] += up[i].duration;
        else
            merged[nm++] = up[i].duration |
                           (up[i].level ? 0x80000000u : 0u);
    }
    TEST_ASSERT_EQUAL_UINT32(0u, nm % 2u);
    TEST_ASSERT_EQUAL_UINT32(nm / 2u, npairs);
    for (uint32_t i = 0; i < npairs; i++)
    {
        TEST_ASSERT_TRUE(merged[2 * i] & 0x80000000u);
        TEST_ASSERT_FALSE(merged[2 * i + 1] & 0x80000000u);
        TEST_ASSERT_EQUAL_UINT32(merged[2 * i] & 0x7FFFFFFFu,
                                 pairs[i].high_us);
        TEST_ASSERT_EQUAL_UINT32(merged[2 * i + 1] & 0x7FFFFFFFu,
                                 pairs[i].low_us);
    }
}

int main(void)
{    UNITY_BEGIN();

    RUN_TEST(test_find_by_name_exact);
    RUN_TEST(test_find_by_name_case_insensitive);
    RUN_TEST(test_find_by_name_unknown);
    RUN_TEST(test_is_supported);

    RUN_TEST(test_kia_v7_required_pairs);
    RUN_TEST(test_kia_v7_encode_structure);
    RUN_TEST(test_kia_v7_repetitions);
    RUN_TEST(test_kia_v7_buffer_overflow);
    RUN_TEST(test_unsupported_protocol_returns_zero);
    RUN_TEST(test_null_params);

    RUN_TEST(test_honda_v1_encode_nonzero);
    RUN_TEST(test_honda_v2_encode_nonzero);
    RUN_TEST(test_ford_v2_encode_nonzero);
    RUN_TEST(test_ford_v1_encode_nonzero);
    RUN_TEST(test_ford_v1_encoder_accepts_natural_key_layout);
    RUN_TEST(test_kia_v3_encode_nonzero);
    RUN_TEST(test_kia_v4_encode_nonzero);
    RUN_TEST(test_kia_v5_encode_nonzero);
    RUN_TEST(test_fiat_v1_encode_nonzero);

    RUN_TEST(test_kia_v6_encode_matches_upload_builder);
    RUN_TEST(test_kia_v6_explicit_serial_and_packed_serial);
    RUN_TEST(test_kia_v6_required_pairs);
    RUN_TEST(test_vag_encode_matches_upload_builder);
    RUN_TEST(test_vag_upstream_buttons_normalize_and_replay_counter_advances);
    RUN_TEST(test_vag_passat_builds_type2_waveform);
    RUN_TEST(test_proto_pirate_counter_state_survives_capture_switches);
    RUN_TEST(test_imported_kia_v6_serial_field_overrides_packed_key_serial);
    RUN_TEST(test_vag_key2_parser_accepts_short_and_upstream_byte_lists);

    RUN_TEST(test_ford_v0_encode_nonzero);
    RUN_TEST(test_mazda_v0_encode_nonzero);
    RUN_TEST(test_honda_static_encode_nonzero);
    RUN_TEST(test_kia_v0_encode_nonzero);
    RUN_TEST(test_kia_v0_encode_wire_format);
    RUN_TEST(test_kia_v0_encode_suzuki_wire_format);
    RUN_TEST(test_kia_v0_encode_honda_wire_format);
    RUN_TEST(test_kia_v0_encode_mitsubishi_wire_format);
    RUN_TEST(test_kia_v1_key_only_matches_explicit_fields);
    RUN_TEST(test_kia_v2_key_only_matches_explicit_fields);
    RUN_TEST(test_kia_v3_encode_uses_mf_key_directly);
    RUN_TEST(test_kia_v4_encode_uses_mf_key_directly);
    RUN_TEST(test_kia_v3_encode_builtin_key_fallback);
    RUN_TEST(test_kia_v5_encode_builtin_key_fallback);
    RUN_TEST(test_kia_v7_encoder_golden_waveform);
    RUN_TEST(test_kia_v1_encode_nonzero);
    RUN_TEST(test_kia_v2_encode_nonzero);
    RUN_TEST(test_renault_v0_encode_nonzero);
    RUN_TEST(test_chrysler_v0_encode_nonzero);
    RUN_TEST(test_fiat_v0_encode_nonzero);
    RUN_TEST(test_subaru_encode_nonzero);
    RUN_TEST(test_star_line_encode_nonzero);
    return UNITY_END();
}
