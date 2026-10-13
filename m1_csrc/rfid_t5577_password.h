/* See COPYING.txt for license details. */

#ifndef RFID_T5577_PASSWORD_H_
#define RFID_T5577_PASSWORD_H_

#include <stdbool.h>
#include <stdint.h>

#define RFID_T5577_PASSWORD_CLEAR_WRITE_COUNT 2u

typedef struct {
    uint8_t page;
    uint8_t block;
    bool lock_bit;
    uint32_t data;
    bool with_password;
    uint32_t password;
} rfid_t5577_password_clear_write_t;

bool rfid_t5577_password_parse(const char *text, uint32_t *password);
bool rfid_t5577_password_clear_write(
    uint8_t index,
    uint32_t password,
    rfid_t5577_password_clear_write_t *write);

#endif
