/* See COPYING.txt for license details. */

/**
 * test_nfc_card_info.c
 *
 * Unit tests for the pure-logic helpers in nfc_card_info.h / nfc_card_info.c:
 *   nfc_manufacturer_name() — UID byte 0 → ISO/IEC 7816-6 manufacturer string
 *   nfc_sak_type_str()      — SAK + ATQA → NFC-A card-type string
 *   nfc_uid_fmt()           — raw UID bytes → "AA BB CC …" hex string
 *   nfc_uid_step()          — UID big-endian increment / decrement
 *
 * Build with the host-side CMake:
 *   cmake -B build-tests -S tests && cmake --build build-tests
 *   ctest --test-dir build-tests --output-on-failure
 */

#include "unity.h"
#include "nfc_card_info.h"
#include <string.h>
#include <stdint.h>

void setUp(void)    {}
void tearDown(void) {}

/* =========================================================================
 * nfc_manufacturer_name
 * =========================================================================*/

void test_mfr_nxp(void)
{
    TEST_ASSERT_EQUAL_STRING("NXP", nfc_manufacturer_name(0x04));
}

void test_mfr_stmicro(void)
{
    TEST_ASSERT_EQUAL_STRING("STMicro", nfc_manufacturer_name(0x02));
}

void test_mfr_motorola(void)
{
    TEST_ASSERT_EQUAL_STRING("Motorola", nfc_manufacturer_name(0x01));
}

void test_mfr_ti(void)
{
    TEST_ASSERT_EQUAL_STRING("TI", nfc_manufacturer_name(0x07));
}

void test_mfr_samsung(void)
{
    TEST_ASSERT_EQUAL_STRING("Samsung", nfc_manufacturer_name(0x0E));
}

void test_mfr_lg(void)
{
    TEST_ASSERT_EQUAL_STRING("LG", nfc_manufacturer_name(0x10));
}

void test_mfr_em_micro(void)
{
    TEST_ASSERT_EQUAL_STRING("EM Micro", nfc_manufacturer_name(0x16));
}

void test_mfr_siliconcraft(void)
{
    TEST_ASSERT_EQUAL_STRING("SiliconCraft", nfc_manufacturer_name(0x28));
}

void test_mfr_unknown_zero(void)
{
    TEST_ASSERT_EQUAL_STRING("Unknown", nfc_manufacturer_name(0x00));
}

void test_mfr_unknown_ff(void)
{
    TEST_ASSERT_EQUAL_STRING("Unknown", nfc_manufacturer_name(0xFF));
}

void test_mfr_unknown_mid_range(void)
{
    /* 0x11 is not in the table */
    TEST_ASSERT_EQUAL_STRING("Unknown", nfc_manufacturer_name(0x11));
}

void test_mfr_returns_nonnull(void)
{
    /* All possible byte values must return a non-NULL string */
    for (int b = 0; b <= 0xFF; b++) {
        const char *s = nfc_manufacturer_name((uint8_t)b);
        TEST_ASSERT_NOT_NULL(s);
        TEST_ASSERT_TRUE(s[0] != '\0');
    }
}

/* =========================================================================
 * nfc_sak_type_str
 * =========================================================================*/

void test_sak_classic_1k(void)
{
    const uint8_t atqa[2] = {0x04, 0x00};
    TEST_ASSERT_EQUAL_STRING("Classic 1K", nfc_sak_type_str(0x08, atqa));
}

void test_sak_classic_4k(void)
{
    const uint8_t atqa[2] = {0x02, 0x00};
    TEST_ASSERT_EQUAL_STRING("Classic 4K", nfc_sak_type_str(0x18, atqa));
}

void test_sak_classic_mini(void)
{
    const uint8_t atqa[2] = {0x04, 0x00};
    TEST_ASSERT_EQUAL_STRING("Classic Mini", nfc_sak_type_str(0x09, atqa));
}

void test_sak_classic_2k(void)
{
    const uint8_t atqa[2] = {0x04, 0x00};
    TEST_ASSERT_EQUAL_STRING("Classic 2K", nfc_sak_type_str(0x10, atqa));
}

void test_sak_classic_4k_plus(void)
{
    const uint8_t atqa[2] = {0x04, 0x00};
    TEST_ASSERT_EQUAL_STRING("Classic 4K (Plus)", nfc_sak_type_str(0x11, atqa));
}

void test_sak_ultralight_ntag(void)
{
    /* SAK 0x00 + ATQA[0]==0x44 → Ultralight/NTAG */
    const uint8_t atqa[2] = {0x44, 0x00};
    TEST_ASSERT_EQUAL_STRING("Ultralight/NTAG", nfc_sak_type_str(0x00, atqa));
}

void test_sak_ultralight_wrong_atqa(void)
{
    /* SAK 0x00 but ATQA does NOT match UL — falls through to Other */
    const uint8_t atqa[2] = {0x04, 0x00};
    TEST_ASSERT_EQUAL_STRING("Other", nfc_sak_type_str(0x00, atqa));
}

void test_sak_desfire_iso_dep(void)
{
    const uint8_t atqa[2] = {0x04, 0x03};
    TEST_ASSERT_EQUAL_STRING("DESFire/ISO-DEP", nfc_sak_type_str(0x20, atqa));
}

void test_sak_classic_iso_dep(void)
{
    const uint8_t atqa[2] = {0x04, 0x00};
    TEST_ASSERT_EQUAL_STRING("Classic+ISO-DEP", nfc_sak_type_str(0x28, atqa));
}

void test_sak_classic_desfire(void)
{
    const uint8_t atqa[2] = {0x04, 0x00};
    TEST_ASSERT_EQUAL_STRING("Classic+DESFire", nfc_sak_type_str(0x60, atqa));
}

void test_sak_other(void)
{
    /* Unmapped SAK */
    const uint8_t atqa[2] = {0x00, 0x00};
    TEST_ASSERT_EQUAL_STRING("Other", nfc_sak_type_str(0xFF, atqa));
}

void test_sak_returns_nonnull(void)
{
    /* All 256 SAK values must return a non-NULL, non-empty string */
    const uint8_t atqa[2] = {0x44, 0x00};
    for (int s = 0; s <= 0xFF; s++) {
        const char *str = nfc_sak_type_str((uint8_t)s, atqa);
        TEST_ASSERT_NOT_NULL(str);
        TEST_ASSERT_TRUE(str[0] != '\0');
    }
}

/* =========================================================================
 * nfc_uid_fmt
 * =========================================================================*/

void test_uid_fmt_single_byte(void)
{
    const uint8_t uid[1] = {0xAB};
    char buf[32];
    nfc_uid_fmt(uid, 1, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("AB", buf);
}

void test_uid_fmt_four_bytes(void)
{
    const uint8_t uid[4] = {0x04, 0xB7, 0x28, 0x13};
    char buf[32];
    nfc_uid_fmt(uid, 4, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("04 B7 28 13", buf);
}

void test_uid_fmt_seven_bytes(void)
{
    const uint8_t uid[7] = {0x04, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    char buf[32];
    nfc_uid_fmt(uid, 7, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("04 01 02 03 04 05 06", buf);
}

void test_uid_fmt_ten_bytes(void)
{
    const uint8_t uid[10] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99};
    char buf[40];
    nfc_uid_fmt(uid, 10, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("00 11 22 33 44 55 66 77 88 99", buf);
}

void test_uid_fmt_zero_len(void)
{
    const uint8_t uid[4] = {0x01, 0x02, 0x03, 0x04};
    char buf[32];
    buf[0] = 'X';
    nfc_uid_fmt(uid, 0, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("", buf);
}

void test_uid_fmt_null_uid(void)
{
    char buf[32];
    buf[0] = 'X';
    nfc_uid_fmt(NULL, 4, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("", buf);
}

void test_uid_fmt_null_out_no_crash(void)
{
    const uint8_t uid[4] = {0x01, 0x02, 0x03, 0x04};
    /* Must not crash */
    nfc_uid_fmt(uid, 4, NULL, 0);
}

void test_uid_fmt_zero_out_size_no_crash(void)
{
    const uint8_t uid[4] = {0x01, 0x02, 0x03, 0x04};
    char buf[1];
    /* Must not crash; with size 1 there's no room for any digit */
    nfc_uid_fmt(uid, 4, buf, 0);
}

void test_uid_fmt_all_zeros(void)
{
    const uint8_t uid[4] = {0x00, 0x00, 0x00, 0x00};
    char buf[32];
    nfc_uid_fmt(uid, 4, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("00 00 00 00", buf);
}

void test_uid_fmt_all_ff(void)
{
    const uint8_t uid[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    char buf[32];
    nfc_uid_fmt(uid, 4, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("FF FF FF FF", buf);
}

/* =========================================================================
 * nfc_uid_step — increment
 * =========================================================================*/

void test_uid_step_increment_basic(void)
{
    uint8_t uid[4] = {0x00, 0x00, 0x00, 0x01};
    nfc_uid_step(uid, 4, +1);
    const uint8_t expected[4] = {0x00, 0x00, 0x00, 0x02};
    TEST_ASSERT_EQUAL_MEMORY(expected, uid, 4);
}

void test_uid_step_increment_carry(void)
{
    /* Last byte wraps → carry to next */
    uint8_t uid[4] = {0x00, 0x00, 0x00, 0xFF};
    nfc_uid_step(uid, 4, +1);
    const uint8_t expected[4] = {0x00, 0x00, 0x01, 0x00};
    TEST_ASSERT_EQUAL_MEMORY(expected, uid, 4);
}

void test_uid_step_increment_multi_carry(void)
{
    uint8_t uid[4] = {0x00, 0x00, 0xFF, 0xFF};
    nfc_uid_step(uid, 4, +1);
    const uint8_t expected[4] = {0x00, 0x01, 0x00, 0x00};
    TEST_ASSERT_EQUAL_MEMORY(expected, uid, 4);
}

void test_uid_step_increment_full_wrap(void)
{
    /* All-FF wraps to all-00 */
    uint8_t uid[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    nfc_uid_step(uid, 4, +1);
    const uint8_t expected[4] = {0x00, 0x00, 0x00, 0x00};
    TEST_ASSERT_EQUAL_MEMORY(expected, uid, 4);
}

/* =========================================================================
 * nfc_uid_step — decrement
 * =========================================================================*/

void test_uid_step_decrement_basic(void)
{
    uint8_t uid[4] = {0x00, 0x00, 0x00, 0x02};
    nfc_uid_step(uid, 4, -1);
    const uint8_t expected[4] = {0x00, 0x00, 0x00, 0x01};
    TEST_ASSERT_EQUAL_MEMORY(expected, uid, 4);
}

void test_uid_step_decrement_borrow(void)
{
    /* Last byte is 0 → borrow from previous */
    uint8_t uid[4] = {0x00, 0x00, 0x01, 0x00};
    nfc_uid_step(uid, 4, -1);
    const uint8_t expected[4] = {0x00, 0x00, 0x00, 0xFF};
    TEST_ASSERT_EQUAL_MEMORY(expected, uid, 4);
}

void test_uid_step_decrement_multi_borrow(void)
{
    uint8_t uid[4] = {0x00, 0x01, 0x00, 0x00};
    nfc_uid_step(uid, 4, -1);
    const uint8_t expected[4] = {0x00, 0x00, 0xFF, 0xFF};
    TEST_ASSERT_EQUAL_MEMORY(expected, uid, 4);
}

void test_uid_step_decrement_full_wrap(void)
{
    /* All-00 wraps to all-FF */
    uint8_t uid[4] = {0x00, 0x00, 0x00, 0x00};
    nfc_uid_step(uid, 4, -1);
    const uint8_t expected[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    TEST_ASSERT_EQUAL_MEMORY(expected, uid, 4);
}

/* =========================================================================
 * nfc_uid_step — null / zero guards
 * =========================================================================*/

void test_uid_step_null_uid_no_crash(void)
{
    nfc_uid_step(NULL, 4, +1);
}

void test_uid_step_zero_len_no_change(void)
{
    uint8_t uid[4] = {0x01, 0x02, 0x03, 0x04};
    nfc_uid_step(uid, 0, +1);
    const uint8_t expected[4] = {0x01, 0x02, 0x03, 0x04};
    TEST_ASSERT_EQUAL_MEMORY(expected, uid, 4);
}

void test_uid_step_zero_dir_no_change(void)
{
    uint8_t uid[4] = {0xAA, 0xBB, 0xCC, 0xDD};
    nfc_uid_step(uid, 4, 0);
    const uint8_t expected[4] = {0xAA, 0xBB, 0xCC, 0xDD};
    TEST_ASSERT_EQUAL_MEMORY(expected, uid, 4);
}

void test_uid_step_single_byte_increment(void)
{
    uint8_t uid[1] = {0x0A};
    nfc_uid_step(uid, 1, +1);
    TEST_ASSERT_EQUAL_UINT8(0x0B, uid[0]);
}

void test_uid_step_single_byte_wrap(void)
{
    uint8_t uid[1] = {0xFF};
    nfc_uid_step(uid, 1, +1);
    TEST_ASSERT_EQUAL_UINT8(0x00, uid[0]);
}

/* =========================================================================
 * main
 * =========================================================================*/

int main(void)
{
    UNITY_BEGIN();

    /* nfc_manufacturer_name */
    RUN_TEST(test_mfr_nxp);
    RUN_TEST(test_mfr_stmicro);
    RUN_TEST(test_mfr_motorola);
    RUN_TEST(test_mfr_ti);
    RUN_TEST(test_mfr_samsung);
    RUN_TEST(test_mfr_lg);
    RUN_TEST(test_mfr_em_micro);
    RUN_TEST(test_mfr_siliconcraft);
    RUN_TEST(test_mfr_unknown_zero);
    RUN_TEST(test_mfr_unknown_ff);
    RUN_TEST(test_mfr_unknown_mid_range);
    RUN_TEST(test_mfr_returns_nonnull);

    /* nfc_sak_type_str */
    RUN_TEST(test_sak_classic_1k);
    RUN_TEST(test_sak_classic_4k);
    RUN_TEST(test_sak_classic_mini);
    RUN_TEST(test_sak_classic_2k);
    RUN_TEST(test_sak_classic_4k_plus);
    RUN_TEST(test_sak_ultralight_ntag);
    RUN_TEST(test_sak_ultralight_wrong_atqa);
    RUN_TEST(test_sak_desfire_iso_dep);
    RUN_TEST(test_sak_classic_iso_dep);
    RUN_TEST(test_sak_classic_desfire);
    RUN_TEST(test_sak_other);
    RUN_TEST(test_sak_returns_nonnull);

    /* nfc_uid_fmt */
    RUN_TEST(test_uid_fmt_single_byte);
    RUN_TEST(test_uid_fmt_four_bytes);
    RUN_TEST(test_uid_fmt_seven_bytes);
    RUN_TEST(test_uid_fmt_ten_bytes);
    RUN_TEST(test_uid_fmt_zero_len);
    RUN_TEST(test_uid_fmt_null_uid);
    RUN_TEST(test_uid_fmt_null_out_no_crash);
    RUN_TEST(test_uid_fmt_zero_out_size_no_crash);
    RUN_TEST(test_uid_fmt_all_zeros);
    RUN_TEST(test_uid_fmt_all_ff);

    /* nfc_uid_step — increment */
    RUN_TEST(test_uid_step_increment_basic);
    RUN_TEST(test_uid_step_increment_carry);
    RUN_TEST(test_uid_step_increment_multi_carry);
    RUN_TEST(test_uid_step_increment_full_wrap);

    /* nfc_uid_step — decrement */
    RUN_TEST(test_uid_step_decrement_basic);
    RUN_TEST(test_uid_step_decrement_borrow);
    RUN_TEST(test_uid_step_decrement_multi_borrow);
    RUN_TEST(test_uid_step_decrement_full_wrap);

    /* nfc_uid_step — guards */
    RUN_TEST(test_uid_step_null_uid_no_crash);
    RUN_TEST(test_uid_step_zero_len_no_change);
    RUN_TEST(test_uid_step_zero_dir_no_change);
    RUN_TEST(test_uid_step_single_byte_increment);
    RUN_TEST(test_uid_step_single_byte_wrap);

    return UNITY_END();
}
