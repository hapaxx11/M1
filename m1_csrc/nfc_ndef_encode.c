/* See COPYING.txt for license details. */

/*
 * nfc_ndef_encode.c
 *
 * Pure-logic NDEF (NFC Data Exchange Format) encoder for M1.
 *
 * Builds NDEF TLV payloads suitable for writing to Type 2 Tags
 * (NTAG213/215/216).  Hardware-independent — no FatFS, no RTOS,
 * no display.
 *
 * M1 Project
 */

#include "nfc_ndef_encode.h"
#include <string.h>

/*──────────── Internal helpers ────────────*/

/**
 * @brief  Build a single-record NDEF message in TLV format.
 *
 * Structure:
 *   [0x03]                    — NDEF Message TLV type
 *   [length]                  — 1 byte (short) or 3 bytes (0xFF + 2-byte BE)
 *   [NDEF record header]      — MB|ME|CF|SR|IL | TNF
 *   [type_length]
 *   [payload_length]          — 1 byte for SR, 4 bytes otherwise
 *   [type]
 *   [payload]
 *   [0xFE]                    — Terminator TLV
 */
static size_t build_tlv(uint8_t *out, size_t out_size,
                        uint8_t tnf,
                        const uint8_t *type, uint8_t type_len,
                        const uint8_t *payload, size_t payload_len)
{
    /* Calculate NDEF record size */
    bool short_record = (payload_len <= 255);
    size_t record_len = 1 /* header */ + 1 /* type_len */ +
                        (short_record ? 1 : 4) /* payload_len */ +
                        type_len + payload_len;

    /* TLV wrapper: type (1) + length (1 or 3) + record + terminator (1) */
    bool short_tlv = (record_len <= 254);
    size_t total = 1 + (short_tlv ? 1 : 3) + record_len + 1;

    if (out == NULL || out_size < total || type == NULL)
        return 0;

    size_t pos = 0;

    /* TLV type */
    out[pos++] = 0x03;  /* NDEF Message TLV */

    /* TLV length */
    if (short_tlv)
    {
        out[pos++] = (uint8_t)record_len;
    }
    else
    {
        out[pos++] = 0xFF;
        out[pos++] = (uint8_t)(record_len >> 8);
        out[pos++] = (uint8_t)(record_len & 0xFF);
    }

    /* NDEF record header:
     *   MB=1, ME=1, CF=0, SR=short_record, IL=0, TNF=tnf
     *   bits: 1 1 0 SR 0 TNF[2:0] */
    uint8_t header = 0xC0 | (short_record ? 0x10 : 0x00) | (tnf & 0x07);
    out[pos++] = header;

    /* Type length */
    out[pos++] = type_len;

    /* Payload length */
    if (short_record)
    {
        out[pos++] = (uint8_t)payload_len;
    }
    else
    {
        out[pos++] = (uint8_t)((payload_len >> 24) & 0xFF);
        out[pos++] = (uint8_t)((payload_len >> 16) & 0xFF);
        out[pos++] = (uint8_t)((payload_len >> 8) & 0xFF);
        out[pos++] = (uint8_t)(payload_len & 0xFF);
    }

    /* Type */
    memcpy(&out[pos], type, type_len);
    pos += type_len;

    /* Payload */
    if (payload_len > 0 && payload != NULL)
    {
        memcpy(&out[pos], payload, payload_len);
        pos += payload_len;
    }

    /* Terminator TLV */
    out[pos++] = 0xFE;

    return pos;
}

/*──────────── URI Record ────────────*/

size_t ndef_encode_uri(uint8_t *out, size_t out_size,
                       ndef_uri_code_t uri_code, const char *uri)
{
    if (uri == NULL)
        return 0;

    size_t uri_len = strlen(uri);

    /* Payload: [uri_code][uri_data...] */
    size_t payload_len = 1 + uri_len;
    uint8_t payload[NDEF_MAX_MESSAGE_SIZE];
    if (payload_len > sizeof(payload))
        return 0;

    payload[0] = (uint8_t)uri_code;
    memcpy(&payload[1], uri, uri_len);

    /* TNF=0x01 (NFC Forum well-known type), Type='U' */
    uint8_t type = 'U';
    return build_tlv(out, out_size, 0x01, &type, 1, payload, payload_len);
}

/*──────────── URI Auto-Detect ────────────*/

/* Prefix table — longest match first to avoid partial matches */
static const struct {
    const char *prefix;
    ndef_uri_code_t code;
} uri_prefix_table[] = {
    { "https://www.", NDEF_URI_HTTPS_WWW },
    { "http://www.",  NDEF_URI_HTTP_WWW  },
    { "https://",    NDEF_URI_HTTPS     },
    { "http://",     NDEF_URI_HTTP      },
    { "tel:",        NDEF_URI_TEL       },
    { "mailto:",     NDEF_URI_MAILTO    },
};
#define URI_PREFIX_COUNT  (sizeof(uri_prefix_table) / sizeof(uri_prefix_table[0]))

size_t ndef_encode_uri_auto(uint8_t *out, size_t out_size, const char *full_uri)
{
    if (full_uri == NULL)
        return 0;

    /* Try to match a known prefix */
    for (size_t i = 0; i < URI_PREFIX_COUNT; i++)
    {
        size_t plen = strlen(uri_prefix_table[i].prefix);
        if (strncmp(full_uri, uri_prefix_table[i].prefix, plen) == 0)
        {
            return ndef_encode_uri(out, out_size,
                                   uri_prefix_table[i].code,
                                   full_uri + plen);
        }
    }

    /* No match — use NDEF_URI_NONE (no compression) */
    return ndef_encode_uri(out, out_size, NDEF_URI_NONE, full_uri);
}

/*──────────── Text Record ────────────*/

size_t ndef_encode_text(uint8_t *out, size_t out_size,
                        const char *lang, const char *text)
{
    if (lang == NULL || text == NULL)
        return 0;

    size_t lang_len = strlen(lang);
    if (lang_len > 63)
        return 0;  /* Language code too long (6 bits max) */

    size_t text_len = strlen(text);
    size_t payload_len = 1 + lang_len + text_len;

    uint8_t payload[NDEF_MAX_MESSAGE_SIZE];
    if (payload_len > sizeof(payload))
        return 0;

    /* Status byte: bit 7 = UTF-8 (0), bits 5:0 = lang code length */
    payload[0] = (uint8_t)(lang_len & 0x3F);
    memcpy(&payload[1], lang, lang_len);
    memcpy(&payload[1 + lang_len], text, text_len);

    /* TNF=0x01 (NFC Forum well-known type), Type='T' */
    uint8_t type = 'T';
    return build_tlv(out, out_size, 0x01, &type, 1, payload, payload_len);
}

/*──────────── WiFi Simple Config ────────────*/

/*
 * WiFi Simple Configuration NDEF record uses:
 *   TNF=0x02 (Media type)
 *   Type="application/vnd.wfa.wsc"
 *
 * Payload is a series of TLV attributes (big-endian 2-byte type + 2-byte length):
 *   0x100E (Credential) — wraps:
 *     0x1026 (Network Index) = 0x01
 *     0x1045 (SSID)
 *     0x1003 (Auth Type) = 0x0020 (WPA2-Personal) or 0x0001 (Open)
 *     0x100F (Encryption Type) = 0x0008 (AES) or 0x0001 (None)
 *     0x1027 (Network Key)
 *     0x1028 (MAC Address) = FF:FF:FF:FF:FF:FF (broadcast)
 */

/* Write a WiFi attribute TLV into buf at pos, return new pos.
 * Returns 0 if the write would exceed buf_size. */
static size_t wifi_attr(uint8_t *buf, size_t buf_size, size_t pos,
                        uint16_t attr_id,
                        const uint8_t *data, uint16_t data_len)
{
    size_t need = 4 + (size_t)data_len;   /* 2 id + 2 len + payload */
    if (pos + need > buf_size)
        return 0;

    buf[pos++] = (uint8_t)(attr_id >> 8);
    buf[pos++] = (uint8_t)(attr_id & 0xFF);
    buf[pos++] = (uint8_t)(data_len >> 8);
    buf[pos++] = (uint8_t)(data_len & 0xFF);
    if (data_len > 0 && data != NULL)
    {
        memcpy(&buf[pos], data, data_len);
        pos += data_len;
    }
    return pos;
}

size_t ndef_encode_wifi(uint8_t *out, size_t out_size,
                        const char *ssid, const char *password,
                        bool auth_wpa2)
{
    if (ssid == NULL)
        return 0;

    size_t ssid_len = strlen(ssid);
    size_t pass_len = (password != NULL) ? strlen(password) : 0;

    /* WiFi SSID max = 32 bytes; WPA2 password max = 63 bytes.
     * Also guard against overflowing the local stack buffers. */
    if (ssid_len > 32 || pass_len > 63)
        return 0;

    /* Build inner credential attributes */
    uint8_t cred[256];
    size_t cpos = 0;

    /* Network Index = 0x01 */
    uint8_t net_idx = 0x01;
    cpos = wifi_attr(cred, sizeof(cred), cpos, 0x1026, &net_idx, 1);
    if (cpos == 0) return 0;

    /* SSID */
    cpos = wifi_attr(cred, sizeof(cred), cpos, 0x1045, (const uint8_t *)ssid, (uint16_t)ssid_len);
    if (cpos == 0) return 0;

    /* Auth Type */
    uint8_t auth[2] = { 0x00, auth_wpa2 ? 0x20 : 0x01 };
    cpos = wifi_attr(cred, sizeof(cred), cpos, 0x1003, auth, 2);
    if (cpos == 0) return 0;

    /* Encryption Type */
    uint8_t enc[2] = { 0x00, auth_wpa2 ? 0x08 : 0x01 };
    cpos = wifi_attr(cred, sizeof(cred), cpos, 0x100F, enc, 2);
    if (cpos == 0) return 0;

    /* Network Key */
    if (pass_len > 0)
        cpos = wifi_attr(cred, sizeof(cred), cpos, 0x1027, (const uint8_t *)password, (uint16_t)pass_len);
    else
        cpos = wifi_attr(cred, sizeof(cred), cpos, 0x1027, NULL, 0);
    if (cpos == 0) return 0;

    /* MAC Address = FF:FF:FF:FF:FF:FF */
    uint8_t mac[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    cpos = wifi_attr(cred, sizeof(cred), cpos, 0x1028, mac, 6);
    if (cpos == 0) return 0;

    /* Wrap in Credential attribute */
    uint8_t payload[300];
    size_t ppos = 0;
    ppos = wifi_attr(payload, sizeof(payload), ppos, 0x100E, cred, (uint16_t)cpos);
    if (ppos == 0) return 0;

    /* TNF=0x02 (Media type), Type="application/vnd.wfa.wsc" */
    const uint8_t type[] = "application/vnd.wfa.wsc";
    return build_tlv(out, out_size, 0x02, type, sizeof(type) - 1,
                     payload, ppos);
}

/*──────────── Phone Number ────────────*/

size_t ndef_encode_phone(uint8_t *out, size_t out_size, const char *phone)
{
    return ndef_encode_uri(out, out_size, NDEF_URI_TEL, phone);
}
