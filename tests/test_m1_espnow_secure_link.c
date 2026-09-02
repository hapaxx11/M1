/* See COPYING.txt for license details. */

#include "unity.h"

#include <string.h>

#include "espnow_chunk.h"
#include "espnow_secure.h"
#include "m1_espnow_secure_link.h"

#define MAX_QUEUE  16u

typedef struct {
    uint8_t mac[ESPNOW_MAC_LEN];
    uint8_t data[ESPNOW_CHUNK_MSG_MAX];
    uint8_t len;
} queued_frame_t;

static const uint8_t s_local[ESPNOW_MAC_LEN] =
    { 0x02, 0x11, 0x22, 0x33, 0x44, 0x55 };
static const uint8_t s_peer[ESPNOW_MAC_LEN] =
    { 0x02, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE };
static const uint8_t s_other_peer[ESPNOW_MAC_LEN] =
    { 0x02, 0xAB, 0xBC, 0xCD, 0xDE, 0xEF };
static queued_frame_t s_rx[MAX_QUEUE];
static queued_frame_t s_tx[MAX_QUEUE];
static uint8_t s_rx_count;
static uint8_t s_rx_pos;
static uint8_t s_tx_count;

void setUp(void)
{
    s_rx_count = 0;
    s_rx_pos = 0;
    s_tx_count = 0;
    memset(s_rx, 0, sizeof(s_rx));
    memset(s_tx, 0, sizeof(s_tx));
    m1_espnow_secure_link_reset();
}

void tearDown(void) {}

static void push_rx(const uint8_t mac[ESPNOW_MAC_LEN],
                    const uint8_t *data, uint8_t len)
{
    TEST_ASSERT_LESS_THAN_UINT8(MAX_QUEUE, s_rx_count);
    memcpy(s_rx[s_rx_count].mac, mac, ESPNOW_MAC_LEN);
    memcpy(s_rx[s_rx_count].data, data, len);
    s_rx[s_rx_count].len = len;
    s_rx_count++;
}

bool m1_espnow_send(const uint8_t mac[6], const uint8_t *data, size_t len)
{
    TEST_ASSERT_LESS_OR_EQUAL_size_t(ESPNOW_CHUNK_FRAME_MAX, len);
    TEST_ASSERT_LESS_THAN_UINT8(MAX_QUEUE, s_tx_count);
    memcpy(s_tx[s_tx_count].mac, mac, ESPNOW_MAC_LEN);
    memcpy(s_tx[s_tx_count].data, data, len);
    s_tx[s_tx_count].len = (uint8_t)len;
    s_tx_count++;
    return true;
}

bool m1_espnow_recv_msg(uint8_t from_mac[6], uint8_t *buf,
                        size_t buf_size, uint8_t *out_len)
{
    queued_frame_t *f;

    if (s_rx_pos >= s_rx_count)
        return false;

    f = &s_rx[s_rx_pos++];
    memcpy(from_mac, f->mac, ESPNOW_MAC_LEN);
    *out_len = f->len;
    if (f->len > buf_size)
        *out_len = (uint8_t)buf_size;
    memcpy(buf, f->data, *out_len);
    return true;
}

bool m1_espnow_start(uint8_t channel) { (void)channel; return true; }
bool m1_espnow_stop(void) { return true; }
bool m1_espnow_announce(void) { return true; }
uint8_t m1_espnow_poll_peers(void *peers, uint8_t max_peers)
{
    (void)peers;
    (void)max_peers;
    return 0;
}
void m1_espnow_get_mac(uint8_t mac[6]) { memcpy(mac, s_local, ESPNOW_MAC_LEN); }
uint8_t m1_espnow_get_channel(void) { return 1u; }
void *m1_espnow_file_open(const char *path) { (void)path; return NULL; }
bool m1_espnow_file_write(void *handle, const uint8_t *data, size_t len)
{
    (void)handle;
    (void)data;
    (void)len;
    return false;
}
void m1_espnow_file_close(void *handle) { (void)handle; }

static void configure_link(void)
{
    TEST_ASSERT_TRUE(m1_espnow_secure_link_configure(s_local, s_peer, 1234u));
}

void test_first_send_sends_hello_then_plaintext_fallback(void)
{
    const uint8_t payload[] = { 0x20, 0x01, 'h', 'i' };

    configure_link();
    TEST_ASSERT_TRUE(m1_espnow_secure_link_send(s_peer, payload,
                                                sizeof(payload)));
    TEST_ASSERT_EQUAL_UINT8(2u, s_tx_count);
    TEST_ASSERT_EQUAL_UINT8(ESPNOW_SECURE_CTRL_HELLO, s_tx[0].data[0]);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, s_tx[1].data, sizeof(payload));
    TEST_ASSERT_TRUE(m1_espnow_secure_link_fallback());
}

void test_ack_enables_encrypted_fragment_send(void)
{
    uint8_t ack[2];
    size_t ack_len = 0;
    const uint8_t payload[] = { 0x30, 0x01, 'a', 'c', 't' };
    uint8_t from[ESPNOW_MAC_LEN];
    uint8_t out[8];
    uint8_t out_len = 0;

    configure_link();
    TEST_ASSERT_TRUE(espnow_secure_build_control(ESPNOW_SECURE_CTRL_ACK,
                                                 ack, sizeof(ack), &ack_len));
    push_rx(s_peer, ack, (uint8_t)ack_len);
    TEST_ASSERT_FALSE(m1_espnow_secure_link_recv(from, out, sizeof(out),
                                                 &out_len));
    TEST_ASSERT_TRUE(m1_espnow_secure_link_encrypted());

    TEST_ASSERT_TRUE(m1_espnow_secure_link_send(s_peer, payload,
                                                sizeof(payload)));
    TEST_ASSERT_GREATER_THAN_UINT8(1u, s_tx_count);
    TEST_ASSERT_EQUAL_UINT8(ESPNOW_APP_FRAG, s_tx[0].data[0]);
}

void test_plaintext_from_peer_is_rejected_after_encryption_enabled(void)
{
    uint8_t ack[2];
    size_t ack_len = 0;
    const uint8_t plain[] = { 0x20, 0x09, 'n', 'o' };
    uint8_t from[ESPNOW_MAC_LEN];
    uint8_t out[8];
    uint8_t out_len = 0;

    configure_link();
    TEST_ASSERT_TRUE(espnow_secure_build_control(ESPNOW_SECURE_CTRL_ACK,
                                                 ack, sizeof(ack), &ack_len));
    push_rx(s_peer, ack, (uint8_t)ack_len);
    push_rx(s_peer, plain, sizeof(plain));

    TEST_ASSERT_FALSE(m1_espnow_secure_link_recv(from, out, sizeof(out),
                                                 &out_len));
    TEST_ASSERT_TRUE(m1_espnow_secure_link_encrypted());
}

void test_plaintext_fallback_preserves_large_legacy_frame(void)
{
    uint8_t legacy[60];
    uint8_t from[ESPNOW_MAC_LEN];
    uint8_t out[sizeof(legacy)];
    uint8_t out_len = 0;

    for (uint8_t i = 0; i < sizeof(legacy); ++i)
        legacy[i] = (uint8_t)(0x10u + i);

    configure_link();
    push_rx(s_peer, legacy, sizeof(legacy));

    TEST_ASSERT_TRUE(m1_espnow_secure_link_recv(from, out, sizeof(out),
                                                &out_len));
    TEST_ASSERT_EQUAL_UINT8(sizeof(legacy), out_len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(legacy, out, sizeof(legacy));
    TEST_ASSERT_TRUE(m1_espnow_secure_link_fallback());
}

void test_long_plaintext_send_is_fragmented_for_fallback(void)
{
    uint8_t payload[82];
    espnow_chunk_reasm_t reasm;
    espnow_chunk_status_t st = ESPNOW_CHUNK_IGNORED;

    payload[0] = 0x20u;
    payload[1] = 0x04u;
    for (uint8_t i = 2u; i < sizeof(payload); ++i)
        payload[i] = (uint8_t)('a' + (i % 26u));

    configure_link();
    TEST_ASSERT_TRUE(m1_espnow_secure_link_send(s_peer, payload,
                                                sizeof(payload)));
    TEST_ASSERT_GREATER_THAN_UINT8(2u, s_tx_count);
    TEST_ASSERT_EQUAL_UINT8(ESPNOW_SECURE_CTRL_HELLO, s_tx[0].data[0]);
    TEST_ASSERT_EQUAL_UINT8(ESPNOW_APP_FRAG, s_tx[1].data[0]);

    espnow_chunk_reasm_init(&reasm);
    for (uint8_t i = 1u; i < s_tx_count; ++i)
        st = espnow_chunk_reasm_feed(&reasm, s_tx[i].data, s_tx[i].len);

    TEST_ASSERT_EQUAL(ESPNOW_CHUNK_COMPLETE, st);
    TEST_ASSERT_EQUAL_UINT16(sizeof(payload), reasm.msg_len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, reasm.msg, sizeof(payload));
}

void test_long_plaintext_fragments_reassemble_in_fallback(void)
{
    uint8_t payload[82];
    espnow_chunk_splitter_t split;
    uint8_t frame[ESPNOW_CHUNK_FRAME_MAX];
    size_t frame_len = 0;
    uint8_t from[ESPNOW_MAC_LEN];
    uint8_t out[sizeof(payload)];
    uint8_t out_len = 0;

    payload[0] = 0x20u;
    payload[1] = 0x05u;
    for (uint8_t i = 2u; i < sizeof(payload); ++i)
        payload[i] = (uint8_t)('A' + (i % 26u));

    configure_link();
    TEST_ASSERT_TRUE(espnow_chunk_split_init(&split, 0x51u, payload,
                                            sizeof(payload)));
    while (espnow_chunk_split_next(&split, frame, sizeof(frame), &frame_len))
        push_rx(s_peer, frame, (uint8_t)frame_len);

    TEST_ASSERT_TRUE(m1_espnow_secure_link_recv(from, out, sizeof(out),
                                                &out_len));
    TEST_ASSERT_EQUAL_UINT8(sizeof(payload), out_len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, out, sizeof(payload));
    TEST_ASSERT_TRUE(m1_espnow_secure_link_fallback());
}

void test_hello_does_not_reject_following_plaintext_fallback(void)
{
    uint8_t hello[2];
    size_t hello_len = 0;
    const uint8_t plain[] = { 0x20, 0x09, 'o', 'k' };
    uint8_t from[ESPNOW_MAC_LEN];
    uint8_t out[sizeof(plain)];
    uint8_t out_len = 0;

    configure_link();
    TEST_ASSERT_TRUE(espnow_secure_build_control(ESPNOW_SECURE_CTRL_HELLO,
                                                 hello, sizeof(hello),
                                                 &hello_len));
    push_rx(s_peer, hello, (uint8_t)hello_len);
    push_rx(s_peer, plain, sizeof(plain));
    TEST_ASSERT_TRUE(m1_espnow_secure_link_recv(from, out, sizeof(out),
                                                &out_len));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(plain, out, sizeof(plain));
    TEST_ASSERT_FALSE(m1_espnow_secure_link_encrypted());
}

void test_reassembly_does_not_mix_peers(void)
{
    uint8_t payload[82];
    espnow_chunk_splitter_t split;
    uint8_t frame[ESPNOW_CHUNK_FRAME_MAX];
    size_t frame_len = 0;
    uint8_t from[ESPNOW_MAC_LEN];
    uint8_t out[sizeof(payload)];
    uint8_t out_len = 0;

    memset(payload, 'a', sizeof(payload));
    payload[0] = 0x20u;
    TEST_ASSERT_TRUE(espnow_chunk_split_init(&split, 0x01u, payload,
                                             sizeof(payload)));
    TEST_ASSERT_TRUE(espnow_chunk_split_next(&split, frame, sizeof(frame),
                                             &frame_len));
    configure_link();
    push_rx(s_peer, frame, (uint8_t)frame_len);
    while (espnow_chunk_split_next(&split, frame, sizeof(frame), &frame_len))
        push_rx(s_other_peer, frame, (uint8_t)frame_len);
    TEST_ASSERT_FALSE(m1_espnow_secure_link_recv(from, out, sizeof(out),
                                                 &out_len));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_first_send_sends_hello_then_plaintext_fallback);
    RUN_TEST(test_ack_enables_encrypted_fragment_send);
    RUN_TEST(test_plaintext_from_peer_is_rejected_after_encryption_enabled);
    RUN_TEST(test_plaintext_fallback_preserves_large_legacy_frame);
    RUN_TEST(test_long_plaintext_send_is_fragmented_for_fallback);
    RUN_TEST(test_long_plaintext_fragments_reassemble_in_fallback);
    RUN_TEST(test_hello_does_not_reject_following_plaintext_fallback);
    RUN_TEST(test_reassembly_does_not_mix_peers);
    return UNITY_END();
}
