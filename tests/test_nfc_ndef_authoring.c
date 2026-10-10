#include "unity.h"

#include <string.h>

#include "nfc_ndef_authoring.h"
#include "nfc_ndef_encode.h"

void setUp(void) {}
void tearDown(void) {}

static uint8_t output[256];

void test_authoring_uri_encodes_automatically_compressed_url(void)
{
    size_t size = nfc_ndef_authoring_encode(
        NFC_NDEF_AUTHOR_URI, "https://example.com", NULL, output, sizeof(output));

    TEST_ASSERT_TRUE(size > 0);
    TEST_ASSERT_EQUAL_HEX8(0x03, output[0]);
    TEST_ASSERT_EQUAL_HEX8(0x04, output[6]);
}

void test_authoring_text_and_phone_records(void)
{
    size_t size = nfc_ndef_authoring_encode(
        NFC_NDEF_AUTHOR_TEXT, "Hello", NULL, output, sizeof(output));
    TEST_ASSERT_TRUE(size > 0);
    TEST_ASSERT_EQUAL_HEX8('T', output[5]);

    size = nfc_ndef_authoring_encode(
        NFC_NDEF_AUTHOR_PHONE, "+123456789", NULL, output, sizeof(output));
    TEST_ASSERT_TRUE(size > 0);
    TEST_ASSERT_EQUAL_HEX8('U', output[5]);
    TEST_ASSERT_EQUAL_HEX8(NDEF_URI_TEL, output[6]);
}

void test_authoring_wifi_handles_password_and_open_network(void)
{
    size_t secured_size = nfc_ndef_authoring_encode(
        NFC_NDEF_AUTHOR_WIFI, "M1-net", "secret", output, sizeof(output));
    TEST_ASSERT_TRUE(secured_size > 0);

    size_t open_size = nfc_ndef_authoring_encode(
        NFC_NDEF_AUTHOR_WIFI, "M1-net", "", output, sizeof(output));
    TEST_ASSERT_TRUE(open_size > 0);
    TEST_ASSERT_TRUE(open_size < secured_size);
}

void test_authoring_rejects_invalid_inputs_and_small_output(void)
{
    TEST_ASSERT_EQUAL_UINT(
        0, nfc_ndef_authoring_encode(NFC_NDEF_AUTHOR_URI, "", NULL, output, sizeof(output)));
    TEST_ASSERT_EQUAL_UINT(
        0, nfc_ndef_authoring_encode(NFC_NDEF_AUTHOR_URI, "https://x", NULL, NULL, sizeof(output)));
    TEST_ASSERT_EQUAL_UINT(
        0, nfc_ndef_authoring_encode(NFC_NDEF_AUTHOR_URI, "https://x", NULL, output, 1));
    TEST_ASSERT_EQUAL_UINT(
        0, nfc_ndef_authoring_encode((nfc_ndef_authoring_type_t)99, "x", NULL, output, sizeof(output)));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_authoring_uri_encodes_automatically_compressed_url);
    RUN_TEST(test_authoring_text_and_phone_records);
    RUN_TEST(test_authoring_wifi_handles_password_and_open_network);
    RUN_TEST(test_authoring_rejects_invalid_inputs_and_small_output);
    return UNITY_END();
}
