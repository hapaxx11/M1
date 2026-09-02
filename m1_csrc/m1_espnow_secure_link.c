/* See COPYING.txt for license details. */

/**
 * @file   m1_espnow_secure_link.c
 * @brief  Firmware transport wrapper for optional ESP-NOW app encryption.
 */

#include "m1_espnow_secure_link.h"

#include <string.h>

#include "espnow_chunk.h"
#include "espnow_crypto.h"
#include "espnow_secure.h"

static espnow_crypto_key_t s_key;
static espnow_chunk_reasm_t s_reasm;
static uint8_t s_peer_mac[ESPNOW_MAC_LEN];
static uint8_t s_reasm_owner[ESPNOW_MAC_LEN];
static uint8_t s_next_msg_id;
static bool s_configured;
static bool s_encrypted;
static bool s_hello_sent;
static bool s_fallback;

void m1_espnow_secure_link_reset(void)
{
    memset(&s_key, 0, sizeof(s_key));
    memset(s_peer_mac, 0, sizeof(s_peer_mac));
    memset(s_reasm_owner, 0, sizeof(s_reasm_owner));
    espnow_chunk_reasm_init(&s_reasm);
    s_next_msg_id = 1u;
    s_configured = false;
    s_encrypted = false;
    s_hello_sent = false;
    s_fallback = false;
}

bool m1_espnow_secure_link_configure(
    const uint8_t local_mac[ESPNOW_MAC_LEN],
    const uint8_t peer_mac[ESPNOW_MAC_LEN],
    uint16_t confirm_code)
{
    m1_espnow_secure_link_reset();
    if (!espnow_secure_derive_pair_key(local_mac, peer_mac, confirm_code,
                                       &s_key))
        return false;
    memcpy(s_peer_mac, peer_mac, ESPNOW_MAC_LEN);
    s_configured = true;
    return true;
}

static bool secure_peer_matches(const uint8_t mac[ESPNOW_MAC_LEN])
{
    return s_configured && mac != NULL &&
           memcmp(mac, s_peer_mac, ESPNOW_MAC_LEN) == 0;
}

static void secure_send_control(espnow_secure_ctrl_t type)
{
    uint8_t frame[2];
    size_t frame_len = 0;

    if (!s_configured)
        return;
    if (!espnow_secure_build_control(type, frame, sizeof(frame), &frame_len))
        return;
    (void)m1_espnow_send(s_peer_mac, frame, frame_len);
}

static bool secure_send_encrypted(const uint8_t peer_mac[ESPNOW_MAC_LEN],
                                  const uint8_t *payload, size_t len)
{
    uint8_t envelope[ESPNOW_CRYPTO_ENVELOPE_MAX];
    size_t envelope_len = 0;
    espnow_chunk_splitter_t split;
    uint8_t frame[ESPNOW_CHUNK_FRAME_MAX];
    size_t frame_len = 0;

    if (len > ESPNOW_CRYPTO_PLAINTEXT_MAX)
        return false;
    if (espnow_crypto_seal(&s_key, payload, len, envelope, sizeof(envelope),
                           &envelope_len) != ESPNOW_CRYPTO_OK)
        return false;
    if (!espnow_chunk_split_init(&split, s_next_msg_id++, envelope,
                                 envelope_len))
        return false;

    while (espnow_chunk_split_next(&split, frame, sizeof(frame), &frame_len)) {
        if (!m1_espnow_send(peer_mac, frame, frame_len))
            return false;
    }
    return true;
}

static bool secure_send_chunked_plaintext(const uint8_t peer_mac[ESPNOW_MAC_LEN],
                                          const uint8_t *payload, size_t len)
{
    espnow_chunk_splitter_t split;
    uint8_t frame[ESPNOW_CHUNK_FRAME_MAX];
    size_t frame_len = 0;

    if (!espnow_chunk_split_init(&split, s_next_msg_id++, payload, len))
        return false;

    while (espnow_chunk_split_next(&split, frame, sizeof(frame), &frame_len)) {
        if (!m1_espnow_send(peer_mac, frame, frame_len))
            return false;
    }
    return true;
}

bool m1_espnow_secure_link_send(const uint8_t peer_mac[ESPNOW_MAC_LEN],
                                const uint8_t *payload, size_t len)
{
    if (peer_mac == NULL || payload == NULL || len == 0u)
        return false;

    if (secure_peer_matches(peer_mac) && s_encrypted) {
        return secure_send_encrypted(peer_mac, payload, len);
    }

    if (secure_peer_matches(peer_mac) && !s_encrypted && !s_hello_sent) {
        secure_send_control(ESPNOW_SECURE_CTRL_HELLO);
        s_hello_sent = true;
        s_fallback = true;
    }

    if (len > M1_ESPNOW_SEND_PAYLOAD_MAX)
        return secure_send_chunked_plaintext(peer_mac, payload, len);

    return m1_espnow_send(peer_mac, payload, len);
}

static bool secure_open_reassembled(uint8_t *buf, size_t buf_size,
                                    uint8_t *out_len)
{
    size_t plain_len = 0;

    if (!s_configured)
        return false;
    if (espnow_crypto_open(&s_key, s_reasm.msg, s_reasm.msg_len, buf,
                           buf_size, &plain_len) != ESPNOW_CRYPTO_OK)
        return false;
    if (plain_len > 255u)
        return false;

    *out_len = (uint8_t)plain_len;
    s_encrypted = true;
    s_fallback = false;
    return true;
}

static bool secure_copy_reassembled_plaintext(uint8_t from_mac[ESPNOW_MAC_LEN],
                                              const uint8_t raw_from[ESPNOW_MAC_LEN],
                                              uint8_t *buf, size_t buf_size,
                                              uint8_t *out_len)
{
    size_t copy_len;

    if (s_reasm.msg_len == 0u)
        return false;
    if (secure_peer_matches(raw_from) &&
        !espnow_secure_should_accept_plaintext(!s_encrypted, s_encrypted))
        return false;

    copy_len = s_reasm.msg_len;
    if (copy_len > buf_size)
        copy_len = buf_size;
    memcpy(from_mac, raw_from, ESPNOW_MAC_LEN);
    memcpy(buf, s_reasm.msg, copy_len);
    *out_len = (uint8_t)copy_len;
    if (s_configured && secure_peer_matches(raw_from) && !s_encrypted)
        s_fallback = true;
    return true;
}

bool m1_espnow_secure_link_recv(uint8_t from_mac[ESPNOW_MAC_LEN],
                                uint8_t *buf, size_t buf_size,
                                uint8_t *out_len)
{
    uint8_t raw_from[ESPNOW_MAC_LEN];
    uint8_t raw[ESPNOW_CHUNK_MSG_MAX];
    uint8_t raw_len = 0;

    if (from_mac == NULL || buf == NULL || out_len == NULL || buf_size == 0u)
        return false;

    while (m1_espnow_recv_msg(raw_from, raw, sizeof(raw), &raw_len)) {
        espnow_secure_ctrl_t ctrl = ESPNOW_SECURE_CTRL_NONE;
        espnow_chunk_status_t chunk_status;

        if (raw_len == 0u)
            continue;

        if (secure_peer_matches(raw_from) &&
            espnow_secure_parse_control(raw, raw_len, &ctrl)) {
            if (ctrl == ESPNOW_SECURE_CTRL_HELLO) {
                secure_send_control(ESPNOW_SECURE_CTRL_ACK);
            } else if (ctrl == ESPNOW_SECURE_CTRL_ACK) {
                s_encrypted = true;
                s_fallback = false;
            } else if (ctrl == ESPNOW_SECURE_CTRL_NACK) {
                s_fallback = true;
            }
            continue;
        }

        if (raw[0] == ESPNOW_APP_FRAG) {
            if (s_reasm.active &&
                memcmp(raw_from, s_reasm_owner, ESPNOW_MAC_LEN) != 0) {
                espnow_chunk_reasm_init(&s_reasm);
            }
            if (!s_reasm.active)
                memcpy(s_reasm_owner, raw_from, ESPNOW_MAC_LEN);
        }
        chunk_status = espnow_chunk_reasm_feed(&s_reasm, raw, raw_len);
        if (chunk_status == ESPNOW_CHUNK_COMPLETE) {
            if (secure_peer_matches(raw_from) &&
                s_reasm.msg[0] == ESPNOW_APP_CRYPTO_BASE) {
                if (secure_open_reassembled(buf, buf_size, out_len)) {
                    memcpy(from_mac, raw_from, ESPNOW_MAC_LEN);
                    memset(s_reasm_owner, 0, sizeof(s_reasm_owner));
                    return true;
                }
                continue;
            }
            if (secure_copy_reassembled_plaintext(from_mac, raw_from, buf,
                                                  buf_size, out_len)) {
                memset(s_reasm_owner, 0, sizeof(s_reasm_owner));
                return true;
            }
        }
        if (chunk_status == ESPNOW_CHUNK_ERROR)
            memset(s_reasm_owner, 0, sizeof(s_reasm_owner));
        if (chunk_status != ESPNOW_CHUNK_IGNORED)
            continue;

        if (!secure_peer_matches(raw_from) ||
            espnow_secure_should_accept_plaintext(!s_encrypted, s_encrypted)) {
            size_t copy_len = raw_len;
            if (copy_len > buf_size)
                copy_len = buf_size;
            memcpy(from_mac, raw_from, ESPNOW_MAC_LEN);
            memcpy(buf, raw, copy_len);
            *out_len = (uint8_t)copy_len;
            if (s_configured && !s_encrypted)
                s_fallback = true;
            return true;
        }
    }

    return false;
}

bool m1_espnow_secure_link_encrypted(void)
{
    return s_encrypted;
}

bool m1_espnow_secure_link_fallback(void)
{
    return s_fallback;
}
