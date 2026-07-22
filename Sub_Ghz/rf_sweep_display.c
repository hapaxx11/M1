/* See COPYING.txt for license details. */

/*
 * rf_sweep_display.c
 *
 * Pure-logic display formatting for Signal Identifier results.
 * See rf_sweep_display.h.  No hardware dependencies.
 *
 * M1 Project — Hapax fork
 */

#include "rf_sweep_display.h"

#include <stdio.h>
#include <string.h>

/*----------------------------------------------------------------------------*/
/* Helpers                                                                    */
/*----------------------------------------------------------------------------*/

const char *rf_sweep_display_security_prefix(rf_security_t sec)
{
    switch (sec) {
        case RF_SEC_FIXED:     return "F:";
        case RF_SEC_ROLLING:   return "R:";
        case RF_SEC_ENCRYPTED: return "E:";
        default:               return "";
    }
}

void rf_sweep_display_strip_tags(char *buf, uint16_t buf_len, const char *name)
{
    if (buf == NULL || buf_len == 0)
        return;
    buf[0] = '\0';

    if (name == NULL)
        return;

    uint16_t out = 0;
    const char *src = name;

    while (*src != '\0' && out < buf_len - 1) {
        /* Detect " (" pattern indicating a parenthetical tag. */
        if (src[0] == ' ' && src[1] == '(') {
            /* Skip forward to closing ')' or end of string. */
            const char *close = strchr(src + 2, ')');
            if (close != NULL) {
                src = close + 1;
                continue;
            }
            /* No closing paren — treat literally. */
        }
        buf[out++] = *src++;
    }
    buf[out] = '\0';

    /* Trim trailing whitespace that may remain after tag removal. */
    while (out > 0 && buf[out - 1] == ' ')
        buf[--out] = '\0';
}

/*----------------------------------------------------------------------------*/
/* Public API                                                                 */
/*----------------------------------------------------------------------------*/

void rf_sweep_display_format_hit(char *buf, uint16_t buf_len,
                                 const rf_sweep_hit_t *hit,
                                 uint8_t min_hits)
{
    if (buf == NULL || buf_len == 0)
        return;
    buf[0] = '\0';

    if (hit == NULL)
        return;

    /* Determine the name source: decode-confirmed hits use decode_name;
     * fingerprint-scored hits use the database signature name. */
    const char *name_src = NULL;
    bool is_decoded = (hit->sig == NULL && hit->decode_name != NULL);
    if (is_decoded)
        name_src = hit->decode_name;
    else if (hit->sig != NULL)
        name_src = hit->sig->name;
    else
        return;  /* no name available */

    /* Format frequency as MHz (e.g. "433.92"). */
    uint32_t mhz_int = hit->freq_hz / 1000000UL;
    uint32_t mhz_frac = (hit->freq_hz % 1000000UL) / 10000UL; /* 2 decimal digits */
    char freq_str[10];
    snprintf(freq_str, sizeof(freq_str), "%lu.%02lu",
             (unsigned long)mhz_int, (unsigned long)mhz_frac);

    /* Security prefix — omitted for decode-confirmed hits (security unknown). */
    const char *sec_pfx = is_decoded ? "" : rf_sweep_display_security_prefix(hit->security);

    /* Strip tags from the name. */
    char clean_name[24];
    rf_sweep_display_strip_tags(clean_name, sizeof(clean_name), name_src);

    /* Confidence string.  Decode-confirmed hits always show their confidence
     * (100%) regardless of hit count — a decode is definitive. */
    char conf_str[6];
    if (is_decoded || hit->hits >= min_hits)
        snprintf(conf_str, sizeof(conf_str), "%u%%", (unsigned)hit->confidence);
    else
        snprintf(conf_str, sizeof(conf_str), "?%%");

    /* Assemble: "{freq} {sec}{name} {conf}" */
    snprintf(buf, buf_len, "%s %s%s %s",
             freq_str, sec_pfx, clean_name, conf_str);
}
