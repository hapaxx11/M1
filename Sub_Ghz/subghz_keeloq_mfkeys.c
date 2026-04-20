/* See COPYING.txt for license details. */

/**
 * @file  subghz_keeloq_mfkeys.c
 * @brief KeeLoq manufacturer (master) key store — FatFS backed implementation.
 *
 * Loads manufacturer keys from 0:/SUBGHZ/keeloq_mfcodes at runtime.
 * File format is Flipper-compatible:
 *   AABBCCDDEEFFAABB:TYPE:Name
 * where TYPE is 1 (Simple), 2 (Normal), or 3 (Secure) learning.
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

/*============================================================================*/
/* API implementation                                                          */
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

        /* Skip Flipper keystore header lines:
         * "Filetype: ...", "Version: ...", "Encryption: ..." */
        if (strncmp(line, "Filetype:", 9) == 0 ||
            strncmp(line, "Version:",  8) == 0 ||
            strncmp(line, "Encryption:", 11) == 0)
            continue;

        /* Expected format: AABBCCDDEEFFAABB:TYPE:Name
         * parse_hex64 consumes exactly 16 hex digits and verifies the ':' follows. */
        char *p = line;

        /* Parse 64-bit hex key; p still points at the first char of the key */
        uint64_t mfr_key = 0;
        if (!parse_hex64(p, &mfr_key)) continue;

        /* Advance p past the 16 hex digits to the first ':' */
        p += 16;
        p++;  /* skip ':' */

        /* Parse learn type digit */
        if (!(*p >= '0' && *p <= '9')) continue;
        int learn_type = *p - '0';
        if (learn_type < KEELOQ_LEARN_SIMPLE || learn_type > KEELOQ_LEARN_SECURE)
            continue;  /* unknown type — skip */
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

    f_close(&f);
    return true;  /* success even if 0 entries parsed */
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
