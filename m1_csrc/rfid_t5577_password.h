/* See COPYING.txt for license details. */

#ifndef RFID_T5577_PASSWORD_H_
#define RFID_T5577_PASSWORD_H_

#include <stdbool.h>
#include <stdint.h>

bool rfid_t5577_password_parse(const char *text, uint32_t *password);

#endif
