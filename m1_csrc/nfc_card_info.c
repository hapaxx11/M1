/* See COPYING.txt for license details. */

/**
 * @file   nfc_card_info.c
 * @brief  Pure-logic NFC card classification helpers — no HAL, RTOS, or display deps.
 *
 * Extracted from m1_nfc.c following the Preferred Modularization Pattern.
 *
 * M1 Project
 */

#include "nfc_card_info.h"
#include <stdio.h>
#include <string.h>

/* =========================================================================
 * nfc_manufacturer_name
 * =========================================================================*/

const char *nfc_manufacturer_name(uint8_t mfr_byte)
{
    switch (mfr_byte) {
        case 0x01: return "Motorola";
        case 0x02: return "STMicro";
        case 0x03: return "Hitachi";
        case 0x04: return "NXP";
        case 0x05: return "Infineon";
        case 0x06: return "Cylink";
        case 0x07: return "TI";
        case 0x08: return "Fujitsu";
        case 0x09: return "Matsushita";
        case 0x0A: return "NEC";
        case 0x0B: return "Oki";
        case 0x0C: return "Toshiba";
        case 0x0D: return "Mitsubishi";
        case 0x0E: return "Samsung";
        case 0x0F: return "Hyundai";
        case 0x10: return "LG";
        case 0x16: return "EM Micro";
        case 0x28: return "SiliconCraft";
        default:   return "Unknown";
    }
}

/* =========================================================================
 * nfc_sak_type_str
 * =========================================================================*/

const char *nfc_sak_type_str(uint8_t sak, const uint8_t atqa[2])
{
    if (sak == 0x08) return "Classic 1K";
    if (sak == 0x18) return "Classic 4K";
    if (sak == 0x09) return "Classic Mini";
    if (sak == 0x10) return "Classic 2K";
    if (sak == 0x11) return "Classic 4K (Plus)";
    if (sak == 0x00 && atqa[0] == 0x44) return "Ultralight/NTAG";
    if (sak == 0x20) return "DESFire/ISO-DEP";
    if (sak == 0x28) return "Classic+ISO-DEP";
    if (sak == 0x60) return "Classic+DESFire";
    return "Other";
}

/* =========================================================================
 * nfc_uid_fmt
 * =========================================================================*/

void nfc_uid_fmt(const uint8_t *uid, uint8_t uid_len,
                 char *out, size_t out_size)
{
    if (!out || out_size == 0)
        return;

    out[0] = '\0';

    if (!uid || uid_len == 0)
        return;

    size_t pos = 0;
    for (uint8_t i = 0; i < uid_len; i++) {
        /* Each byte needs 3 chars ("XX ") except the last needs 2 ("XX\0") */
        size_t needed = (i + 1u < uid_len) ? 3u : 3u; /* always 3: "XX " + NUL guard */
        if (pos + needed > out_size)
            break;
        int written = snprintf(out + pos, out_size - pos,
                               (i + 1u < uid_len) ? "%02X " : "%02X",
                               uid[i]);
        if (written <= 0)
            break;
        pos += (size_t)written;
    }
    /* snprintf always null-terminates, so out is safe */
}

/* =========================================================================
 * nfc_uid_step
 * =========================================================================*/

void nfc_uid_step(uint8_t *uid, uint8_t uid_len, int8_t dir)
{
    if (!uid || uid_len == 0 || dir == 0)
        return;

    if (dir > 0) {
        for (int i = (int)uid_len - 1; i >= 0; i--) {
            uid[i]++;
            if (uid[i] != 0) break;
        }
    } else {
        for (int i = (int)uid_len - 1; i >= 0; i--) {
            uid[i]--;
            if (uid[i] != 0xFF) break;
        }
    }
}
