/* See COPYING.txt for license details. */

/**
 * @file  subghz_keeloq_mfkeys.c
 * @brief KeeLoq manufacturer (master) key store — FatFS backed implementation.
 *
 * Loads manufacturer keys from 0:/SUBGHZ/keeloq_mfcodes at runtime.
 *
 * Supported formats:
 *
 * 1. Compact (Flipper-compatible plain-text):
 *      AABBCCDDEEFFAABB:TYPE:Name
 *    where TYPE is 1 (Simple), 2 (Normal), or 3 (Secure) learning.
 *
 * 2. RocketGod SubGHz Toolkit multi-line format:
 *      Manufacturer: Name
 *      Key (Hex):    AABBCCDDEEFFAABB
 *      Key (Dec):    <decimal — ignored>
 *      Type:         TYPE
 *      ------------------------------------
 *    As exported by RocketGod's SubGHz Toolkit for Flipper Zero.
 *    Use scripts/convert_keeloq_keys.py to convert to compact format.
 */

#include "subghz_keeloq_mfkeys.h"

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "ff.h"

/*============================================================================*/
/* Internal table                                                              */
/*============================================================================*/

#define KEELOQ_MFKEYS_MAX   512     /**< Maximum entries in the in-memory table */
#define KEELOQ_LINE_MAX     128     /**< Maximum line length in the keystore file */

static KeeLoqMfrEntry *s_table   = NULL;
static uint32_t        s_count   = 0;

/*============================================================================*/
/* Helpers                                                                     */
/*============================================================================*/

/** Case-insensitive character comparison. */
static int ci_strcmp(const char *a, const char *b)
{
    while (*a && *b) {
        int ca = tolower((unsigned char)*a);
        int cb = tolower((unsigned char)*b);
        if (ca != cb) return ca - cb;
        a++; b++;
    }
    return tolower((unsigned char)*a) - tolower((unsigned char)*b);
}

/**
 * Parse a hex string into a uint64_t (big-endian, exactly 16 hex digits).
 * Returns false if the string does not contain exactly 16 hex digits
 * followed immediately by a ':' separator.
 */
static bool parse_hex64(const char *p, uint64_t *out)
{
    uint64_t v = 0;
    int digits = 0;
    while (*p && digits < 16) {
        char c = *p++;
        uint8_t nibble;
        if (c >= '0' && c <= '9')       nibble = (uint8_t)(c - '0');
        else if (c >= 'A' && c <= 'F')  nibble = (uint8_t)(c - 'A' + 10);
        else if (c >= 'a' && c <= 'f')  nibble = (uint8_t)(c - 'a' + 10);
        else return false;
        v = (v << 4) | nibble;
        digits++;
    }
    /* Must have exactly 16 hex digits followed by ':' */
    if (digits != 16 || *p != ':') return false;
    *out = v;
    return true;
}

/**
 * Parse a standalone hex string into a uint64_t (big-endian, exactly 16
 * hex digits, terminated by end-of-string or whitespace — no ':' required).
 *
 * Any non-hex character (including whitespace mid-string) stops scanning and
 * causes the 16-digit check to fail, returning false.  A string like
 * "0123456789ABCDEF" succeeds; "0123456789AB CDEF" fails at the space.
 */
static bool parse_hex64_standalone(const char *p, uint64_t *out)
{
    uint64_t v = 0;
    int digits = 0;
    /* Accept only printable hex digits — any other character (including
     * whitespace) terminates the loop; if we didn't reach 16 digits by then,
     * the check below rejects the value. */
    for (; digits < 16; digits++, p++) {
        char c = *p;
        uint8_t nibble;
        if (c >= '0' && c <= '9')       nibble = (uint8_t)(c - '0');
        else if (c >= 'A' && c <= 'F')  nibble = (uint8_t)(c - 'A' + 10);
        else if (c >= 'a' && c <= 'f')  nibble = (uint8_t)(c - 'a' + 10);
        else break;  /* non-hex character — stop */
        v = (v << 4) | nibble;
    }
    if (digits != 16) return false;
    *out = v;
    return true;
}

/** Try to append a RocketGod-format entry to the table. */
static void rg_try_emit(const char *rg_name, const char *rg_hex,
                        int rg_type, bool has_name, bool has_hex, bool has_type)
{
    if (!has_name || !has_hex || !has_type) return;
    if (s_count >= KEELOQ_MFKEYS_MAX) return;
    if (rg_type < KEELOQ_LEARN_SIMPLE || rg_type > KEELOQ_LEARN_SECURE) return;

    uint64_t mfr_key = 0;
    if (!parse_hex64_standalone(rg_hex, &mfr_key)) return;

    KeeLoqMfrEntry *e = &s_table[s_count];
    e->key        = mfr_key;
    e->learn_type = (KeeLoqLearnType)rg_type;
    strncpy(e->name, rg_name, sizeof(e->name) - 1);
    e->name[sizeof(e->name) - 1] = '\0';
    s_count++;
}

/*============================================================================*/
/* keeloq_mfkeys_load_text()                                                  */
/*============================================================================*/

bool keeloq_mfkeys_load_text(const char *text)
{
    if (!text) return false;

    /* Free any previously loaded table */
    keeloq_mfkeys_free();

    /* Allocate the entry table */
    s_table = (KeeLoqMfrEntry *)malloc(KEELOQ_MFKEYS_MAX * sizeof(KeeLoqMfrEntry));
    if (!s_table) return false;
    s_count = 0;

    /* State for RocketGod multi-line format */
    char rg_name[48] = {'\0'};
    char rg_hex[20]  = {'\0'};
    int  rg_type_val = 0;
    bool rg_has_name = false;
    bool rg_has_hex  = false;
    bool rg_has_type = false;

    char line[KEELOQ_LINE_MAX];
    const char *pos = text;

    while (*pos && s_count < KEELOQ_MFKEYS_MAX) {
        /* Extract one line from the text buffer */
        size_t i = 0;
        while (*pos && *pos != '\n' && i < sizeof(line) - 1) {
            line[i++] = *pos++;
        }
        if (*pos == '\n') pos++;
        line[i] = '\0';

        /* Strip trailing whitespace / CRLF */
        while (i > 0 && (line[i-1] == '\r' || line[i-1] == '\n' ||
                          line[i-1] == ' '))
            line[--i] = '\0';

        /* Skip blank lines and comments */
        if (i == 0 || line[0] == '#') continue;

        /* Skip Flipper keystore header lines */
        if (strncmp(line, "Filetype:",   9) == 0 ||
            strncmp(line, "Version:",    8) == 0 ||
            strncmp(line, "Encryption:", 11) == 0)
            continue;

        /* ── RocketGod multi-line format detection ─────────────────── */

        if (strncmp(line, "Manufacturer:", 13) == 0) {
            /* New RocketGod entry — reset pending state */
            const char *v = line + 13;
            while (*v == ' ') v++;
            strncpy(rg_name, v, sizeof(rg_name) - 1);
            rg_name[sizeof(rg_name) - 1] = '\0';
            rg_has_name = (rg_name[0] != '\0');
            rg_has_hex  = false;
            rg_has_type = false;
            continue;
        }

        if (strncmp(line, "Key (Hex):", 10) == 0) {
            /* e.g. "Key (Hex):    0123456789ABCDEF"
             * The strncmp guard above already verifies the ':' is present.
             * Skip past the full "Key (Hex):" prefix (10 chars) then spaces. */
            const char *v = line + 10;
            while (*v == ' ') v++;
            strncpy(rg_hex, v, sizeof(rg_hex) - 1);
            rg_hex[sizeof(rg_hex) - 1] = '\0';
            rg_has_hex = (rg_hex[0] != '\0');
            continue;
        }

        if (strncmp(line, "Key (Dec):", 10) == 0) {
            /* Decimal key — present in RocketGod output, ignored by M1 */
            continue;
        }

        if (rg_has_name && strncmp(line, "Type:", 5) == 0) {
            const char *v = line + 5;
            while (*v == ' ') v++;
            if (*v >= '0' && *v <= '9') {
                rg_type_val = *v - '0';
                rg_has_type = true;
            }
            /* Emit entry immediately if we have all fields */
            if (rg_has_name && rg_has_hex && rg_has_type) {
                rg_try_emit(rg_name, rg_hex, rg_type_val,
                            rg_has_name, rg_has_hex, rg_has_type);
                rg_has_name = rg_has_hex = rg_has_type = false;
            }
            continue;
        }

        if (line[0] == '-' && line[1] == '-') {
            /* RocketGod separator — emit pending entry if complete */
            rg_try_emit(rg_name, rg_hex, rg_type_val,
                        rg_has_name, rg_has_hex, rg_has_type);
            rg_has_name = rg_has_hex = rg_has_type = false;
            continue;
        }

        /* ── Compact format: AABBCCDDEEFFAABB:TYPE:Name ─────────────── */

        char *p = line;

        /* parse_hex64 consumes exactly 16 hex digits and verifies ':' follows */
        uint64_t mfr_key = 0;
        if (!parse_hex64(p, &mfr_key)) continue;

        /* Advance p past the 16 hex digits to the first ':' */
        p += 16;
        p++;  /* skip ':' */

        /* Parse learn type digit */
        if (!(*p >= '0' && *p <= '9')) continue;
        int learn_type = *p - '0';
        if (learn_type < KEELOQ_LEARN_SIMPLE || learn_type > KEELOQ_LEARN_SECURE)
            continue;
        p++;

        /* Skip ':' separator */
        if (*p != ':') continue;
        p++;

        /* Skip leading whitespace before name */
        while (*p == ' ') p++;

        /* Name must be non-empty */
        if (*p == '\0') continue;

        /* Populate entry */
        KeeLoqMfrEntry *e = &s_table[s_count];
        e->key        = mfr_key;
        e->learn_type = (KeeLoqLearnType)learn_type;
        strncpy(e->name, p, sizeof(e->name) - 1);
        e->name[sizeof(e->name) - 1] = '\0';
        s_count++;
    }

    /* Emit any trailing RocketGod entry not terminated by a separator */
    rg_try_emit(rg_name, rg_hex, rg_type_val,
                rg_has_name, rg_has_hex, rg_has_type);

    return true;  /* success even if 0 entries parsed */
}

/*============================================================================*/
/* keeloq_mfkeys_load()                                                       */
/*============================================================================*/

bool keeloq_mfkeys_load(void)
{
    /* Free any previously loaded table */
    keeloq_mfkeys_free();

    FIL f;
    FRESULT fr = f_open(&f, KEELOQ_MFKEYS_PATH, FA_READ);
    if (fr != FR_OK)
        return false;

    /* Allocate the entry table */
    s_table = (KeeLoqMfrEntry *)malloc(KEELOQ_MFKEYS_MAX * sizeof(KeeLoqMfrEntry));
    if (!s_table) {
        f_close(&f);
        return false;
    }
    s_count = 0;

    /* State for RocketGod multi-line format */
    char rg_name[48] = {'\0'};
    char rg_hex[20]  = {'\0'};
    int  rg_type_val = 0;
    bool rg_has_name = false;
    bool rg_has_hex  = false;
    bool rg_has_type = false;

    char line[KEELOQ_LINE_MAX];

    while (f_gets(line, sizeof(line), &f) && s_count < KEELOQ_MFKEYS_MAX)
    {
        /* Strip trailing whitespace / CRLF */
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\r' || line[len-1] == '\n' ||
                            line[len-1] == ' '))
            line[--len] = '\0';

        /* Skip blank lines and comments */
        if (len == 0 || line[0] == '#') continue;

        /* Skip Flipper keystore header lines */
        if (strncmp(line, "Filetype:",   9) == 0 ||
            strncmp(line, "Version:",    8) == 0 ||
            strncmp(line, "Encryption:", 11) == 0)
            continue;

        /* ── RocketGod multi-line format detection ─────────────────── */

        if (strncmp(line, "Manufacturer:", 13) == 0) {
            const char *v = line + 13;
            while (*v == ' ') v++;
            strncpy(rg_name, v, sizeof(rg_name) - 1);
            rg_name[sizeof(rg_name) - 1] = '\0';
            rg_has_name = (rg_name[0] != '\0');
            rg_has_hex  = false;
            rg_has_type = false;
            continue;
        }

        if (strncmp(line, "Key (Hex):", 10) == 0) {
            /* The strncmp guard above already verifies the ':' is present.
             * Skip past the full "Key (Hex):" prefix (10 chars) then spaces. */
            const char *v = line + 10;
            while (*v == ' ') v++;
            strncpy(rg_hex, v, sizeof(rg_hex) - 1);
            rg_hex[sizeof(rg_hex) - 1] = '\0';
            rg_has_hex = (rg_hex[0] != '\0');
            continue;
        }

        if (strncmp(line, "Key (Dec):", 10) == 0) {
            continue;
        }

        if (rg_has_name && strncmp(line, "Type:", 5) == 0) {
            const char *v = line + 5;
            while (*v == ' ') v++;
            if (*v >= '0' && *v <= '9') {
                rg_type_val = *v - '0';
                rg_has_type = true;
            }
            if (rg_has_name && rg_has_hex && rg_has_type) {
                rg_try_emit(rg_name, rg_hex, rg_type_val,
                            rg_has_name, rg_has_hex, rg_has_type);
                rg_has_name = rg_has_hex = rg_has_type = false;
            }
            continue;
        }

        if (line[0] == '-' && line[1] == '-') {
            rg_try_emit(rg_name, rg_hex, rg_type_val,
                        rg_has_name, rg_has_hex, rg_has_type);
            rg_has_name = rg_has_hex = rg_has_type = false;
            continue;
        }

        /* ── Compact format: AABBCCDDEEFFAABB:TYPE:Name ─────────────── */

        char *p = line;

        uint64_t mfr_key = 0;
        if (!parse_hex64(p, &mfr_key)) continue;

        p += 16;
        p++;  /* skip ':' */

        if (!(*p >= '0' && *p <= '9')) continue;
        int learn_type = *p - '0';
        if (learn_type < KEELOQ_LEARN_SIMPLE || learn_type > KEELOQ_LEARN_SECURE)
            continue;
        p++;

        if (*p != ':') continue;
        p++;

        while (*p == ' ') p++;

        if (*p == '\0') continue;

        KeeLoqMfrEntry *e = &s_table[s_count];
        e->key        = mfr_key;
        e->learn_type = (KeeLoqLearnType)learn_type;
        strncpy(e->name, p, sizeof(e->name) - 1);
        e->name[sizeof(e->name) - 1] = '\0';
        s_count++;
    }

    /* Emit any trailing RocketGod entry not terminated by a separator */
    rg_try_emit(rg_name, rg_hex, rg_type_val,
                rg_has_name, rg_has_hex, rg_has_type);

    f_close(&f);
    return true;
}

void keeloq_mfkeys_free(void)
{
    if (s_table) {
        free(s_table);
        s_table = NULL;
    }
    s_count = 0;
}

bool keeloq_mfkeys_find(const char *name, KeeLoqMfrEntry *entry)
{
    if (!s_table || !name || !entry) return false;

    for (uint32_t i = 0; i < s_count; i++) {
        if (ci_strcmp(s_table[i].name, name) == 0) {
            *entry = s_table[i];
            return true;
        }
    }
    return false;
}

uint32_t keeloq_mfkeys_count(void)
{
    return s_count;
}
