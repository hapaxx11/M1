/* See COPYING.txt for license details. */

/*
 * test_keeloq_mfkeys_enc.c
 *
 * Host-side unit tests for the encrypted KeeLoq keystore:
 *   keeloq_mfkeys_save_encrypted()
 *   keeloq_mfkeys_load_encrypted()
 *   keeloq_mfkeys_load()  (auto-migration from plaintext)
 *
 * File paths are overridden via compile-time -D flags in CMakeLists.txt
 * (KEELOQ_MFKEYS_PATH and KEELOQ_MFKEYS_ENC_PATH) so that tests use
 * /tmp/ rather than the production "0:/SUBGHZ/" paths.
 *
 * Tests:
 *   test_save_encrypted_empty_table_fails   — save when nothing loaded → false
 *   test_save_load_encrypted_roundtrip      — full encrypt/decrypt cycle
 *   test_load_encrypted_bad_magic_fails     — wrong magic → false
 *   test_load_encrypted_truncated_payload_fails — too few bytes → false
 *   test_auto_migration_from_plaintext      — load() migrates plaintext → enc
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "unity.h"
#include "subghz_keeloq_mfkeys.h"

/* ------------------------------------------------------------------ */
/* Helpers to remove the temp files                                    */
/* ------------------------------------------------------------------ */

static void cleanup_files(void)
{
    remove(KEELOQ_MFKEYS_ENC_PATH);
    remove(KEELOQ_MFKEYS_PATH);
}

void setUp(void)
{
    keeloq_mfkeys_free();
    cleanup_files();
}

void tearDown(void)
{
    keeloq_mfkeys_free();
    cleanup_files();
}

/* ------------------------------------------------------------------ */

/* test_save_encrypted_empty_table_fails
 * With no table loaded, keeloq_mfkeys_save_encrypted() must return false.
 */
void test_save_encrypted_empty_table_fails(void)
{
    TEST_ASSERT_EQUAL_UINT32(0, keeloq_mfkeys_count());
    TEST_ASSERT_FALSE(keeloq_mfkeys_save_encrypted());
}

/* test_save_load_encrypted_roundtrip
 * Load two entries via load_text(), save_encrypted(), free, load_encrypted().
 * The loaded entries must match the originals exactly.
 */
void test_save_load_encrypted_roundtrip(void)
{
    const char *text =
        "0102030405060708:2:AcmeCorp\n"
        "AABBCCDDEEFF0011:1:BetaBrand\n";

    TEST_ASSERT_TRUE(keeloq_mfkeys_load_text(text));
    TEST_ASSERT_EQUAL_UINT32(2, keeloq_mfkeys_count());

    /* Save to encrypted file */
    TEST_ASSERT_TRUE(keeloq_mfkeys_save_encrypted());

    /* Release the in-memory table */
    keeloq_mfkeys_free();
    TEST_ASSERT_EQUAL_UINT32(0, keeloq_mfkeys_count());

    /* Reload from the encrypted file */
    TEST_ASSERT_TRUE(keeloq_mfkeys_load_encrypted());
    TEST_ASSERT_EQUAL_UINT32(2, keeloq_mfkeys_count());

    /* Verify each entry */
    KeeLoqMfrEntry e;

    TEST_ASSERT_TRUE(keeloq_mfkeys_find("AcmeCorp", &e));
    TEST_ASSERT_EQUAL_UINT64(0x0102030405060708ULL, e.key);
    TEST_ASSERT_EQUAL_INT(KEELOQ_LEARN_NORMAL, e.learn_type);

    TEST_ASSERT_TRUE(keeloq_mfkeys_find("BetaBrand", &e));
    TEST_ASSERT_EQUAL_UINT64(0xAABBCCDDEEFF0011ULL, e.key);
    TEST_ASSERT_EQUAL_INT(KEELOQ_LEARN_SIMPLE, e.learn_type);
}

/* test_load_encrypted_bad_magic_fails
 * A file with a wrong magic word must be rejected.
 */
void test_load_encrypted_bad_magic_fails(void)
{
    /* Build a file: bad magic + valid-looking header + 32 garbage bytes */
    FILE *f = fopen(KEELOQ_MFKEYS_ENC_PATH, "wb");
    TEST_ASSERT_NOT_NULL(f);

    /* Header: wrong magic (all zeros), version 1, payload_len = 32 */
    uint8_t header[9] = {
        0x00, 0x00, 0x00, 0x00,  /* wrong magic */
        0x01,                     /* version */
        0x20, 0x00, 0x00, 0x00   /* payload_len = 32 LE */
    };
    fwrite(header, 1, sizeof(header), f);

    /* 32 bytes of filler payload */
    uint8_t payload[32];
    memset(payload, 0xAA, sizeof(payload));
    fwrite(payload, 1, sizeof(payload), f);
    fclose(f);

    TEST_ASSERT_FALSE(keeloq_mfkeys_load_encrypted());
    TEST_ASSERT_EQUAL_UINT32(0, keeloq_mfkeys_count());
}

/* test_load_encrypted_truncated_payload_fails
 * A file that claims a large payload_len but provides only a few bytes
 * must be rejected (f_read returns fewer bytes than expected).
 */
void test_load_encrypted_truncated_payload_fails(void)
{
    FILE *f = fopen(KEELOQ_MFKEYS_ENC_PATH, "wb");
    TEST_ASSERT_NOT_NULL(f);

    /* Valid magic and version, payload_len = 1000 (0x03E8 LE) */
    uint8_t header[9] = {
        0x4D, 0x31, 0x4B, 0x4C,  /* "M1KL" */
        0x01,                     /* version */
        0xE8, 0x03, 0x00, 0x00   /* payload_len = 1000 LE */
    };
    fwrite(header, 1, sizeof(header), f);

    /* Provide only 4 bytes of the 1000-byte payload */
    fwrite("ABCD", 1, 4, f);
    fclose(f);

    TEST_ASSERT_FALSE(keeloq_mfkeys_load_encrypted());
}

/* test_auto_migration_from_plaintext
 * When only a plaintext keeloq_mfcodes file exists, keeloq_mfkeys_load():
 *   1. Loads the plaintext entries.
 *   2. Creates the encrypted .enc file.
 *   3. Deletes the plaintext file.
 *   4. A subsequent keeloq_mfkeys_load() succeeds from the .enc file.
 */
void test_auto_migration_from_plaintext(void)
{
    /* Write a plaintext keystore file */
    FILE *f = fopen(KEELOQ_MFKEYS_PATH, "w");
    TEST_ASSERT_NOT_NULL(f);
    fputs("0102030405060708:2:AcmeCorp\n", f);
    fputs("AABBCCDDEEFF0011:1:BetaBrand\n", f);
    fclose(f);

    /* No encrypted file should exist yet */
    FILE *enc = fopen(KEELOQ_MFKEYS_ENC_PATH, "rb");
    TEST_ASSERT_NULL(enc);

    /* keeloq_mfkeys_load() must load from plaintext and auto-migrate */
    TEST_ASSERT_TRUE(keeloq_mfkeys_load());
    TEST_ASSERT_EQUAL_UINT32(2, keeloq_mfkeys_count());

    /* The encrypted file must now exist */
    enc = fopen(KEELOQ_MFKEYS_ENC_PATH, "rb");
    TEST_ASSERT_NOT_NULL(enc);
    fclose(enc);

    /* The plaintext file must have been deleted */
    FILE *plain = fopen(KEELOQ_MFKEYS_PATH, "rb");
    TEST_ASSERT_NULL(plain);

    /* A second load must succeed (now from the .enc file) */
    keeloq_mfkeys_free();
    TEST_ASSERT_TRUE(keeloq_mfkeys_load());
    TEST_ASSERT_EQUAL_UINT32(2, keeloq_mfkeys_count());

    KeeLoqMfrEntry e;
    TEST_ASSERT_TRUE(keeloq_mfkeys_find("AcmeCorp", &e));
    TEST_ASSERT_EQUAL_UINT64(0x0102030405060708ULL, e.key);
    TEST_ASSERT_TRUE(keeloq_mfkeys_find("BetaBrand", &e));
    TEST_ASSERT_EQUAL_UINT64(0xAABBCCDDEEFF0011ULL, e.key);
}

/* ------------------------------------------------------------------ */

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_save_encrypted_empty_table_fails);
    RUN_TEST(test_save_load_encrypted_roundtrip);
    RUN_TEST(test_load_encrypted_bad_magic_fails);
    RUN_TEST(test_load_encrypted_truncated_payload_fails);
    RUN_TEST(test_auto_migration_from_plaintext);
    return UNITY_END();
}
