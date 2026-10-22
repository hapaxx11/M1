/* See COPYING.txt for license details. */

/**
 * @file  protopirate_keys_test_data.c
 * @brief Synthetic ProtoPirate keystore symbols for host unit tests.
 *
 * Provides test definitions of the build-injected Kia V6 / VAG AUT64 key
 * material so host targets that link subghz_proto_pirate.c (which references
 * the symbols through the Kia V6 encoder gate) resolve them.  Values are
 * synthetic; no real key material.  Targets that define their own keystore
 * symbols (test_kia_v6_decode, test_subghz_kia_v6_encoder,
 * test_vag_decode) must NOT link this file.
 */

#include "subghz_protopirate_keys_builtin.h"

const uint64_t m1_kia_v6_keys_builtin[M1_KIA_V6_KEY_COUNT] = {
    0x1122334455667788ULL, 0x99AABBCCDDEEFF00ULL,
};
const bool m1_kia_v6_keys_builtin_available = true;
const uint8_t m1_vag_aut64_keys_builtin[M1_VAG_AUT64_KEY_BYTES] = {0};
const bool m1_vag_aut64_keys_builtin_available = false;
const uint64_t m1_kia_mf_key_builtin = 0x0102030405060708ULL;
const bool m1_kia_mf_key_builtin_available = true;
const uint64_t m1_kia_v5_key_builtin = 0xA1B2C3D4E5F60718ULL;
const bool m1_kia_v5_key_builtin_available = true;
