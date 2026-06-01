/* See COPYING.txt for license details. */

/**
 * test_nfc_file_parse.c
 *
 * Unit tests for the pure-logic NFC file body parser in nfc_file_parse.h/.c:
 *   nfc_parse_hex_bytes()    — hex string → byte array
 *   nfc_parse_device_type()  — device-type string → tech/family/unit_size
 *   nfc_parse_body()         — "Page N:" / "Block N:" lines → dump buffer
 *
 * Uses an in-memory line reader (no FatFS dependency).
 *
 * Build with the host-side CMake:
 *   cmake -B build-tests -S tests && cmake --build build-tests
 *   ctest --test-dir build-tests --output-on-failure
 */

#include "unity.h"
#include "nfc_file_parse.h"
#include <string.h>
#include <stdint.h>

void setUp(void)    {}
void tearDown(void) {}

/* =========================================================================
 * In-memory line reader for testing (no FatFS)
 * =========================================================================*/

typedef struct {
    const char *data;   /* Full input string */
    size_t      pos;    /* Current read position */
    size_t      len;    /* Total length */
} mem_reader_ctx_t;

static int mem_getline(void *ctx, char *buf, size_t bufsz)
{
    mem_reader_ctx_t *mr = (mem_reader_ctx_t *)ctx;
    if (mr->pos >= mr->len) return -1;  /* EOF */

    size_t w = 0;
    while (mr->pos < mr->len && w + 1 < bufsz) {
        char c = mr->data[mr->pos++];
        buf[w++] = c;
        if (c == '\n') break;
    }
    buf[w] = '\0';
    return (int)w;
}

static const nfc_line_reader_t s_mem_reader = { .getline = mem_getline };

/* =========================================================================
 * nfc_parse_hex_bytes tests
 * =========================================================================*/

void test_hex_spaced(void)
{
    uint8_t out[8];
    size_t  len = 0;
    TEST_ASSERT_EQUAL_INT(0, nfc_parse_hex_bytes("AA BB CC", out, sizeof(out), &len));
    TEST_ASSERT_EQUAL_size_t(3, len);
    TEST_ASSERT_EQUAL_HEX8(0xAA, out[0]);
    TEST_ASSERT_EQUAL_HEX8(0xBB, out[1]);
    TEST_ASSERT_EQUAL_HEX8(0xCC, out[2]);
}

void test_hex_packed(void)
{
    uint8_t out[8];
    size_t  len = 0;
    TEST_ASSERT_EQUAL_INT(0, nfc_parse_hex_bytes("AABBCC", out, sizeof(out), &len));
    TEST_ASSERT_EQUAL_size_t(3, len);
    TEST_ASSERT_EQUAL_HEX8(0xAA, out[0]);
}

void test_hex_lowercase(void)
{
    uint8_t out[4];
    size_t  len = 0;
    TEST_ASSERT_EQUAL_INT(0, nfc_parse_hex_bytes("de ad", out, sizeof(out), &len));
    TEST_ASSERT_EQUAL_size_t(2, len);
    TEST_ASSERT_EQUAL_HEX8(0xDE, out[0]);
    TEST_ASSERT_EQUAL_HEX8(0xAD, out[1]);
}

void test_hex_odd_nibble(void)
{
    uint8_t out[4];
    size_t  len = 0;
    TEST_ASSERT_EQUAL_INT(1, nfc_parse_hex_bytes("ABC", out, sizeof(out), &len));
}

void test_hex_invalid_char(void)
{
    uint8_t out[4];
    size_t  len = 0;
    TEST_ASSERT_EQUAL_INT(2, nfc_parse_hex_bytes("GG", out, sizeof(out), &len));
}

void test_hex_buffer_full(void)
{
    uint8_t out[2];
    size_t  len = 0;
    TEST_ASSERT_EQUAL_INT(3, nfc_parse_hex_bytes("AA BB CC", out, 2, &len));
}

void test_hex_empty(void)
{
    uint8_t out[4];
    size_t  len = 99;
    TEST_ASSERT_EQUAL_INT(0, nfc_parse_hex_bytes("", out, sizeof(out), &len));
    TEST_ASSERT_EQUAL_size_t(0, len);
}

void test_hex_whitespace_only(void)
{
    uint8_t out[4];
    size_t  len = 99;
    TEST_ASSERT_EQUAL_INT(0, nfc_parse_hex_bytes("   \t  \n", out, sizeof(out), &len));
    TEST_ASSERT_EQUAL_size_t(0, len);
}

void test_hex_null_input(void)
{
    uint8_t out[4];
    size_t  len = 0;
    TEST_ASSERT_EQUAL_INT(2, nfc_parse_hex_bytes(NULL, out, sizeof(out), &len));
}

/* =========================================================================
 * nfc_parse_device_type tests
 * =========================================================================*/

void test_devtype_classic(void)
{
    nfc_family_info_t fi;
    TEST_ASSERT_EQUAL_INT(0, nfc_parse_device_type("Classic", &fi));
    TEST_ASSERT_EQUAL_UINT8(0, fi.tech);
    TEST_ASSERT_EQUAL_UINT8(0, fi.family);      /* FAM_CLASSIC */
    TEST_ASSERT_EQUAL_UINT16(16, fi.unit_size);
}

void test_devtype_mifare_classic_1k(void)
{
    nfc_family_info_t fi;
    TEST_ASSERT_EQUAL_INT(0, nfc_parse_device_type("Mifare Classic 1K", &fi));
    TEST_ASSERT_EQUAL_UINT16(16, fi.unit_size);
}

void test_devtype_ultralight(void)
{
    nfc_family_info_t fi;
    TEST_ASSERT_EQUAL_INT(0, nfc_parse_device_type("Ultralight/NTAG", &fi));
    TEST_ASSERT_EQUAL_UINT8(1, fi.family);       /* FAM_ULTRALIGHT */
    TEST_ASSERT_EQUAL_UINT16(4, fi.unit_size);
}

void test_devtype_ntag215(void)
{
    nfc_family_info_t fi;
    TEST_ASSERT_EQUAL_INT(0, nfc_parse_device_type("NTAG215", &fi));
    TEST_ASSERT_EQUAL_UINT8(1, fi.family);
}

void test_devtype_mifare_ultralight(void)
{
    nfc_family_info_t fi;
    TEST_ASSERT_EQUAL_INT(0, nfc_parse_device_type("Mifare Ultralight", &fi));
    TEST_ASSERT_EQUAL_UINT8(1, fi.family);
}

void test_devtype_iso14443_3a(void)
{
    nfc_family_info_t fi;
    TEST_ASSERT_EQUAL_INT(0, nfc_parse_device_type("ISO14443-3A", &fi));
    TEST_ASSERT_EQUAL_UINT8(0, fi.tech);
}

void test_devtype_desfire(void)
{
    nfc_family_info_t fi;
    TEST_ASSERT_EQUAL_INT(0, nfc_parse_device_type("DESFire", &fi));
    TEST_ASSERT_EQUAL_UINT8(2, fi.family);       /* FAM_DESFIRE */
}

void test_devtype_picopass(void)
{
    nfc_family_info_t fi;
    TEST_ASSERT_EQUAL_INT(0, nfc_parse_device_type("PicoPass", &fi));
    TEST_ASSERT_EQUAL_UINT16(8, fi.unit_size);
}

void test_devtype_unknown(void)
{
    nfc_family_info_t fi;
    TEST_ASSERT_EQUAL_INT(-1, nfc_parse_device_type("FooBar", &fi));
}

void test_devtype_null(void)
{
    nfc_family_info_t fi;
    TEST_ASSERT_EQUAL_INT(-1, nfc_parse_device_type(NULL, &fi));
}

/* =========================================================================
 * nfc_parse_body tests
 * =========================================================================*/

void test_body_pages(void)
{
    const char *input =
        "Page 0: 04 A2 BC DE\n"
        "Page 1: 11 22 33 44\n"
        "Page 2: AA BB CC DD\n";

    mem_reader_ctx_t mr = { .data = input, .pos = 0, .len = strlen(input) };
    uint8_t dump[64] = {0};
    uint8_t valid[8] = {0};
    nfc_parse_body_meta_t meta;

    nfc_parse_result_t rc = nfc_parse_body(&s_mem_reader, &mr, 4,
                                            dump, sizeof(dump),
                                            valid, sizeof(valid), &meta);
    TEST_ASSERT_EQUAL_INT(NFC_PARSE_OK, rc);
    TEST_ASSERT_EQUAL_UINT32(2, meta.max_seen_unit);
    TEST_ASSERT_EQUAL_HEX8(0x04, dump[0]);
    TEST_ASSERT_EQUAL_HEX8(0xA2, dump[1]);
    TEST_ASSERT_EQUAL_HEX8(0x11, dump[4]);
    TEST_ASSERT_EQUAL_HEX8(0xAA, dump[8]);
    /* valid bits: pages 0,1,2 marked */
    TEST_ASSERT_EQUAL_HEX8(0x07, valid[0]);
}

void test_body_blocks(void)
{
    const char *input =
        "Block 0: 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F 10\n"
        "Block 1: 11 12 13 14 15 16 17 18 19 1A 1B 1C 1D 1E 1F 20\n";

    mem_reader_ctx_t mr = { .data = input, .pos = 0, .len = strlen(input) };
    uint8_t dump[256] = {0};
    uint8_t valid[32] = {0};
    nfc_parse_body_meta_t meta;

    nfc_parse_result_t rc = nfc_parse_body(&s_mem_reader, &mr, 16,
                                            dump, sizeof(dump),
                                            valid, sizeof(valid), &meta);
    TEST_ASSERT_EQUAL_INT(NFC_PARSE_OK, rc);
    TEST_ASSERT_EQUAL_UINT32(1, meta.max_seen_unit);
    TEST_ASSERT_EQUAL_HEX8(0x01, dump[0]);
    TEST_ASSERT_EQUAL_HEX8(0x10, dump[15]);
    TEST_ASSERT_EQUAL_HEX8(0x11, dump[16]);
}

void test_body_skip_comments_and_blanks(void)
{
    const char *input =
        "# This is a comment\n"
        "\n"
        "Page 0: FF EE DD CC\n"
        "# Another comment\n"
        "Page 1: 11 22 33 44\n";

    mem_reader_ctx_t mr = { .data = input, .pos = 0, .len = strlen(input) };
    uint8_t dump[32] = {0};
    nfc_parse_body_meta_t meta;

    nfc_parse_result_t rc = nfc_parse_body(&s_mem_reader, &mr, 4,
                                            dump, sizeof(dump),
                                            NULL, 0, &meta);
    TEST_ASSERT_EQUAL_INT(NFC_PARSE_OK, rc);
    TEST_ASSERT_EQUAL_UINT32(1, meta.max_seen_unit);
    TEST_ASSERT_EQUAL_HEX8(0xFF, dump[0]);
    TEST_ASSERT_EQUAL_HEX8(0x11, dump[4]);
}

void test_body_sparse_pages(void)
{
    const char *input =
        "Page 0: 01 02 03 04\n"
        "Page 5: AA BB CC DD\n";

    mem_reader_ctx_t mr = { .data = input, .pos = 0, .len = strlen(input) };
    uint8_t dump[64] = {0};
    uint8_t valid[8] = {0};
    nfc_parse_body_meta_t meta;

    nfc_parse_result_t rc = nfc_parse_body(&s_mem_reader, &mr, 4,
                                            dump, sizeof(dump),
                                            valid, sizeof(valid), &meta);
    TEST_ASSERT_EQUAL_INT(NFC_PARSE_OK, rc);
    TEST_ASSERT_EQUAL_UINT32(5, meta.max_seen_unit);
    /* Pages 1-4 should be zero */
    TEST_ASSERT_EQUAL_HEX8(0x00, dump[4]);
    /* Page 5 should have data */
    TEST_ASSERT_EQUAL_HEX8(0xAA, dump[20]);
    /* Valid bits: only 0 and 5 */
    TEST_ASSERT_EQUAL_HEX8(0x21, valid[0]);
}

void test_body_empty_input(void)
{
    const char *input = "";
    mem_reader_ctx_t mr = { .data = input, .pos = 0, .len = 0 };
    uint8_t dump[16] = {0};
    nfc_parse_body_meta_t meta;

    nfc_parse_result_t rc = nfc_parse_body(&s_mem_reader, &mr, 4,
                                            dump, sizeof(dump),
                                            NULL, 0, &meta);
    TEST_ASSERT_EQUAL_INT(NFC_PARSE_OK, rc);
    TEST_ASSERT_EQUAL_UINT32(0, meta.max_seen_unit);
}

void test_body_null_buffer(void)
{
    nfc_parse_body_meta_t meta;
    nfc_parse_result_t rc = nfc_parse_body(&s_mem_reader, NULL, 4,
                                            NULL, 0,
                                            NULL, 0, &meta);
    TEST_ASSERT_EQUAL_INT(NFC_PARSE_ERR_NO_BUFFER, rc);
}

void test_body_no_newline_at_end(void)
{
    const char *input = "Page 0: 01 02 03 04";  /* no trailing \n */
    mem_reader_ctx_t mr = { .data = input, .pos = 0, .len = strlen(input) };
    uint8_t dump[16] = {0};
    nfc_parse_body_meta_t meta;

    nfc_parse_result_t rc = nfc_parse_body(&s_mem_reader, &mr, 4,
                                            dump, sizeof(dump),
                                            NULL, 0, &meta);
    TEST_ASSERT_EQUAL_INT(NFC_PARSE_OK, rc);
    TEST_ASSERT_EQUAL_HEX8(0x01, dump[0]);
}

/* =========================================================================
 * main
 * =========================================================================*/

int main(void)
{
    UNITY_BEGIN();

    /* hex parsing */
    RUN_TEST(test_hex_spaced);
    RUN_TEST(test_hex_packed);
    RUN_TEST(test_hex_lowercase);
    RUN_TEST(test_hex_odd_nibble);
    RUN_TEST(test_hex_invalid_char);
    RUN_TEST(test_hex_buffer_full);
    RUN_TEST(test_hex_empty);
    RUN_TEST(test_hex_whitespace_only);
    RUN_TEST(test_hex_null_input);

    /* device type */
    RUN_TEST(test_devtype_classic);
    RUN_TEST(test_devtype_mifare_classic_1k);
    RUN_TEST(test_devtype_ultralight);
    RUN_TEST(test_devtype_ntag215);
    RUN_TEST(test_devtype_mifare_ultralight);
    RUN_TEST(test_devtype_iso14443_3a);
    RUN_TEST(test_devtype_desfire);
    RUN_TEST(test_devtype_picopass);
    RUN_TEST(test_devtype_unknown);
    RUN_TEST(test_devtype_null);

    /* body parsing */
    RUN_TEST(test_body_pages);
    RUN_TEST(test_body_blocks);
    RUN_TEST(test_body_skip_comments_and_blanks);
    RUN_TEST(test_body_sparse_pages);
    RUN_TEST(test_body_empty_input);
    RUN_TEST(test_body_null_buffer);
    RUN_TEST(test_body_no_newline_at_end);

    return UNITY_END();
}
