/* See COPYING.txt for license details. */

/**
 * @file   nfc_file_parse.c
 * @brief  Pure-logic NFC file body parser — no HAL, RTOS, FatFS, or display deps.
 *
 * Extracted from nfc_storage.c (Phase B blocker removal).
 * Uses nfc_line_reader_t vtable for I/O abstraction (same pattern as
 * ir_block_reader_t in ir_signal_record.c).
 */

#include "nfc_file_parse.h"
#include <string.h>
#include <ctype.h>
#include <stdio.h>

/* =========================================================================
 * Internal helpers (extracted from nfc_storage.c statics)
 * =========================================================================*/

static const char *skip_ws(const char *s)
{
    while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') s++;
    return s;
}

static char *str_trim_inplace(char *s)
{
    /* ltrim */
    char *p = s;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    /* rtrim */
    size_t len = strlen(s);
    while (len > 0 &&
           (s[len - 1] == ' ' || s[len - 1] == '\t' ||
            s[len - 1] == '\r' || s[len - 1] == '\n')) {
        s[--len] = '\0';
    }
    return s;
}

static int hex_nibble(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

/* =========================================================================
 * nfc_parse_hex_bytes  (was static parse_hex_bytes in nfc_storage.c)
 * =========================================================================*/

int nfc_parse_hex_bytes(const char *s, uint8_t *out, size_t max, size_t *out_len)
{
    size_t  n       = 0;
    int     have_hi = 0;
    uint8_t hi_nib  = 0;

    if (!s || !out) {
        if (out_len) *out_len = 0;
        return 2;
    }

    const unsigned char *p = (const unsigned char *)skip_ws(s);

    while (*p) {
        if (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
            p++;
            continue;
        }

        if (!isxdigit(*p))
            return 2;

        int v = hex_nibble((char)*p);
        if (v < 0)
            return 2;

        if (!have_hi) {
            hi_nib  = (uint8_t)v;
            have_hi = 1;
        } else {
            if (n >= max)
                return 3;
            out[n++] = (uint8_t)((hi_nib << 4) | (uint8_t)v);
            have_hi  = 0;
        }
        p++;
    }

    if (have_hi)
        return 1;

    if (out_len) *out_len = n;
    return 0;
}

/* =========================================================================
 * nfc_parse_device_type  (was static parse_device_type in nfc_storage.c)
 * =========================================================================*/

/* Family / tech constants — match nfc_ctx.h values exactly */
#define TECH_A  0
#define TECH_B  1
#define TECH_F  2
#define TECH_V  3

#define FAM_CLASSIC      0
#define FAM_ULTRALIGHT   1
#define FAM_DESFIRE      2
#define FAM_ICLASS       3
#define FAM_15693        4

int nfc_parse_device_type(const char *s, nfc_family_info_t *out)
{
    if (!s || !out) return -1;
    memset(out, 0, sizeof(*out));

    /* M1 format */
    if (strncmp(s, "Classic", 7) == 0) {
        out->tech = TECH_A; out->family = FAM_CLASSIC; out->unit_size = 16;
        return 0;
    }

    /* Flipper format: "Mifare Classic 1K", "Mifare Classic 4K" */
    if (strncmp(s, "Mifare Classic", 14) == 0) {
        out->tech = TECH_A; out->family = FAM_CLASSIC; out->unit_size = 16;
        return 0;
    }

    if (strncmp(s, "Ultralight/NTAG", 15) == 0) {
        out->tech = TECH_A; out->family = FAM_ULTRALIGHT; out->unit_size = 4;
        return 0;
    }

    /* Flipper format: "NTAG215", "NTAG213", "NTAG216", "NTAG203" */
    if (strncmp(s, "NTAG", 4) == 0) {
        out->tech = TECH_A; out->family = FAM_ULTRALIGHT; out->unit_size = 4;
        return 0;
    }

    /* Flipper format: "Mifare Ultralight", "Mifare Ultralight EV1 ..." */
    if (strncmp(s, "Mifare Ultralight", 17) == 0) {
        out->tech = TECH_A; out->family = FAM_ULTRALIGHT; out->unit_size = 4;
        return 0;
    }

    /* Flipper format: "ISO14443-3A" — generic NFC-A, default to Ultralight */
    if (strncmp(s, "ISO14443-3A", 11) == 0) {
        out->tech = TECH_A; out->family = FAM_ULTRALIGHT; out->unit_size = 4;
        return 0;
    }

    if (strncmp(s, "DESFire", 7) == 0) {
        out->tech = TECH_A; out->family = FAM_DESFIRE; out->unit_size = 16;
        return 0;
    }

    if (strncmp(s, "ISO14443-4A", 11) == 0) {
        out->tech = TECH_A; out->family = FAM_ULTRALIGHT; out->unit_size = 4;
        return 0;
    }

    if (strncmp(s, "Felica", 6) == 0) {
        out->tech = TECH_F; out->family = FAM_CLASSIC; out->unit_size = 16;
        return 0;
    }

    if (strncmp(s, "ISO15693", 8) == 0) {
        out->tech = TECH_V; out->family = FAM_15693; out->unit_size = 4;
        return 0;
    }

    if (strncmp(s, "PicoPass", 8) == 0) {
        out->tech = TECH_V; out->family = FAM_ICLASS; out->unit_size = 8;
        return 0;
    }

    return -1;
}

/* =========================================================================
 * Internal: mark_unit_valid  (was static in nfc_storage.c)
 * =========================================================================*/

static void mark_unit_valid(uint8_t *valid_bits, uint32_t valid_bytes, uint32_t idx)
{
    if (!valid_bits) return;
    uint32_t byte_idx = idx >> 3;
    uint8_t  bit_mask = (uint8_t)(1u << (idx & 7));
    if (byte_idx >= valid_bytes) return;
    valid_bits[byte_idx] |= bit_mask;
}

/* =========================================================================
 * Internal: store_unit  (was static nfc_storage_store_unit in nfc_storage.c)
 * =========================================================================*/

static void store_unit(
        uint8_t        *dump_buf,
        uint16_t        unit_size,
        uint32_t        unit_count,
        uint8_t        *valid_bits,
        uint32_t        valid_bits_bytes,
        uint32_t       *max_seen_unit,
        uint32_t        idx,
        const uint8_t  *src_bytes,
        size_t          src_len)
{
    if (!dump_buf || unit_size == 0 || unit_count == 0) return;

    if (src_len < unit_size) {
        uint8_t tmp[32] = {0};
        size_t copy = src_len;
        if (copy > sizeof(tmp)) copy = sizeof(tmp);
        memcpy(tmp, src_bytes, copy);

        if (idx < unit_count) {
            memcpy(dump_buf + idx * unit_size, tmp, unit_size);
            mark_unit_valid(valid_bits, valid_bits_bytes, idx);
            if (max_seen_unit && idx > *max_seen_unit) *max_seen_unit = idx;
        }
        return;
    }

    if (idx >= unit_count) return;

    memcpy(dump_buf + idx * unit_size, src_bytes, unit_size);
    mark_unit_valid(valid_bits, valid_bits_bytes, idx);
    if (max_seen_unit && idx > *max_seen_unit) *max_seen_unit = idx;
}

/* =========================================================================
 * nfc_parse_body  (was static nfc_storage_parse_body in nfc_storage.c)
 * =========================================================================*/

nfc_parse_result_t nfc_parse_body(
        const nfc_line_reader_t *ops,
        void                    *ctx,
        uint16_t                 unit_size,
        uint8_t                 *dump_buf,
        uint32_t                 dump_buf_bytes,
        uint8_t                 *valid_bits,
        uint32_t                 valid_bits_bytes,
        nfc_parse_body_meta_t   *meta)
{
    if (!ops || !ops->getline || !dump_buf || dump_buf_bytes == 0 || !meta)
        return NFC_PARSE_ERR_NO_BUFFER;

    if (unit_size == 0) unit_size = 4;
    uint32_t unit_count = dump_buf_bytes / unit_size;
    if (unit_count == 0)
        return NFC_PARSE_ERR_NO_BUFFER;

    memset(dump_buf, 0, dump_buf_bytes);
    if (valid_bits && valid_bits_bytes > 0)
        memset(valid_bits, 0, valid_bits_bytes);

    uint32_t max_seen = 0;
    char line_buf[256];

    while (1) {
        int n = ops->getline(ctx, line_buf, sizeof(line_buf));
        if (n == -1) break;          /* EOF */
        if (n < 0)  return NFC_PARSE_ERR_IO;

        char *line = str_trim_inplace(line_buf);
        if (line[0] == '\0') continue;
        if (line[0] == '#')  continue;

        /* Classic: "Block N:" */
        if (strncmp(line, "Block ", 6) == 0) {
            unsigned long idx = 0;
            char *colon = strchr(line, ':');
            if (!colon) continue;
            *colon = '\0';
            if (sscanf(line, "Block %lu", &idx) != 1) continue;

            char *data_str = colon + 1;
            while (*data_str == ' ' || *data_str == '\t') data_str++;
            uint8_t bytes[32];
            size_t  len = 0;
            int b = nfc_parse_hex_bytes(data_str, bytes, sizeof(bytes), &len);
            if (b != 0)
                return NFC_PARSE_ERR_FORMAT;
            store_unit(dump_buf, unit_size, unit_count,
                       valid_bits, valid_bits_bytes,
                       &max_seen, (uint32_t)idx,
                       bytes, len);
            continue;
        }

        /* Type2: "Page N:" */
        if (strncmp(line, "Page ", 5) == 0) {
            unsigned long idx = 0;
            char *colon = strchr(line, ':');
            if (!colon) continue;
            *colon = '\0';
            if (sscanf(line, "Page %lu", &idx) != 1) continue;

            char *data_str = colon + 1;
            while (*data_str == ' ' || *data_str == '\t') data_str++;
            uint8_t bytes[32];
            size_t  len = 0;
            int b = nfc_parse_hex_bytes(data_str, bytes, sizeof(bytes), &len);
            if (b != 0)
                return NFC_PARSE_ERR_FORMAT;
            store_unit(dump_buf, unit_size, unit_count,
                       valid_bits, valid_bits_bytes,
                       &max_seen, (uint32_t)idx,
                       bytes, len);
            continue;
        }
    }

    meta->unit_size      = unit_size;
    meta->unit_count     = unit_count;
    meta->max_seen_unit  = max_seen;
    return NFC_PARSE_OK;
}
