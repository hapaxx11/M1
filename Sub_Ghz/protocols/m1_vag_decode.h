/* See COPYING.txt for license details. */

#ifndef M1_VAG_DECODE_H
#define M1_VAG_DECODE_H

#include <stdbool.h>
#include <stdint.h>

bool m1_vag_t12_parse(uint16_t prefix, uint64_t key1_raw, uint16_t key2_raw,
                      uint8_t *type, uint8_t *button, uint64_t *key1_out);
bool m1_vag_t12_decrypt(uint8_t type, uint64_t key1, uint16_t key2,
                        const uint8_t *packed_keys, bool keys_available,
                        uint32_t *serial, uint32_t *counter, uint8_t *button);

#endif
