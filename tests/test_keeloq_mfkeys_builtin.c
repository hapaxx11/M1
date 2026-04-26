/* See COPYING.txt for license details. */

/*
 * test_keeloq_mfkeys_builtin.c
 *
 * Host-side unit tests for the build-time KeeLoq key embedding mechanism
 * (Flipper-compatible firmware-embedded manufacturer keys).
 *
 * This test links with tests/stubs/keeloq_builtin_test_data.c, which
 * provides two test entries ("TestVaultMfr" and "AnotherMfr") via the
 * keeloq_mfkeys_builtin_text / keeloq_mfkeys_builtin_len symbols.
 *
 * The SD card paths are redirected to /tmp/ (via -D compile flags) so the
 * tests can verify that the SD card is NOT consulted when builtin data is
 * present — those paths should remain absent from the filesystem.
 *
 * Tests:
 *   test_load_uses_builtin_data
 *       keeloq_mfkeys_load() returns true and finds both builtin entries.
 *
 *   test_builtin_skips_sd_card
 *       With no SD card files present, load() still succeeds (builtin used).
 *
 *   test_builtin_find_entry
 *       keeloq_mfkeys_find() locates a builtin entry by name.
 *
 *   test_builtin_find_case_insensitive
 *       keeloq_mfkeys_find() is case-insensitive.
 *
 *   test_builtin_sd_files_not_created
 *       After loading from builtin, no SD card files are created/modified.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "unity.h"
#include "subghz_keeloq_mfkeys.h"

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

static void cleanup_sd_files(void)
{
    remove(KEELOQ_MFKEYS_ENC_PATH);
    remove(KEELOQ_MFKEYS_PATH);
}

static bool sd_file_exists(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    fclose(f);
    return true;
}

void setUp(void)
{
    keeloq_mfkeys_free();
    cleanup_sd_files();
}

void tearDown(void)
{
    keeloq_mfkeys_free();
    cleanup_sd_files();
}

/* ------------------------------------------------------------------ */

/* test_load_uses_builtin_data
 * keeloq_mfkeys_load() returns true and the two builtin entries are present.
 */
void test_load_uses_builtin_data(void)
{
    /* Confirm the compiled-in stub has data */
    TEST_ASSERT_GREATER_THAN_UINT32(0, keeloq_mfkeys_builtin_len);
    TEST_ASSERT_NOT_NULL(keeloq_mfkeys_builtin_text);

    TEST_ASSERT_TRUE(keeloq_mfkeys_load());

    /* The populated test stub provides exactly 2 entries */
    TEST_ASSERT_EQUAL_UINT32(2, keeloq_mfkeys_count());
}

/* test_builtin_skips_sd_card
 * With no SD card files present at all, load() still succeeds because
 * the builtin data is used — no SD access is attempted.
 */
void test_builtin_skips_sd_card(void)
{
    /* Verify SD files are absent */
    TEST_ASSERT_FALSE(sd_file_exists(KEELOQ_MFKEYS_ENC_PATH));
    TEST_ASSERT_FALSE(sd_file_exists(KEELOQ_MFKEYS_PATH));

    TEST_ASSERT_TRUE(keeloq_mfkeys_load());
    TEST_ASSERT_EQUAL_UINT32(2, keeloq_mfkeys_count());
}

/* test_builtin_find_entry
 * After loading, keeloq_mfkeys_find() locates a builtin entry and the
 * key value and learn type are correct.
 */
void test_builtin_find_entry(void)
{
    TEST_ASSERT_TRUE(keeloq_mfkeys_load());

    KeeLoqMfrEntry e;
    TEST_ASSERT_TRUE(keeloq_mfkeys_find("TestVaultMfr", &e));
    TEST_ASSERT_EQUAL_UINT64(0xAABBCCDDEEFF0011ULL, e.key);
    TEST_ASSERT_EQUAL_INT(KEELOQ_LEARN_NORMAL, e.learn_type);

    KeeLoqMfrEntry e2;
    TEST_ASSERT_TRUE(keeloq_mfkeys_find("AnotherMfr", &e2));
    TEST_ASSERT_EQUAL_UINT64(0x1234567890ABCDEFULL, e2.key);
    TEST_ASSERT_EQUAL_INT(KEELOQ_LEARN_SIMPLE, e2.learn_type);
}

/* test_builtin_find_case_insensitive
 * keeloq_mfkeys_find() must match regardless of case.
 */
void test_builtin_find_case_insensitive(void)
{
    TEST_ASSERT_TRUE(keeloq_mfkeys_load());

    KeeLoqMfrEntry e;
    TEST_ASSERT_TRUE(keeloq_mfkeys_find("testvaultmfr", &e));
    TEST_ASSERT_EQUAL_UINT64(0xAABBCCDDEEFF0011ULL, e.key);

    TEST_ASSERT_TRUE(keeloq_mfkeys_find("ANOTHERMFR", &e));
    TEST_ASSERT_EQUAL_UINT64(0x1234567890ABCDEFULL, e.key);
}

/* test_builtin_sd_files_not_created
 * Loading from builtin must not create any SD card keystore files — the
 * auto-migration path (plaintext → encrypted) must not run.
 */
void test_builtin_sd_files_not_created(void)
{
    TEST_ASSERT_TRUE(keeloq_mfkeys_load());

    /* Neither SD path should have been created */
    TEST_ASSERT_FALSE(sd_file_exists(KEELOQ_MFKEYS_ENC_PATH));
    TEST_ASSERT_FALSE(sd_file_exists(KEELOQ_MFKEYS_PATH));
}

/* ------------------------------------------------------------------ */

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_load_uses_builtin_data);
    RUN_TEST(test_builtin_skips_sd_card);
    RUN_TEST(test_builtin_find_entry);
    RUN_TEST(test_builtin_find_case_insensitive);
    RUN_TEST(test_builtin_sd_files_not_created);
    return UNITY_END();
}
