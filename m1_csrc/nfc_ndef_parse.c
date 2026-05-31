/* See COPYING.txt for license details. */

/*
 * nfc_ndef_parse.c
 *
 * Pure-logic NDEF (NFC Data Exchange Format) parser for M1.
 *
 * Decodes NDEF messages from Type 2 Tags into human-readable text.
 * Hardware-independent — no FatFS, no RTOS, no display.
 *
 * M1 Project
 */

#include "nfc_ndef_parse.h"
#include <string.h>

/* NFC Forum URI RTD prefix table — 36 entries (0x00–0x23). */
static const char *const s_uri_prefixes[] = {
    "",                                  /* 0x00 — no prefix */
    "http://www.",                       /* 0x01 */
    "https://www.",                      /* 0x02 */
    "http://",                           /* 0x03 */
    "https://",                          /* 0x04 */
    "tel:",                              /* 0x05 */
    "mailto:",                           /* 0x06 */
    "ftp://anonymous:anonymous@",        /* 0x07 */
    "ftp://ftp.",                        /* 0x08 */
    "ftps://",                           /* 0x09 */
    "sftp://",                           /* 0x0A */
    "smb://",                            /* 0x0B */
    "nfs://",                            /* 0x0C */
    "ftp://",                            /* 0x0D */
    "dav://",                            /* 0x0E */
    "news:",                             /* 0x0F */
    "telnet://",                         /* 0x10 */
    "imap:",                             /* 0x11 */
    "rtsp://",                           /* 0x12 */
    "urn:",                              /* 0x13 */
    "pop:",                              /* 0x14 */
    "sip:",                              /* 0x15 */
    "sips:",                             /* 0x16 */
    "tftp:",                             /* 0x17 */
    "btspp://",                          /* 0x18 */
    "btl2cap://",                        /* 0x19 */
    "btgoep://",                         /* 0x1A */
    "tcpobex://",                        /* 0x1B */
    "irdaobex://",                       /* 0x1C */
    "file://",                           /* 0x1D */
    "urn:epc:id:",                       /* 0x1E */
    "urn:epc:tag:",                      /* 0x1F */
    "urn:epc:pat:",                      /* 0x20 */
    "urn:epc:raw:",                      /* 0x21 */
    "urn:epc:",                          /* 0x22 */
    "urn:nfc:",                          /* 0x23 */
};

#define URI_PREFIX_COUNT  (sizeof(s_uri_prefixes) / sizeof(s_uri_prefixes[0]))

uint16_t ndef_parse_records(const uint8_t *ndef_msg, uint16_t ndef_len,
                            char *out, uint16_t out_size)
{
    if (!ndef_msg || !out || out_size == 0)
        return 0;

    out[0] = '\0';

    if (ndef_len == 0)
        return 0;

    uint16_t pos     = 0;
    uint16_t written = 0;

    while (pos < ndef_len && written < out_size - 1)
    {
        /* Need at least: header + type_len + (1 byte SR payload_len) */
        if (pos + 3 > ndef_len) break;

        uint8_t  header   = ndef_msg[pos++];
        uint8_t  tnf      = header & 0x07;
        uint8_t  type_len = ndef_msg[pos++];
        uint32_t payload_len;

        if (header & 0x10) { /* SR (Short Record) flag */
            payload_len = ndef_msg[pos++];
        } else {
            if (pos + 4 > ndef_len) break;
            payload_len = ((uint32_t)ndef_msg[pos]     << 24) |
                          ((uint32_t)ndef_msg[pos + 1] << 16) |
                          ((uint32_t)ndef_msg[pos + 2] <<  8) |
                           (uint32_t)ndef_msg[pos + 3];
            pos += 4;
        }

        /* IL (ID Length) flag */
        uint8_t id_len = 0;
        if (header & 0x08) {
            if (pos >= ndef_len) break;
            id_len = ndef_msg[pos++];
        }

        /* Type field */
        if (pos + type_len > ndef_len) break;
        uint8_t type_byte = (type_len > 0) ? ndef_msg[pos] : 0;
        pos += type_len;

        /* ID field */
        pos += id_len;

        /* Payload */
        if (pos + payload_len > ndef_len) break;

        /* ── URI record (TNF=0x01, Type='U') ── */
        if (tnf == 0x01 && type_byte == 'U' && payload_len > 0)
        {
            uint8_t prefix_code = ndef_msg[pos];
            const char *prefix = (prefix_code < URI_PREFIX_COUNT)
                                     ? s_uri_prefixes[prefix_code] : "";
            uint16_t plen = (uint16_t)strlen(prefix);

            if (written + plen < out_size - 1) {
                memcpy(&out[written], prefix, plen);
                written += plen;
            }

            uint32_t copy_len = payload_len - 1;
            if (written + copy_len >= out_size - 1)
                copy_len = out_size - 1 - written;
            memcpy(&out[written], &ndef_msg[pos + 1], copy_len);
            written += (uint16_t)copy_len;

            if (written < out_size - 1)
                out[written++] = '\n';
        }
        /* ── Text record (TNF=0x01, Type='T') ── */
        else if (tnf == 0x01 && type_byte == 'T' && payload_len > 1)
        {
            uint8_t  status     = ndef_msg[pos];
            uint8_t  lang_len   = status & 0x3F;
            uint32_t text_start = 1u + lang_len;

            if (text_start < payload_len) {
                uint32_t text_len = payload_len - text_start;
                if (written + text_len >= out_size - 1)
                    text_len = out_size - 1 - written;
                memcpy(&out[written], &ndef_msg[pos + text_start], text_len);
                written += (uint16_t)text_len;

                if (written < out_size - 1)
                    out[written++] = '\n';
            }
        }

        pos += (uint16_t)payload_len;

        /* ME (Message End) flag — last record */
        if (header & 0x40) break;
    }

    /* Strip trailing newline */
    if (written > 0 && out[written - 1] == '\n')
        written--;

    out[written] = '\0';
    return written;
}
