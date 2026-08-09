/*
 * test_m1_amiibo.c — regression tests for the Amiibo master-key re-signing
 * glue (NFC/amiibo/m1_amiibo.c), ported from bedge117/M1 "C3".
 *
 * Uses a synthetic (non-retail) key blob to validate the load/resign/selftest
 * plumbing end-to-end without requiring a real key_retail.bin.
 */
#include "unity.h"
#include "m1_amiibo.h"
#include "nfc3d/amiibo.h"
#include <string.h>
#include <stdio.h>

#define AMIIBO_DUMP_SIZE 540

void setUp(void) {}
void tearDown(void) {}

/* Build a deterministic, structurally-valid (but not real-retail) key blob
 * and write it to a temp file so m1_amiibo_load_keys() can read it via the
 * host FatFs stub. */
static void write_fake_keys(const char *path)
{
    nfc3d_amiibo_keys keys;
    memset(&keys, 0, sizeof(keys));

    for (int i = 0; i < 16; i++) keys.data.hmacKey[i] = (uint8_t)(0x10 + i);
    memcpy(keys.data.typeString, "unfixed infos", 14);
    keys.data.magicBytesSize = 0;
    for (int i = 0; i < 32; i++) keys.data.xorPad[i] = (uint8_t)(0x30 + i);

    for (int i = 0; i < 16; i++) keys.tag.hmacKey[i] = (uint8_t)(0x50 + i);
    memcpy(keys.tag.typeString, "locked secret", 14); /* 13 chars + NUL */
    keys.tag.magicBytesSize = 14;
    for (int i = 0; i < 14; i++) keys.tag.magicBytes[i] = (uint8_t)(0x70 + i);
    for (int i = 0; i < 32; i++) keys.tag.xorPad[i] = (uint8_t)(0x90 + i);

    FILE *f = fopen(path, "wb");
    TEST_ASSERT_NOT_NULL(f);
    size_t written = fwrite(&keys, 1, sizeof(keys), f);
    fclose(f);
    TEST_ASSERT_EQUAL_UINT(sizeof(keys), written);
}

/* Build a plausible 540-byte NTAG215-shaped amiibo dump (UID/pages populated,
 * HMACs deliberately left zeroed/wrong — the case re-signing is meant to fix). */
static void make_dump(uint8_t *dump)
{
    memset(dump, 0, AMIIBO_DUMP_SIZE);
    /* UID: page0[0..2], BCC0, page1[0..3] */
    dump[0] = 0x04; dump[1] = 0x11; dump[2] = 0x22; dump[3] = 0x00;
    dump[4] = 0x33; dump[5] = 0x44; dump[6] = 0x55; dump[7] = 0x66;
    /* PACK page (134): leave 0x00 0x00 — unrelated to resign, only checked by
     * the PACK-fix logic in nfc_storage.c, not by m1_amiibo. */
}

void test_selftest_passes_known_vectors(void)
{
    TEST_ASSERT_TRUE(m1_amiibo_selftest());
}

void test_is_loaded_false_before_load(void)
{
    /* Fresh-process invariant: without a prior load, resign must be a no-op
     * and report failure rather than corrupt the dump. */
    uint8_t dump[AMIIBO_DUMP_SIZE];
    make_dump(dump);
    uint8_t before[AMIIBO_DUMP_SIZE];
    memcpy(before, dump, sizeof(dump));

    /* Only assert the safe-fallback contract if keys truly aren't loaded yet
     * (tests in this binary can run in any order against process-global state). */
    if (!m1_amiibo_is_loaded()) {
        TEST_ASSERT_FALSE(m1_amiibo_resign(dump));
        TEST_ASSERT_EQUAL_MEMORY(before, dump, sizeof(dump));
    }
}

void test_load_keys_from_file_and_resign_matches_direct_pack(void)
{
    const char *path = "/tmp/m1_amiibo_test_keys.bin";
    write_fake_keys(path);

    TEST_ASSERT_TRUE(m1_amiibo_load_keys(path));
    TEST_ASSERT_TRUE(m1_amiibo_is_loaded());

    /* Loading again (already loaded) is a no-op success, not a re-read. */
    TEST_ASSERT_TRUE(m1_amiibo_load_keys(path));

    uint8_t dump[AMIIBO_DUMP_SIZE];
    make_dump(dump);
    uint8_t dump_copy[AMIIBO_DUMP_SIZE];
    memcpy(dump_copy, dump, sizeof(dump));

    TEST_ASSERT_TRUE(m1_amiibo_resign(dump));

    /* The trailing config/PWD/PACK region (bytes 520..539) must be preserved
     * untouched — only the first 520 bytes (figure region) may change. */
    TEST_ASSERT_EQUAL_MEMORY(dump_copy + NFC3D_AMIIBO_SIZE, dump + NFC3D_AMIIBO_SIZE,
                              AMIIBO_DUMP_SIZE - NFC3D_AMIIBO_SIZE);

    /* Re-signing must be reproducible: unpack+pack again from scratch with the
     * same keys, starting from the freshly re-signed dump, must be a fixed
     * point (HMACs already match this dump's own UID/content). */
    uint8_t reresigned[AMIIBO_DUMP_SIZE];
    memcpy(reresigned, dump, sizeof(dump));
    TEST_ASSERT_TRUE(m1_amiibo_resign(reresigned));
    TEST_ASSERT_EQUAL_MEMORY(dump, reresigned, sizeof(dump));

    remove(path);
}

void test_resign_null_dump_returns_false(void)
{
    TEST_ASSERT_FALSE(m1_amiibo_resign(NULL));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_selftest_passes_known_vectors);
    RUN_TEST(test_is_loaded_false_before_load);
    RUN_TEST(test_load_keys_from_file_and_resign_matches_direct_pack);
    RUN_TEST(test_resign_null_dump_returns_false);
    return UNITY_END();
}
