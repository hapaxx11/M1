/* See COPYING.txt for license details. */

#ifndef SUBGHZ_PROTOPIRATE_KEYS_BUILTIN_H
#define SUBGHZ_PROTOPIRATE_KEYS_BUILTIN_H

#include <stdbool.h>
#include <stdint.h>

#define M1_KIA_V6_KEY_COUNT 2u
#define M1_VAG_AUT64_KEY_BYTES 48u

extern const uint64_t m1_kia_v6_keys_builtin[M1_KIA_V6_KEY_COUNT];
extern const bool m1_kia_v6_keys_builtin_available;
extern const uint8_t m1_vag_aut64_keys_builtin[M1_VAG_AUT64_KEY_BYTES];
extern const bool m1_vag_aut64_keys_builtin_available;

/* ProtoPirate keystore type 10 (KIA_KEY1): Kia V3/V4 manufacturer key used
 * for direct KeeLoq MF-key frame validation/encryption. */
extern const uint64_t m1_kia_mf_key_builtin;
extern const bool m1_kia_mf_key_builtin_available;
/* ProtoPirate keystore type 13 (KIA_KEY4): Kia V5 counter mixer key. */
extern const uint64_t m1_kia_v5_key_builtin;
extern const bool m1_kia_v5_key_builtin_available;

#endif
