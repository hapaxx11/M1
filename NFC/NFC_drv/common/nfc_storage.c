/* See COPYING.txt for license details. */


#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "nfc_storage.h"
#include "nfc_file_parse.h"
#include "m1_sdcard.h"
#include "nfc_fileio.h"
#include "nfc_ctx.h"
#include "m1_nfc.h"     /* M1NFC_FAM_*, M1NFC_TECH_* */
#include "privateprofilestring.h"  /* INI style parsing for header */
#include "logger.h"     /* platformLog */

#define NFC_STORAGE_MIN_DUMP_UNITS  1

/* nfc_family_info_t is now defined in nfc_file_parse.h */


/*============================================================================*/
/**
 * @brief Mark unit as valid in valid_bits bitmap
 * @param valid_bits Pointer to valid bits bitmap
 * @param valid_bytes Size of valid_bits buffer in bytes
 * @param idx Unit index to mark as valid
 */
/*============================================================================*/
static void mark_unit_valid(uint8_t* valid_bits, uint32_t valid_bytes, uint32_t idx)
{
    if (!valid_bits) return;
    uint32_t byte_idx = idx >> 3;
    uint8_t  bit_mask = (uint8_t)(1u << (idx & 7));
    if (byte_idx >= valid_bytes) return;
    valid_bits[byte_idx] |= bit_mask;
}


/*============================================================================*/
/**
 * @brief Parse device type string and fill family info structure
 * @param s Device type string (e.g., "Classic", "Ultralight/NTAG")
 * @param out Pointer to output structure to fill
 * @return 0 on success, -1 on failure
 */
/*============================================================================*/
static int parse_device_type(const char* s, nfc_family_info_t* out)
{
    /* Delegate to extracted pure-logic parser (nfc_file_parse.c).
     * Platform-specific #ifdef gating for T4T/Felica/15693/iClass is preserved
     * here as overrides when those families are defined. */
    int rc = nfc_parse_device_type(s, out);

#ifdef M1NFC_FAM_T4T
    if (rc != 0 && s && strncmp(s, "ISO14443-4A", 11) == 0) {
        out->tech      = M1NFC_TECH_A;
        out->family    = M1NFC_FAM_T4T;
        out->unit_size = 4;
        return 0;
    }
#endif

#ifdef M1NFC_FAM_FELICA
    if (rc == 0 && s && strncmp(s, "Felica", 6) == 0) {
        /* Override family to M1NFC_FAM_FELICA if defined */
        out->family = M1NFC_FAM_FELICA;
    }
#endif

    return rc;
}

/*============================================================================*/
/**
 * @brief Parse header section using INI style parsing (hybrid approach)
 * 
 * Uses privateprofilestring.c for header fields (Filetype, Version, Device type, UID, ATQA, SAK, ATS)
 * This provides consistency with RFID parsing and better maintainability.
 * 
 * @param c Pointer to NFC context
 * @param faminfo Pointer to output family info structure
 * @param file_path Full path to the NFC file
 * @return NFC_STORAGE_OK on success, error code on failure
 */
/*============================================================================*/
static nfc_storage_result_t nfc_storage_parse_header_ini(
        nfc_run_ctx_t* c,
        nfc_family_info_t* faminfo,
        const char* file_path)
{
    char buf[200];
    ParsedValue data;
    bool saw_filetype = false;
    bool saw_devtype  = false;

    memset(faminfo, 0, sizeof(*faminfo));

    /* 1) Validate Filetype: accept both M1 and Flipper formats */
    data.buf = buf;
    data.max_len = sizeof(buf);
    data.type = VALUE_TYPE_STRING;
    if (!GetPrivateProfileString(&data, "Filetype", file_path)) {
        platformLog("Filetype not found\r\n");
        return NFC_STORAGE_ERR_FORMAT;
    }
    if (strcmp(data.buf, "M1 NFC device") != 0 &&
        strcmp(data.buf, "Flipper NFC device") != 0) {
        platformLog("Unsupported Filetype: %s\r\n", (char*)data.buf);
        return NFC_STORAGE_ERR_FORMAT;
    }
    /* Validate Version — accept any numeric version (2, 3, 4, ...) */
    data.buf = buf;
    data.max_len = sizeof(buf);
    data.type = VALUE_TYPE_STRING;
    if (!GetPrivateProfileString(&data, "Version", file_path)) {
        platformLog("Version not found\r\n");
        return NFC_STORAGE_ERR_FORMAT;
    }
    saw_filetype = true;

    /* 2) Parse Device type */
    data.buf = buf;
    data.max_len = sizeof(buf);
    data.type = VALUE_TYPE_STRING;
    if (!GetPrivateProfileString(&data, "Device type", file_path)) {
        platformLog("Device type not found\r\n");
        return NFC_STORAGE_ERR_FORMAT;
    }
    if (parse_device_type(data.buf, faminfo) != 0) {
        platformLog("Unsupported Device type: %s\r\n", (char*)data.buf);
        return NFC_STORAGE_ERR_UNSUP;
    }
    c->head.tech   = faminfo->tech;
    c->head.family = faminfo->family;
    saw_devtype    = true;

    /* 3) Parse UID */
    // Use temporary buffer like RFID does, then copy to c->head.uid
    uint8_t tmp_uid[10];
    memset(tmp_uid, 0, sizeof(tmp_uid));
    data.buf = tmp_uid;
    data.max_len = sizeof(tmp_uid);
    data.type = VALUE_TYPE_HEX_ARRAY;
    
    // Debug: Check what we're trying to parse
    platformLog("[NFC Storage] Parsing UID from file: %s\r\n", file_path);
    
    if (!GetPrivateProfileHex(&data, "UID", file_path)) {
        platformLog("UID not found\r\n");
        return NFC_STORAGE_ERR_FORMAT;
    }
    
    // Debug: Check parsed result
    //platformLog("[NFC Storage] UID parse result: out_len=%d\r\n", data.v.hex.out_len);
    //platformLog("[NFC Storage] UID tmp buffer: %s\r\n", hex2Str(tmp_uid, data.v.hex.out_len));
    
    c->head.uid_len = (uint8_t)data.v.hex.out_len;
    if (c->head.uid_len == 0 || c->head.uid_len > 10) {
        platformLog("Invalid UID length: %d\r\n", c->head.uid_len);
        return NFC_STORAGE_ERR_FORMAT;
    }
    // Copy parsed UID to context (like RFID does)
    memcpy(c->head.uid, tmp_uid, c->head.uid_len);
    platformLog("[NFC Storage] UID final: len=%d, bytes=%s\r\n", 
                c->head.uid_len, hex2Str(c->head.uid, c->head.uid_len));

    /* 4) Parse ATQA (optional, Tech A only) */
    if (faminfo->tech == M1NFC_TECH_A) {
        data.buf = c->head.a.atqa;
        data.max_len = 2;
        data.type = VALUE_TYPE_HEX_ARRAY;
        if (GetPrivateProfileHex(&data, "ATQA", file_path) && data.v.hex.out_len == 2) {
            c->head.a.has_atqa = true;
        }
    }

    /* 5) Parse SAK (optional, Tech A only) */
    if (faminfo->tech == M1NFC_TECH_A) {
        uint8_t tmp_sak[1];
        data.buf = tmp_sak;
        data.max_len = 1;
        data.type = VALUE_TYPE_HEX_ARRAY;
        if (GetPrivateProfileHex(&data, "SAK", file_path) && data.v.hex.out_len == 1) {
            c->head.a.sak = tmp_sak[0];
            c->head.a.has_sak = true;
        }
    }

    /* 6) Parse ATS (optional, ISO14443-4A only) */
    if (faminfo->tech == M1NFC_TECH_A) {
        data.buf = c->head.a.ats;
        data.max_len = sizeof(c->head.a.ats);
        data.type = VALUE_TYPE_HEX_ARRAY;
        if (GetPrivateProfileHex(&data, "ATS", file_path) && data.v.hex.out_len > 0) {
            c->head.a.ats_len = (uint8_t)data.v.hex.out_len;
        }
    }

    /* 7) Parse "Mifare version" (Flipper v2/v3 format: GET_VERSION bytes) */
    if (faminfo->family == M1NFC_FAM_ULTRALIGHT) {
        uint8_t ver_buf[8];
        data.buf = ver_buf;
        data.max_len = sizeof(ver_buf);
        data.type = VALUE_TYPE_HEX_ARRAY;
        if (GetPrivateProfileHex(&data, "Mifare version", file_path) && data.v.hex.out_len == 8) {
            nfc_ctx_set_t2t_version(ver_buf, 8);
            platformLog("[NFC] Mifare version from file: %s\r\n", hex2Str(ver_buf, 8));
        }
    }

    if (!saw_filetype || !saw_devtype) {
        return NFC_STORAGE_ERR_FORMAT;
    }

    return NFC_STORAGE_OK;
}

/*============================================================================*/
/* nfcfio → nfc_line_reader_t adapter for nfc_parse_body()                    */
/*============================================================================*/
typedef struct {
    nfc_parser_t *ps;
} nfcfio_reader_ctx_t;

static int nfcfio_reader_getline(void *ctx, char *buf, size_t bufsz)
{
    nfcfio_reader_ctx_t *rc = (nfcfio_reader_ctx_t *)ctx;
    return nfcfio_getline(&rc->ps->io, buf, bufsz);
}

static const nfc_line_reader_t s_nfcfio_reader = { .getline = nfcfio_reader_getline };

/*============================================================================*/
/**
 * @brief Parse body section: "Page N:" for Type2, "Block N:" for Classic
 * @param c Pointer to NFC context
 * @param faminfo Pointer to family info structure
 * @param dump_buf Dump buffer to store parsed data
 * @param dump_buf_bytes Size of dump buffer
 * @param valid_bits Valid bits bitmap (optional)
 * @param valid_bits_bytes Size of valid_bits buffer
 * @return NFC_STORAGE_OK on success, error code on failure
 * @note Delegates to nfc_parse_body() from nfc_file_parse.c via nfcfio adapter
 */
/*============================================================================*/
static nfc_storage_result_t nfc_storage_parse_body(
        nfc_run_ctx_t*     c,
        const nfc_family_info_t* faminfo,
        uint8_t*           dump_buf,
        uint32_t           dump_buf_bytes,
        uint8_t*           valid_bits,
        uint32_t           valid_bits_bytes)
{
    if (!dump_buf || dump_buf_bytes == 0)
        return NFC_STORAGE_ERR_NO_BUFFER;

    uint16_t unit_size = faminfo->unit_size;
    if (unit_size == 0) unit_size = 4;
    uint32_t unit_count = dump_buf_bytes / unit_size;
    if (unit_count < NFC_STORAGE_MIN_DUMP_UNITS)
        return NFC_STORAGE_ERR_NO_BUFFER;

    nfcfio_reader_ctx_t rctx = { .ps = &c->parser };
    nfc_parse_body_meta_t meta;

    nfc_parse_result_t rc = nfc_parse_body(&s_nfcfio_reader, &rctx,
                                            unit_size,
                                            dump_buf, dump_buf_bytes,
                                            valid_bits, valid_bits_bytes,
                                            &meta);
    if (rc != NFC_PARSE_OK) {
        if (rc == NFC_PARSE_ERR_FORMAT) {
            platformLog("Body Parsing Fail\r\n");
            return NFC_STORAGE_ERR_FORMAT;
        }
        if (rc == NFC_PARSE_ERR_IO)
            return NFC_STORAGE_ERR_IO;
        return NFC_STORAGE_ERR_NO_BUFFER;
    }

    /* Parsing complete → set nfc_ctx.dump metadata */
    nfc_ctx_set_dump(meta.unit_size,
                     meta.unit_count,
                     0,             /* origin */
                     dump_buf,
                     valid_bits,
                     meta.max_seen_unit,
                     true);

    return NFC_STORAGE_OK;
}


/*============================================================================*/
/**
 * @brief Load and parse .nfc text file to fill nfc_ctx
 * 
 * Parses .nfc file format:
 * - Header: Filetype, Version, Device type, UID, ATQA, SAK, ATS, etc.
 * - Body: "Page N:" / "Block N:" lines → stored in dump buffer
 * 
 * @param path Full path on SD card (e.g., "/NFC/card1.nfc")
 * @param dump_buf Workspace pointer to store dump data
 * @param dump_buf_bytes Total size of dump_buf (in bytes)
 * @param valid_bits Unit validity bitmap (optional, NULL allowed)
 * @param valid_bits_bytes Size of valid_bits buffer (bytes, 1 byte per 8 units)
 * @return NFC_STORAGE_OK on success, error code on failure
 */
/*============================================================================*/
nfc_storage_result_t nfc_storage_load_file(
        const char* path,
        uint8_t*    dump_buf,
        uint32_t    dump_buf_bytes,
        uint8_t*    valid_bits,
        uint32_t    valid_bits_bytes)

{
    if (!path || !dump_buf || dump_buf_bytes == 0)
        return NFC_STORAGE_ERR_NO_BUFFER;

    nfc_run_ctx_t* c = nfc_ctx_get();
    nfc_family_info_t faminfo;
    nfc_storage_result_t ret;

    /* Start file session: set source_kind, path */
    nfc_ctx_begin_file(path);

    /* Initialize header/dump */
    memset(&c->head, 0, sizeof(c->head));
    nfc_ctx_clear_dump();
    c->parser.parse_error = 0;

    /* ---------- Phase 1: Parse header using INI style ---------- */
    ret = nfc_storage_parse_header_ini(c, &faminfo, path);

    if (ret != NFC_STORAGE_OK) {
        c->file.sys_error = 1;
        return ret;
    }

    /* ---------- Phase 2: Parse body (pages/blocks) ---------- */
    if (!nfcfio_open_read(&c->parser.io, path)) {
        c->file.sys_error = 1;
        return NFC_STORAGE_ERR_IO;
    }

    ret = nfc_storage_parse_body(c, &faminfo,
                                 dump_buf, dump_buf_bytes,
                                 valid_bits, valid_bits_bytes);

    nfcfio_close(&c->parser.io);

    if (ret == NFC_STORAGE_OK) {
        /* Fix PACK for NTAG215 amiibo: page 134 bytes 0-1 must be 0x80 0x80 */
        if (c->head.family == M1NFC_FAM_ULTRALIGHT &&
            c->dump.unit_size == 4 && c->dump.max_seen_unit >= 134 &&
            dump_buf_bytes >= 135 * 4) {
            uint8_t *pack_page = &dump_buf[134 * 4];
            if (pack_page[0] == 0x00 && pack_page[1] == 0x00) {
                pack_page[0] = 0x80;
                pack_page[1] = 0x80;
                platformLog("[NFC] Fixed PACK at page 134: 80 80\r\n");
            }
        }
        nfc_ctx_refresh_ui();
        c->file.sys_error = 0;
    } else {
        c->file.sys_error = 1;
    }

    return ret;
}


/*============================================================================*/
/**
 * @brief Load raw binary NTAG dump (.bin) into nfc_ctx
 *
 * Reads a raw binary file (e.g., 540-byte Amiibo / NTAG215 dump) from SD card,
 * extracts the 7-byte UID from the NTAG page layout, sets up the NFC context
 * for NTAG215 emulation (ATQA, SAK, GET_VERSION), and populates the dump buffer.
 *
 * NTAG215 memory layout (135 pages × 4 bytes = 540 bytes):
 *   Page 0:   UID[0], UID[1], UID[2], BCC0
 *   Page 1:   UID[3], UID[4], UID[5], UID[6]
 *   Page 2:   BCC1, Internal, Lock0, Lock1
 *   Page 3:   Capability Container (4 bytes)
 *   Page 4-129: User data (504 bytes)
 *   Page 130:  Dynamic lock bytes
 *   Page 131-132: Configuration
 *   Page 133:  PWD (4 bytes)
 *   Page 134:  PACK (2 bytes) + RFUI (2 bytes)
 *
 * @param path             Full path on SD card (e.g., "0:/NFC/amiibo.bin")
 * @param dump_buf         Workspace buffer for page data
 * @param dump_buf_bytes   Size of dump_buf in bytes
 * @param valid_bits       Validity bitmap (optional)
 * @param valid_bits_bytes Size of valid_bits buffer
 * @return NFC_STORAGE_OK on success, error code on failure
 */
/*============================================================================*/
nfc_storage_result_t nfc_storage_load_bin(
        const char* path,
        uint8_t*    dump_buf,
        uint32_t    dump_buf_bytes,
        uint8_t*    valid_bits,
        uint32_t    valid_bits_bytes)
{
    FIL fil;
    FRESULT fres;
    UINT bytes_read;

    if (!path || !dump_buf || dump_buf_bytes == 0)
        return NFC_STORAGE_ERR_NO_BUFFER;

    nfc_run_ctx_t* c = nfc_ctx_get();

    /* Start file session */
    nfc_ctx_begin_file(path);
    memset(&c->head, 0, sizeof(c->head));
    nfc_ctx_clear_dump();

    /* Open binary file */
    fres = f_open(&fil, path, FA_READ);
    if (fres != FR_OK) {
        platformLog("[BIN] f_open('%s') failed: %d\r\n", path, fres);
        c->file.sys_error = 1;
        return NFC_STORAGE_ERR_IO;
    }

    /* Read raw data into dump buffer */
    UINT file_size = f_size(&fil);
    if (file_size == 0) {
        f_close(&fil);
        c->file.sys_error = 1;
        return NFC_STORAGE_ERR_FORMAT;
    }

    /* Clamp to buffer capacity */
    uint32_t read_size = file_size;
    if (read_size > dump_buf_bytes)
        read_size = dump_buf_bytes;

    memset(dump_buf, 0, dump_buf_bytes);
    fres = f_read(&fil, dump_buf, read_size, &bytes_read);
    f_close(&fil);

    if (fres != FR_OK || bytes_read == 0) {
        platformLog("[BIN] f_read failed: fres=%d, read=%u\r\n", fres, bytes_read);
        c->file.sys_error = 1;
        return NFC_STORAGE_ERR_IO;
    }

    /* Calculate page count (4 bytes per page) */
    uint32_t page_count = bytes_read / 4;
    if (page_count == 0) {
        c->file.sys_error = 1;
        return NFC_STORAGE_ERR_FORMAT;
    }

    /* Extract 7-byte UID from NTAG layout */
    if (bytes_read >= 8) {
        c->head.uid[0] = dump_buf[0];   /* Page 0, byte 0 */
        c->head.uid[1] = dump_buf[1];   /* Page 0, byte 1 */
        c->head.uid[2] = dump_buf[2];   /* Page 0, byte 2 */
        /* dump_buf[3] = BCC0, skip */
        c->head.uid[3] = dump_buf[4];   /* Page 1, byte 0 */
        c->head.uid[4] = dump_buf[5];   /* Page 1, byte 1 */
        c->head.uid[5] = dump_buf[6];   /* Page 1, byte 2 */
        c->head.uid[6] = dump_buf[7];   /* Page 1, byte 3 */
        c->head.uid_len = 7;
    } else {
        c->file.sys_error = 1;
        return NFC_STORAGE_ERR_FORMAT;
    }

    /* Set NFC-A identity for NTAG215 */
    c->head.tech         = M1NFC_TECH_A;
    c->head.family       = M1NFC_FAM_ULTRALIGHT;
    c->head.a.atqa[0]    = 0x44;
    c->head.a.atqa[1]    = 0x00;
    c->head.a.has_atqa   = true;
    c->head.a.sak        = 0x00;
    c->head.a.has_sak    = true;

    /* Set GET_VERSION for NTAG215 */
    uint8_t ntag215_ver[8] = { 0x00, 0x04, 0x04, 0x02, 0x01, 0x00, 0x11, 0x03 };
    nfc_ctx_set_t2t_version(ntag215_ver, 8);

    /* Fix PACK for amiibo: page 134 bytes 0-1 must be 0x80 0x80.
     * Many dump tools write 0x00 because PWD/PACK are write-only on real tags.
     * The Switch sends PWD_AUTH and checks the 2-byte PACK response. */
    if (page_count >= 135 && bytes_read >= 135 * 4) {
        uint8_t *pack_page = &dump_buf[134 * 4];
        if (pack_page[0] == 0x00 && pack_page[1] == 0x00) {
            pack_page[0] = 0x80;
            pack_page[1] = 0x80;
            platformLog("[BIN] Fixed PACK at page 134: 80 80\r\n");
        }
    }

    /* Mark all read pages as valid */
    if (valid_bits && valid_bits_bytes > 0)
        memset(valid_bits, 0, valid_bits_bytes);

    for (uint32_t i = 0; i < page_count; i++) {
        mark_unit_valid(valid_bits, valid_bits_bytes, i);
    }

    /* Set dump metadata */
    nfc_ctx_set_dump(4, page_count, 0, dump_buf, valid_bits,
                     page_count - 1, true);

    /* Update UI */
    nfc_ctx_refresh_ui();
    c->file.sys_error = 0;

    platformLog("[BIN] Loaded %lu pages from '%s', UID=%s\r\n",
                (unsigned long)page_count, path,
                hex2Str(c->head.uid, c->head.uid_len));

    return NFC_STORAGE_OK;
}
