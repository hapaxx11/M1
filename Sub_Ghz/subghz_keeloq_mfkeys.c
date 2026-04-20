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
 * Returns false if the value is not exactly 16 hex digits, if any non-hex
 * character appears before the 16th digit, or if a hex digit or other
 * non-whitespace character immediately follows the 16th digit (e.g., a 17+
 * digit key is rejected).
 */
static bool parse_hex64_standalone(const char *p, uint64_t *out)
{
    uint64_t v = 0;
    int digits = 0;
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
    /* Reject keys immediately followed by another hex digit (17+ digit strings). */
    char next = *p;
    if ((next >= '0' && next <= '9') ||
        (next >= 'A' && next <= 'F') ||
        (next >= 'a' && next <= 'f'))
        return false;
    *out = v;
    return true;
}

/*============================================================================*/
/* Shared line parser                                                          */
/*============================================================================*/

/**
 * In-progress state for the RocketGod multi-line format.
 * Reset when a "Manufacturer:" line is seen or a "---" separator is consumed.
 */
typedef struct {
    char rg_name[48];
    char rg_hex[20];
    int  rg_type_val;
    bool rg_has_name;
    bool rg_has_hex;
    bool rg_has_type;
} RGState;

/** Try to append the accumulated RocketGod entry to the table. */
static void rg_try_emit(RGState *rg)
{
    if (!rg->rg_has_name || !rg->rg_has_hex || !rg->rg_has_type) return;
    if (s_count >= KEELOQ_MFKEYS_MAX) return;
    if (rg->rg_type_val < KEELOQ_LEARN_SIMPLE ||
        rg->rg_type_val > KEELOQ_LEARN_SECURE) return;

    uint64_t mfr_key = 0;
    if (!parse_hex64_standalone(rg->rg_hex, &mfr_key)) return;

    KeeLoqMfrEntry *e = &s_table[s_count];
    e->key        = mfr_key;
    e->learn_type = (KeeLoqLearnType)rg->rg_type_val;
    strncpy(e->name, rg->rg_name, sizeof(e->name) - 1);
    e->name[sizeof(e->name) - 1] = '\0';
    s_count++;
}

/**
 * Parse one stripped (no trailing whitespace/CRLF), null-terminated line
 * into the table.  Updates @p rg with any accumulated RocketGod state.
 *
 * @param line  Stripped line (modified in-place for compact format pointer math).
 * @param rg    Persistent RocketGod multi-line state across calls.
 */
static void parse_mfkeys_line(char *line, RGState *rg)
{
    if (line[0] == '\0' || line[0] == '#') return;

    /* Skip Flipper keystore header lines */
    if (strncmp(line, "Filetype:",   9) == 0 ||
        strncmp(line, "Version:",    8) == 0 ||
        strncmp(line, "Encryption:", 11) == 0)
        return;

    /* ── RocketGod multi-line format detection ─────────────────── */

    if (strncmp(line, "Manufacturer:", 13) == 0) {
        /* New RocketGod entry — emit any pending entry first, then reset */
        rg_try_emit(rg);
        memset(rg, 0, sizeof(*rg));
        const char *v = line + 13;
        while (*v == ' ') v++;
        strncpy(rg->rg_name, v, sizeof(rg->rg_name) - 1);
        rg->rg_name[sizeof(rg->rg_name) - 1] = '\0';
        rg->rg_has_name = (rg->rg_name[0] != '\0');
        return;
    }

    if (strncmp(line, "Key (Hex):", 10) == 0) {
        /* The strncmp guard above already verifies ':' is present.
         * Skip past the full "Key (Hex):" prefix (10 chars) then spaces. */
        const char *v = line + 10;
        while (*v == ' ') v++;
        strncpy(rg->rg_hex, v, sizeof(rg->rg_hex) - 1);
        rg->rg_hex[sizeof(rg->rg_hex) - 1] = '\0';
        rg->rg_has_hex = (rg->rg_hex[0] != '\0');
        return;
    }

    if (strncmp(line, "Key (Dec):", 10) == 0) {
        /* Decimal key — present in RocketGod output, ignored by M1 */
        return;
    }

    if (rg->rg_has_name && strncmp(line, "Type:", 5) == 0) {
        const char *v = line + 5;
        while (*v == ' ') v++;
        char *endptr;
        long parsed = strtol(v, &endptr, 10);
        /* Accept only a pure integer field (no trailing garbage) */
        if (endptr != v && (*endptr == '\0' || *endptr == ' ') &&
            parsed >= KEELOQ_LEARN_SIMPLE && parsed <= KEELOQ_LEARN_SECURE) {
            rg->rg_type_val = (int)parsed;
            rg->rg_has_type = true;
        }
        /* Emit immediately if all fields are now present */
        if (rg->rg_has_name && rg->rg_has_hex && rg->rg_has_type) {
            rg_try_emit(rg);
            memset(rg, 0, sizeof(*rg));
        }
        return;
    }

    if (line[0] == '-' && line[1] == '-') {
        /* RocketGod separator — emit pending entry if complete, then reset */
        rg_try_emit(rg);
        memset(rg, 0, sizeof(*rg));
        return;
    }

    /* ── Compact format: AABBCCDDEEFFAABB:TYPE:Name ─────────────── */

    if (s_count >= KEELOQ_MFKEYS_MAX) return;

    char *p = line;

    /* parse_hex64 consumes exactly 16 hex digits and verifies ':' follows */
    uint64_t mfr_key = 0;
    if (!parse_hex64(p, &mfr_key)) return;

    /* Advance p past the 16 hex digits and the ':' */
    p += 17;

    /* Parse learn type with full integer validation */
    char *endptr;
    long learn_type = strtol(p, &endptr, 10);
    if (endptr == p || *endptr != ':') return;
    if (learn_type < KEELOQ_LEARN_SIMPLE || learn_type > KEELOQ_LEARN_SECURE) return;
    p = endptr + 1;  /* skip ':' */

    /* Skip leading whitespace before name */
    while (*p == ' ') p++;

    /* Name must be non-empty */
    if (*p == '\0') return;

    /* Populate entry */
    KeeLoqMfrEntry *e = &s_table[s_count];
    e->key        = mfr_key;
    e->learn_type = (KeeLoqLearnType)learn_type;
    strncpy(e->name, p, sizeof(e->name) - 1);
    e->name[sizeof(e->name) - 1] = '\0';
    s_count++;
}

/** Allocate the entry table and reset the count. */
static bool prepare_table(void)
{
    keeloq_mfkeys_free();
    s_table = (KeeLoqMfrEntry *)malloc(KEELOQ_MFKEYS_MAX * sizeof(KeeLoqMfrEntry));
    if (!s_table) return false;
    s_count = 0;
    return true;
}

/*============================================================================*/
/* keeloq_mfkeys_load_text()                                                  */
/*============================================================================*/

bool keeloq_mfkeys_load_text(const char *text)
{
    if (!text) return false;
    if (!prepare_table()) return false;

    RGState rg;
    memset(&rg, 0, sizeof(rg));

    char line[KEELOQ_LINE_MAX];
    const char *pos = text;

    while (*pos && s_count < KEELOQ_MFKEYS_MAX) {
        /* Extract one line from the text buffer */
        size_t i = 0;
        while (*pos && *pos != '\n' && i < sizeof(line) - 1)
            line[i++] = *pos++;
        if (*pos == '\n') pos++;
        line[i] = '\0';

        /* Strip trailing whitespace / CRLF */
        while (i > 0 && (line[i-1] == '\r' || line[i-1] == '\n' ||
                          line[i-1] == ' '))
            line[--i] = '\0';

        parse_mfkeys_line(line, &rg);
    }

    /* Emit any trailing RocketGod entry not terminated by a separator */
    rg_try_emit(&rg);

    return true;  /* success even if 0 entries parsed */
}

/*============================================================================*/
/* keeloq_mfkeys_load()                                                       */
/*============================================================================*/

bool keeloq_mfkeys_load(void)
{
    FIL f;
    FRESULT fr = f_open(&f, KEELOQ_MFKEYS_PATH, FA_READ);
    if (fr != FR_OK)
        return false;

    if (!prepare_table()) {
        f_close(&f);
        return false;
    }

    RGState rg;
    memset(&rg, 0, sizeof(rg));

    char line[KEELOQ_LINE_MAX];

    while (f_gets(line, sizeof(line), &f) && s_count < KEELOQ_MFKEYS_MAX) {
        /* Strip trailing whitespace / CRLF */
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\r' || line[len-1] == '\n' ||
                            line[len-1] == ' '))
            line[--len] = '\0';

        parse_mfkeys_line(line, &rg);
    }

    /* Emit any trailing RocketGod entry not terminated by a separator */
    rg_try_emit(&rg);

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
