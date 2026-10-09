/* See COPYING.txt for license details. */

#include "subghz_protopirate_keys_builtin.h"

#if defined(__GNUC__) && __GNUC__ >= 11
#define M1_KEY_DATA_RETAIN __attribute__((used, retain))
#elif defined(__GNUC__)
#define M1_KEY_DATA_RETAIN __attribute__((used))
#else
#define M1_KEY_DATA_RETAIN
#endif

M1_KEY_DATA_RETAIN const uint64_t m1_kia_v6_keys_builtin[M1_KIA_V6_KEY_COUNT] = {0, 0};
M1_KEY_DATA_RETAIN const bool m1_kia_v6_keys_builtin_available = false;
M1_KEY_DATA_RETAIN const uint8_t m1_vag_aut64_keys_builtin[M1_VAG_AUT64_KEY_BYTES] = {0};
M1_KEY_DATA_RETAIN const bool m1_vag_aut64_keys_builtin_available = false;
