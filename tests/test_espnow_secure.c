/* See COPYING.txt for license details. */

#include "unity.h"

#include <string.h>

#include "espnow_chunk.h"
#include "espnow_secure.h"

void setUp(void) {}
void tearDown(void) {}

static void make_key(espnow_crypto_key_t *key)
{
    const uint8_t local[ESPNOW_SECURE_MAC_LEN] =
        { 0x02, 0x11, 0x22, 0x33, 0x44, 0x55 };
    const uint8_t peer[ESPNOW_SECURE_MAC_LEN] =
        { 0x02, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE };

    TEST_ASSERT_TRUE(espnow_secure_derive_pair_key(local, peer, 1234u, key));
}

void test_pair_key_is_stable_across_roles(void)
{
    const uint8_t a[ESPNOW_SECURE_MAC_LEN] =
        { 0x02, 0x11, 0x22, 0x33, 0x44, 0x55 };
    const uint8_t b[ESPNOW_SECURE_MAC_LEN] =
        { 0x02, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE };
    espnow_crypto_key_t ab;
    espnow_crypto_key_t ba;

    TEST_ASSERT_TRUE(espnow_secure_derive_pair_key(a, b, 4321u, &ab));
    TEST_ASSERT_TRUE(espnow_secure_derive_pair_key(b, a, 4321u, &ba));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(ab.enc_key, ba.enc_key, ESPNOW_CRYPTO_KEY_LEN);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(ab.mac_key, ba.mac_key, ESPNOW_CRYPTO_KEY_LEN);
}

void test_pair_key_changes_with_confirm_code(void)
{
    const uint8_t a[ESPNOW_SECURE_MAC_LEN] =
        { 0x02, 0x11, 0x22, 0x33, 0x44, 0x55 };
    const uint8_t b[ESPNOW_SECURE_MAC_LEN] =
        { 0x02, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE };
    espnow_crypto_key_t k1;
    espnow_crypto_key_t k2;

    TEST_ASSERT_TRUE(espnow_secure_derive_pair_key(a, b, 1111u, &k1));
    TEST_ASSERT_TRUE(espnow_secure_derive_pair_key(a, b, 2222u, &k2));
    TEST_ASSERT_NOT_EQUAL(0, memcmp(k1.enc_key, k2.enc_key,
                                    ESPNOW_CRYPTO_KEY_LEN));
}

void test_control_frames_roundtrip(void)
{
    uint8_t frame[2];
    size_t len = 0;
    espnow_secure_ctrl_t type = ESPNOW_SECURE_CTRL_NONE;

    TEST_ASSERT_TRUE(espnow_secure_build_control(ESPNOW_SECURE_CTRL_HELLO,
                                                 frame, sizeof(frame), &len));
    TEST_ASSERT_EQUAL_size_t(2u, len);
    TEST_ASSERT_TRUE(espnow_secure_parse_control(frame, len, &type));
    TEST_ASSERT_EQUAL(ESPNOW_SECURE_CTRL_HELLO, type);

    TEST_ASSERT_TRUE(espnow_secure_build_control(ESPNOW_SECURE_CTRL_ACK,
                                                 frame, sizeof(frame), &len));
    TEST_ASSERT_TRUE(espnow_secure_parse_control(frame, len, &type));
    TEST_ASSERT_EQUAL(ESPNOW_SECURE_CTRL_ACK, type);
}

void test_control_rejects_bad_version_and_type(void)
{
    uint8_t frame[2] = { ESPNOW_SECURE_CTRL_HELLO, 2u };
    espnow_secure_ctrl_t type = ESPNOW_SECURE_CTRL_NONE;

    TEST_ASSERT_FALSE(espnow_secure_parse_control(frame, sizeof(frame), &type));
    frame[0] = 0xE8u;
    frame[1] = 1u;
    TEST_ASSERT_FALSE(espnow_secure_parse_control(frame, sizeof(frame), &type));
    TEST_ASSERT_FALSE(espnow_secure_build_control(ESPNOW_SECURE_CTRL_NONE,
                                                 frame, sizeof(frame), NULL));
}

void test_encrypted_payload_survives_chunking(void)
{
    espnow_crypto_key_t key;
    const uint8_t plain[] = { 0x20, 0x07, 'h', 'e', 'l', 'l', 'o' };
    uint8_t envelope[ESPNOW_CRYPTO_ENVELOPE_MAX];
    size_t envelope_len = 0;
    espnow_chunk_splitter_t split;
    espnow_chunk_reasm_t reasm;
    uint8_t frame[ESPNOW_CHUNK_FRAME_MAX];
    size_t frame_len = 0;
    uint8_t opened[sizeof(plain)];
    size_t opened_len = 0;
    espnow_chunk_status_t st = ESPNOW_CHUNK_IGNORED;

    make_key(&key);
    TEST_ASSERT_EQUAL(ESPNOW_CRYPTO_OK,
        espnow_crypto_seal(&key, plain, sizeof(plain), envelope,
                           sizeof(envelope), &envelope_len));
    TEST_ASSERT_GREATER_THAN_UINT(ESPNOW_CHUNK_FRAME_MAX, envelope_len);

    TEST_ASSERT_TRUE(espnow_chunk_split_init(&split, 0x42u, envelope,
                                            envelope_len));
    espnow_chunk_reasm_init(&reasm);
    while (espnow_chunk_split_next(&split, frame, sizeof(frame), &frame_len))
        st = espnow_chunk_reasm_feed(&reasm, frame, frame_len);

    TEST_ASSERT_EQUAL(ESPNOW_CHUNK_COMPLETE, st);
    TEST_ASSERT_EQUAL(ESPNOW_CRYPTO_OK,
        espnow_crypto_open(&key, reasm.msg, reasm.msg_len, opened,
                           sizeof(opened), &opened_len));
    TEST_ASSERT_EQUAL_size_t(sizeof(plain), opened_len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(plain, opened, sizeof(plain));
}

void test_plaintext_policy_allows_configured_fallback(void)
{
    TEST_ASSERT_TRUE(espnow_secure_should_accept_plaintext(true, true));
    TEST_ASSERT_TRUE(espnow_secure_should_accept_plaintext(true, false));
    TEST_ASSERT_TRUE(espnow_secure_should_accept_plaintext(false, false));
    TEST_ASSERT_FALSE(espnow_secure_should_accept_plaintext(false, true));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_pair_key_is_stable_across_roles);
    RUN_TEST(test_pair_key_changes_with_confirm_code);
    RUN_TEST(test_control_frames_roundtrip);
    RUN_TEST(test_control_rejects_bad_version_and_type);
    RUN_TEST(test_encrypted_payload_survives_chunking);
    RUN_TEST(test_plaintext_policy_allows_configured_fallback);
    return UNITY_END();
}
