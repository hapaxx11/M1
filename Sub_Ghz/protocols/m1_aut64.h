/* See COPYING.txt for license details. */

#ifndef M1_AUT64_H
#define M1_AUT64_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t index;
    uint8_t key[8];
    uint8_t pbox[8];
    uint8_t sbox[16];
} m1_aut64_key_t;

bool m1_aut64_unpack(m1_aut64_key_t *key, const uint8_t packed[16]);
void m1_aut64_decrypt(const m1_aut64_key_t *key, uint8_t block[8]);

#endif
