/* See COPYING.txt for license details. */

#ifndef SUBGHZ_SAVED_MATCH_H_
#define SUBGHZ_SAVED_MATCH_H_

#include <stdbool.h>
#include <stdint.h>

#define SUBGHZ_SAVED_MATCH_COUNTER_MARGIN  50u
#define SUBGHZ_SAVED_MATCH_PROTOCOL_MAX_LEN 32u

typedef struct {
    char protocol[SUBGHZ_SAVED_MATCH_PROTOCOL_MAX_LEN];
    uint64_t key;
    uint32_t serial;
    uint32_t counter;
    bool has_key;
    bool has_serial;
    bool has_counter;
    bool allow_serial_match;
    bool is_recovered_psa;
} subghz_saved_match_identity_t;

bool subghz_saved_match_counter_compatible(
    const subghz_saved_match_identity_t *received,
    const subghz_saved_match_identity_t *saved);

bool subghz_saved_match_identity(
    const subghz_saved_match_identity_t *received,
    const subghz_saved_match_identity_t *saved);

#endif /* SUBGHZ_SAVED_MATCH_H_ */
