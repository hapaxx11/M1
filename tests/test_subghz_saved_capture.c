/* See COPYING.txt for license details. */

#include "unity.h"
#include "subghz_saved_capture.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define TEST_DIR "/tmp/m1_saved_capture_test"
#define TEST_CAPTURE_PATH TEST_DIR "/capture.sgh"
#define TEST_UPDATE_TMP TEST_CAPTURE_PATH ".update.tmp"
#define TEST_UPDATE_BAK TEST_CAPTURE_PATH ".update.bak"

static void write_tmp(const char *path, const char *text)
{
    FILE *file = fopen(path, "wb");
    TEST_ASSERT_NOT_NULL(file);
    TEST_ASSERT_EQUAL(strlen(text), fwrite(text, 1, strlen(text), file));
    fclose(file);
}

static void remove_test_files(void)
{
    unlink(TEST_DIR "/nested/capture.sub");
    rmdir(TEST_DIR "/nested");
    unlink(TEST_DIR "/capture.sgh");
    unlink(TEST_DIR "/other.sub");
    unlink(TEST_DIR "/notes.txt");
    unlink(TEST_DIR "/format.sub");
    unlink(TEST_DIR "/format.sgh");
    unlink(TEST_DIR "/capture.sub");
    unlink(TEST_UPDATE_TMP);
    unlink(TEST_UPDATE_BAK);
}

void setUp(void)
{
    remove_test_files();
    mkdir(TEST_DIR, 0755);
}

void tearDown(void)
{
    remove_test_files();
    rmdir(TEST_DIR);
}

static subghz_saved_match_identity_t identity(const char *protocol,
                                               uint32_t serial,
                                               uint32_t counter)
{
    subghz_saved_match_identity_t value;
    memset(&value, 0, sizeof(value));
    snprintf(value.protocol, sizeof(value.protocol), "%s", protocol);
    value.serial = serial;
    value.counter = counter;
    value.has_serial = true;
    value.has_counter = true;
    value.allow_serial_match = true;
    return value;
}

void test_saved_match_serial_and_counter_margin(void)
{
    const subghz_saved_match_identity_t rx = identity("PSA", 0x12345678, 150);
    subghz_saved_match_identity_t saved = identity("PSA", 0x12345678, 100);
    TEST_ASSERT_TRUE(subghz_saved_match_identity(&rx, &saved));

    saved.counter = 99;
    TEST_ASSERT_FALSE(subghz_saved_match_identity(&rx, &saved));

    saved.counter = 200;
    TEST_ASSERT_TRUE(subghz_saved_match_identity(&rx, &saved));

    saved.counter = 201;
    TEST_ASSERT_FALSE(subghz_saved_match_identity(&rx, &saved));

    saved = identity("Ford V3", 0x12345678, 100);
    TEST_ASSERT_FALSE(subghz_saved_match_identity(&rx, &saved));

    saved = identity("PSA", 0x12345679, 100);
    TEST_ASSERT_FALSE(subghz_saved_match_identity(&rx, &saved));
}

void test_saved_match_static_protocol_requires_full_key(void)
{
    subghz_saved_match_identity_t rx = identity("Hormann HSM", 0x1234, 0);
    subghz_saved_match_identity_t saved = identity("Hormann HSM", 0x1234, 0);
    rx.allow_serial_match = false;
    saved.key = 0x0102030405060708ULL;
    rx.key = 0x0102030405060709ULL;
    rx.has_key = true;
    saved.has_key = true;

    TEST_ASSERT_FALSE(subghz_saved_match_identity(&rx, &saved));
    rx.key = saved.key;
    TEST_ASSERT_TRUE(subghz_saved_match_identity(&rx, &saved));
}

void test_saved_match_key_fallback_and_missing_counter(void)
{
    subghz_saved_match_identity_t rx = identity("Fiat V2", 0, 0);
    subghz_saved_match_identity_t saved = identity("Fiat V2", 0, 0);
    rx.has_serial = false;
    rx.has_counter = false;
    rx.has_key = true;
    rx.key = 0x123456789ABCDEF0ULL;
    saved.has_serial = false;
    saved.has_counter = false;
    saved.has_key = true;
    saved.key = rx.key;

    TEST_ASSERT_TRUE(subghz_saved_match_identity(&rx, &saved));
    saved.key ^= 1u;
    TEST_ASSERT_FALSE(subghz_saved_match_identity(&rx, &saved));

    saved.key = rx.key;
    saved.has_counter = true;
    saved.counter = UINT32_MAX;
    TEST_ASSERT_TRUE(subghz_saved_match_identity(&rx, &saved));
}

void test_saved_match_serial_received_key_only_saved_falls_back_to_key(void)
{
    subghz_saved_match_identity_t rx = identity("PSA", 0x12345678, 100);
    subghz_saved_match_identity_t saved;
    memset(&saved, 0, sizeof(saved));
    snprintf(saved.protocol, sizeof(saved.protocol), "%s", "PSA");
    saved.key = 0x123A445566778899ULL;
    saved.has_key = true;

    rx.allow_serial_match = true;
    rx.has_key = true;
    rx.key = saved.key;
    TEST_ASSERT_TRUE(subghz_saved_match_identity(&rx, &saved));

    saved.key ^= 1u;
    TEST_ASSERT_FALSE(subghz_saved_match_identity(&rx, &saved));
}

void test_saved_capture_read_flipper_and_native_identities(void)
{
    write_tmp(TEST_DIR "/other.sub",
              "Filetype: Flipper SubGhz Key File\r\n"
              "Version: 1\r\n"
              "Protocol: PSA\r\n"
              "Bit: 80\r\n"
              "Key: 12 3A 44 55 66 77 88 99\r\n"
              "Serial: 305419896\r\n"
              "Cnt: 100\r\n");

    subghz_saved_match_identity_t parsed;
    TEST_ASSERT_TRUE(subghz_saved_capture_read_identity(TEST_DIR "/other.sub", &parsed));
    TEST_ASSERT_EQUAL_STRING("PSA", parsed.protocol);
    TEST_ASSERT_EQUAL_UINT64(0x123A445566778899ULL, parsed.key);
    TEST_ASSERT_EQUAL_UINT32(0x12345678, parsed.serial);
    TEST_ASSERT_EQUAL_UINT32(100, parsed.counter);
    TEST_ASSERT_TRUE(parsed.has_key);
    TEST_ASSERT_TRUE(parsed.has_serial);
    TEST_ASSERT_TRUE(parsed.has_counter);

    write_tmp(TEST_DIR "/capture.sgh",
              "Filetype: M1 SubGHz PACKET\r\n"
              "Version: 0.9\r\n"
              "Protocol: Renault V1\r\n"
              "Payload: 0x0102030405060708\r\n"
              "Serial: 4275878552\r\n"
              "Cnt: 42\r\n");
    TEST_ASSERT_TRUE(subghz_saved_capture_read_identity(TEST_DIR "/capture.sgh", &parsed));
    TEST_ASSERT_EQUAL_STRING("Renault V1", parsed.protocol);
    TEST_ASSERT_EQUAL_UINT64(0x0102030405060708ULL, parsed.key);
    TEST_ASSERT_EQUAL_UINT32(0xFEDCBA98, parsed.serial);
    TEST_ASSERT_EQUAL_UINT32(42, parsed.counter);
}

void test_saved_capture_find_match_scans_sub_and_sgh(void)
{
    write_tmp(TEST_DIR "/capture.sgh",
              "Filetype: M1 SubGHz PACKET\r\n"
              "Version: 0.9\r\n"
              "Protocol: PSA\r\n"
              "Payload: 0x123A445566778899\r\n"
              "Serial: 305419896\r\n"
              "Cnt: 100\r\n");
    write_tmp(TEST_DIR "/other.sub",
              "Filetype: Flipper SubGhz Key File\r\n"
              "Version: 1\r\n"
              "Protocol: Ford V3\r\n"
              "Key: 00 00 00 00 00 00 00 01\r\n"
              "Serial: 1\r\n"
              "Cnt: 100\r\n");
    write_tmp(TEST_DIR "/notes.txt", "not a capture\r\n");

    SubGHz_Dec_Info_t received;
    memset(&received, 0, sizeof(received));
    received.key = 0x0102030405060708ULL;
    received.serial_number = 0x12345678;
    received.rolling_code = 140;

    char path[128];
    TEST_ASSERT_TRUE(subghz_saved_capture_find_match(
        TEST_DIR, &received, "PSA", true, path, sizeof(path)));
    TEST_ASSERT_EQUAL_STRING(TEST_DIR "/capture.sgh", path);

    received.rolling_code = 151;
    TEST_ASSERT_FALSE(subghz_saved_capture_find_match(
        TEST_DIR, &received, "PSA", true, path, sizeof(path)));
    TEST_ASSERT_EQUAL_UINT8(0, path[0]);
}

void test_saved_capture_find_match_searches_nested_directories(void)
{
    TEST_ASSERT_EQUAL_INT(0, mkdir(TEST_DIR "/nested", 0755));
    write_tmp(TEST_DIR "/nested/capture.sub",
              "Filetype: Flipper SubGhz Key File\r\n"
              "Version: 1\r\n"
              "Protocol: PSA\r\n"
              "Key: 12 3A 44 55 66 77 88 99\r\n"
              "Serial: 305419896\r\n"
              "Cnt: 100\r\n");

    SubGHz_Dec_Info_t received;
    memset(&received, 0, sizeof(received));
    received.key = 0x123A445566778899ULL;
    received.serial_number = 0x12345678;
    received.rolling_code = 100;

    char path[128];
    TEST_ASSERT_TRUE(subghz_saved_capture_find_match(
        TEST_DIR, &received, "PSA", true, path, sizeof(path)));
    TEST_ASSERT_EQUAL_STRING(TEST_DIR "/nested/capture.sub", path);
}

void test_saved_capture_does_not_offer_update_for_recovered_psa(void)
{
    write_tmp(TEST_DIR "/capture.sub",
              "Filetype: Flipper SubGhz Key File\r\n"
              "Version: 1\r\n"
              "Protocol: PSA\r\n"
              "Key: 12 3A 44 55 66 77 88 99\r\n"
              "Serial: 305419896\r\n"
              "Btn: 2\r\n"
              "Cnt: 100\r\n"
              "Type: 54\r\n"
              "CRC: 66\r\n"
              "Seed: 332553\r\n");

    subghz_saved_match_identity_t parsed;
    TEST_ASSERT_TRUE(subghz_saved_capture_read_identity(
        TEST_DIR "/capture.sub", &parsed));
    TEST_ASSERT_TRUE(parsed.is_recovered_psa);

    SubGHz_Dec_Info_t received;
    memset(&received, 0, sizeof(received));
    received.key = 0x123A445566778899ULL;
    received.serial_number = 0x12345678;
    received.rolling_code = 100;

    char path[128];
    TEST_ASSERT_FALSE(subghz_saved_capture_find_match(
        TEST_DIR, &received, "PSA", true, path, sizeof(path)));
}

static const char *s_update_text;
static bool s_update_writer_success;

static bool test_capture_writer(const char *path, bool m1_native,
                                const flipper_subghz_key_capture_t *capture)
{
    (void)m1_native;
    (void)capture;
    FILE *file = fopen(path, "wb");
    if (!file)
        return false;
    const size_t length = strlen(s_update_text);
    const bool complete = fwrite(s_update_text, 1, length, file) == length;
    const bool closed = fclose(file) == 0;
    return s_update_writer_success && complete && closed;
}

static void assert_file_equals(const char *path, const char *expected)
{
    char actual[256];
    FILE *file = fopen(path, "rb");
    TEST_ASSERT_NOT_NULL(file);
    const size_t length = fread(actual, 1, sizeof(actual) - 1u, file);
    TEST_ASSERT_EQUAL_INT(0, fclose(file));
    actual[length] = '\0';
    TEST_ASSERT_EQUAL_STRING(expected, actual);
}

void test_saved_capture_update_preserves_original_on_write_failure(void)
{
    const char *original = "original capture\n";
    write_tmp(TEST_CAPTURE_PATH, original);

    const flipper_subghz_key_capture_t capture = {0};
    s_update_text = "partial replacement";
    s_update_writer_success = false;
    TEST_ASSERT_FALSE(subghz_saved_capture_update(
        TEST_CAPTURE_PATH, false, &capture, test_capture_writer));
    assert_file_equals(TEST_CAPTURE_PATH, original);
    TEST_ASSERT_FALSE(access(TEST_UPDATE_TMP, F_OK) == 0);
}

void test_saved_capture_update_replaces_only_after_success(void)
{
    const char *original = "original capture\n";
    const char *replacement = "complete replacement\n";
    write_tmp(TEST_CAPTURE_PATH, original);

    const flipper_subghz_key_capture_t capture = {0};
    s_update_text = replacement;
    s_update_writer_success = true;
    TEST_ASSERT_TRUE(subghz_saved_capture_update(
        TEST_CAPTURE_PATH, false, &capture, test_capture_writer));
    assert_file_equals(TEST_CAPTURE_PATH, replacement);
    TEST_ASSERT_FALSE(access(TEST_UPDATE_TMP, F_OK) == 0);
    TEST_ASSERT_FALSE(access(TEST_UPDATE_BAK, F_OK) == 0);
}

void test_saved_capture_get_format_preserves_preset_and_native_modulation(void)
{
    write_tmp(TEST_DIR "/format.sub",
              "Filetype: Flipper SubGhz Key File\r\n"
              "Version: 1\r\n"
              "Preset: FuriHalSubGhzPreset2FSKDev238Async\r\n"
              "Protocol: Ford V1\r\n"
              "Key: 00 00 00 00 00 00 00 01\r\n");

    bool native = true;
    char preset[FLIPPER_SUBGHZ_PRESET_MAX_LEN];
    TEST_ASSERT_TRUE(subghz_saved_capture_get_format(
        TEST_DIR "/format.sub", &native, preset, sizeof(preset)));
    TEST_ASSERT_FALSE(native);
    TEST_ASSERT_EQUAL_STRING("FuriHalSubGhzPreset2FSKDev238Async", preset);

    write_tmp(TEST_DIR "/format.sgh",
              "Filetype: M1 SubGHz PACKET\r\n"
              "Version: 0.9\r\n"
              "Modulation: FSK\r\n"
              "Protocol: Ford V1\r\n"
              "Payload: 0x0000000000000001\r\n");
    TEST_ASSERT_TRUE(subghz_saved_capture_get_format(
        TEST_DIR "/format.sgh", &native, preset, sizeof(preset)));
    TEST_ASSERT_TRUE(native);
    TEST_ASSERT_EQUAL_STRING("FuriHalSubGhzPreset2FSKDev476Async", preset);
}

void test_saved_capture_from_history_preserves_decoded_fields(void)
{
    SubGHz_History_Entry_t entry;
    memset(&entry, 0, sizeof(entry));
    entry.frequency = 433920000;
    entry.info.protocol = PSA;
    entry.info.bit_len = 80;
    entry.info.key = 0x123A445566778899ULL;
    entry.info.te = 250;
    entry.info.serial_number = 0x12345678;
    entry.info.rolling_code = 0x4321;
    entry.info.button_id = 2;
    entry.info.key2 = 0x003600B1;

    flipper_subghz_key_capture_t capture;
    subghz_saved_capture_from_history(&entry, "PSA", &capture);
    TEST_ASSERT_EQUAL_UINT32(433920000, capture.frequency);
    TEST_ASSERT_EQUAL_STRING("PSA", capture.protocol);
    TEST_ASSERT_EQUAL_UINT64(0x123A445566778899ULL, capture.key);
    TEST_ASSERT_EQUAL_UINT32(0x12345678, capture.serial);
    TEST_ASSERT_EQUAL_UINT32(0x4321, capture.counter);
    TEST_ASSERT_EQUAL_UINT32(2, capture.button);
    TEST_ASSERT_EQUAL_UINT8(0x36, capture.type);
    TEST_ASSERT_EQUAL_UINT8(0xB1, capture.fx);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_saved_match_serial_and_counter_margin);
    RUN_TEST(test_saved_match_static_protocol_requires_full_key);
    RUN_TEST(test_saved_match_key_fallback_and_missing_counter);
    RUN_TEST(test_saved_match_serial_received_key_only_saved_falls_back_to_key);
    RUN_TEST(test_saved_capture_read_flipper_and_native_identities);
    RUN_TEST(test_saved_capture_find_match_scans_sub_and_sgh);
    RUN_TEST(test_saved_capture_find_match_searches_nested_directories);
    RUN_TEST(test_saved_capture_does_not_offer_update_for_recovered_psa);
    RUN_TEST(test_saved_capture_update_preserves_original_on_write_failure);
    RUN_TEST(test_saved_capture_update_replaces_only_after_success);
    RUN_TEST(test_saved_capture_get_format_preserves_preset_and_native_modulation);
    RUN_TEST(test_saved_capture_from_history_preserves_decoded_fields);
    return UNITY_END();
}
