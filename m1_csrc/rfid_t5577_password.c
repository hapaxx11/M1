/* See COPYING.txt for license details. */

#include "rfid_t5577_password.h"

#include <string.h>

static int hex_nibble(char ch)
{
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return -1;
}

bool rfid_t5577_password_parse(const char *text, uint32_t *password)
{
    if (!text || !password || strlen(text) != 8) return false;

    uint32_t parsed = 0;
    for (unsigned i = 0; i < 8; i++) {
        int nibble = hex_nibble(text[i]);
        if (nibble < 0) return false;
        parsed = (parsed << 4) | (uint32_t)nibble;
    }

    *password = parsed;
    return true;
}
