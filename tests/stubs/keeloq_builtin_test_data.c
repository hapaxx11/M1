/* See COPYING.txt for license details. */

/**
 * @file  keeloq_builtin_test_data.c
 * @brief Populated builtin stub for test_keeloq_mfkeys_builtin unit tests.
 *
 * Provides two test manufacturer entries so that test_keeloq_mfkeys_builtin.c
 * can verify that keeloq_mfkeys_load() uses the compiled-in data and does
 * NOT fall back to the SD card paths.
 *
 * This file is ONLY linked into the test_keeloq_mfkeys_builtin executable.
 * Production firmware links Sub_Ghz/subghz_keeloq_mfkeys_builtin.c instead.
 */

#include "subghz_keeloq_mfkeys.h"

static const char s_test_builtin_text[] =
    "AABBCCDDEEFF0011:2:TestVaultMfr\n"
    "1234567890ABCDEF:1:AnotherMfr\n";

const char * const keeloq_mfkeys_builtin_text = s_test_builtin_text;
const uint32_t     keeloq_mfkeys_builtin_len  =
    (uint32_t)(sizeof(s_test_builtin_text) - 1u);
