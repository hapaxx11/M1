/*
 * test_mfkey32.c — Host-side unit test for the on-device MFKey32 key recovery.
 *
 * Validates mfkey32v2_recover() and mfkey32_selftest() against the published
 * known-answer vector:
 *   uid  = 0x2a234f80
 *   nt0  = 0x240bd022  {nr0} = 0xad2e1687  {ar0} = 0x57e6f7e4
 *   nt1  = 0x18a4bd3e  {nr1} = 0xaccc1a23  {ar1} = 0x6f10e401
 *   key  = 0xa0a1a2a3a4a5
 *
 * The mfkey32.c implementation is self-contained (no HAL / RTOS / FreeRTOS
 * headers); only <stdint.h>, <stdbool.h>, and <string.h> are needed.
 */

#include "unity.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* mfkey32.h / mfkey32.c live in NFC/NFC_drv/legacy/. The CMake target
 * adds that directory to INCLUDE_DIRECTORIES. */
#include "mfkey32.h"

void setUp(void) { }
void tearDown(void) { }

/* ---- selftest passthrough ---- */
void test_mfkey32_selftest(void)
{
    TEST_ASSERT_TRUE(mfkey32_selftest());
}

/* ---- known-answer vector ---- */
void test_mfkey32_known_answer(void)
{
    uint64_t key = 0;
    bool ok = mfkey32v2_recover(
        0x2a234f80u,
        0x240bd022u, 0xad2e1687u, 0x57e6f7e4u,
        0x18a4bd3eu, 0xaccc1a23u, 0x6f10e401u,
        &key, NULL, NULL);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_UINT64(0xa0a1a2a3a4a5ULL, key);
}

/* ---- key bytes MSB-first ---- */
void test_mfkey32_key_bytes_msb_first(void)
{
    uint64_t key = 0;
    bool ok = mfkey32v2_recover(
        0x2a234f80u,
        0x240bd022u, 0xad2e1687u, 0x57e6f7e4u,
        0x18a4bd3eu, 0xaccc1a23u, 0x6f10e401u,
        &key, NULL, NULL);
    TEST_ASSERT_TRUE(ok);
    /* key bytes MSB-first should be a0 a1 a2 a3 a4 a5 */
    TEST_ASSERT_EQUAL_HEX8(0xa0, (uint8_t)(key >> 40));
    TEST_ASSERT_EQUAL_HEX8(0xa1, (uint8_t)(key >> 32));
    TEST_ASSERT_EQUAL_HEX8(0xa2, (uint8_t)(key >> 24));
    TEST_ASSERT_EQUAL_HEX8(0xa3, (uint8_t)(key >> 16));
    TEST_ASSERT_EQUAL_HEX8(0xa4, (uint8_t)(key >> 8));
    TEST_ASSERT_EQUAL_HEX8(0xa5, (uint8_t)(key));
}

/* ---- NULL key_out is safe (don't crash) ---- */
void test_mfkey32_null_key_out_safe(void)
{
    bool ok = mfkey32v2_recover(
        0x2a234f80u,
        0x240bd022u, 0xad2e1687u, 0x57e6f7e4u,
        0x18a4bd3eu, 0xaccc1a23u, 0x6f10e401u,
        NULL, NULL, NULL);
    TEST_ASSERT_TRUE(ok);
}

/* ---- wrong ar0 yields no key ---- */
void test_mfkey32_wrong_ar_no_key(void)
{
    uint64_t key = 0;
    bool ok = mfkey32v2_recover(
        0x2a234f80u,
        0x240bd022u, 0xad2e1687u, 0xDEADBEEFu, /* bad ar0 */
        0x18a4bd3eu, 0xaccc1a23u, 0x6f10e401u,
        &key, NULL, NULL);
    TEST_ASSERT_FALSE(ok);
    TEST_ASSERT_EQUAL_UINT64(0, key);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_mfkey32_selftest);
    RUN_TEST(test_mfkey32_known_answer);
    RUN_TEST(test_mfkey32_key_bytes_msb_first);
    RUN_TEST(test_mfkey32_null_key_out_safe);
    RUN_TEST(test_mfkey32_wrong_ar_no_key);
    return UNITY_END();
}
