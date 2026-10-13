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

bool rfid_t5577_password_clear_write(
    uint8_t index,
    uint32_t password,
    rfid_t5577_password_clear_write_t *write)
{
    if (!write || index >= RFID_T5577_PASSWORD_CLEAR_WRITE_COUNT)
        return false;

    if (index == 0u) {
        *write = (rfid_t5577_password_clear_write_t) {
            .page = 0u,
            .block = 0u,
            .lock_bit = false,
            .data = 0x00148040u,
            .with_password = true,
            .password = password,
        };
    } else {
        *write = (rfid_t5577_password_clear_write_t) {
            .page = 0u,
            .block = 7u,
            .lock_bit = false,
            .data = 0u,
            .with_password = false,
            .password = 0u,
        };
    }
    return true;
}
