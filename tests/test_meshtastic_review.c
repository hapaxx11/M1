#include "unity.h"
#include "protocol.h"
#include "history.h"
#include "transport.h"

#include "lib/meshtastic_api/meshtastic/admin.pb.h"
#include "lib/meshtastic_api/meshtastic/mesh.pb.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static uint8_t s_tx_frame[4 + MAX_FRAME_SIZE];
static size_t s_tx_len;
static bool s_random_available;
static uint8_t s_random_calls;
static char s_status[32];

typedef struct {
    const uint8_t* buf;
    size_t len;
} TestPayload;

void setUp(void) {
    memset(s_tx_frame, 0, sizeof(s_tx_frame));
    s_tx_len = 0;
    s_random_available = true;
    s_random_calls = 0;
    s_status[0] = '\0';
}

void tearDown(void) {}

bool transport_is_up(ZeroMeshApp* app) {
    return app && app->uart_up;
}

void transport_tx(ZeroMeshApp* app, const uint8_t* data, size_t len) {
    (void)app;
    if(len <= sizeof(s_tx_frame)) {
        memcpy(s_tx_frame, data, len);
        s_tx_len = len;
    }
}

bool m1_crypto_random_bytes(uint8_t* buf, size_t len) {
    s_random_calls++;
    if(!s_random_available) return false;
    memset(buf, 0x5A, len);
    return true;
}

void history_add(ZeroMeshApp* app, const char* text, uint32_t from, uint32_t to, bool is_tx) {
    (void)app;
    (void)text;
    (void)from;
    (void)to;
    (void)is_tx;
}

void log_line(ZeroMeshApp* app, const char* fmt, ...) {
    (void)app;
    (void)fmt;
}

void set_status(ZeroMeshApp* app, const char* fmt, ...) {
    (void)app;
    va_list args;
    va_start(args, fmt);
    vsnprintf(s_status, sizeof(s_status), fmt, args);
    va_end(args);
}

static bool encode_bytes(
    pb_ostream_t* stream,
    const pb_field_t* field,
    void* const* arg) {
    const TestPayload* payload = (const TestPayload*)(*arg);
    return pb_encode_tag_for_field(stream, field) &&
           pb_encode_string(stream, payload->buf, payload->len);
}

void test_text_packet_uses_selected_channel(void) {
    ZeroMeshApp app = {0};
    app.uart_up = true;
    app.current_channel = 2;

    send_text_message(&app, "hello", 0xFFFFFFFFu);

    TEST_ASSERT_TRUE(s_tx_len > 4);
    TEST_ASSERT_EQUAL_HEX8(ZEROMESH_MAGIC0, s_tx_frame[0]);
    TEST_ASSERT_EQUAL_HEX8(ZEROMESH_MAGIC1, s_tx_frame[1]);
    uint16_t payload_len = (uint16_t)(((uint16_t)s_tx_frame[2] << 8) | s_tx_frame[3]);
    TEST_ASSERT_EQUAL_UINT(s_tx_len - 4, payload_len);

    meshtastic_ToRadio to = meshtastic_ToRadio_init_default;
    pb_istream_t is = pb_istream_from_buffer(s_tx_frame + 4, payload_len);
    TEST_ASSERT_TRUE(pb_decode(&is, meshtastic_ToRadio_fields, &to));
    TEST_ASSERT_EQUAL(meshtastic_ToRadio_packet_tag, to.which_payload_variant);
    TEST_ASSERT_EQUAL_UINT(2, to.payload_variant.packet.channel);
}

void test_private_channel_is_not_sent_without_secure_entropy(void) {
    ZeroMeshApp app = {0};
    app.uart_up = true;
    memcpy(app.cfg_ch_name, "Private", sizeof("Private"));
    s_random_available = false;

    set_channel_config(&app, true, true);

    TEST_ASSERT_EQUAL_UINT(1, s_random_calls);
    TEST_ASSERT_EQUAL_UINT(0, s_tx_len);
    TEST_ASSERT_EQUAL_STRING("Channel key failed", s_status);
}

void test_channel_response_retains_name_and_psk_callbacks(void) {
    ZeroMeshApp app = {0};
    uint8_t expected_psk[32];
    for(size_t i = 0; i < sizeof(expected_psk); i++) {
        expected_psk[i] = (uint8_t)(i + 1);
    }

    meshtastic_AdminMessage am = meshtastic_AdminMessage_init_default;
    am.which_payload_variant = meshtastic_AdminMessage_get_channel_response_tag;
    meshtastic_Channel* channel = &am.payload_variant.get_channel_response;
    channel->index = 0;
    channel->has_settings = true;
    channel->settings.has_module_settings = true;
    channel->settings.module_settings.position_precision = 32;

    TestPayload name = {.buf = (const uint8_t*)"private-main", .len = 12};
    TestPayload psk = {.buf = expected_psk, .len = sizeof(expected_psk)};
    channel->settings.name.funcs.encode = encode_bytes;
    channel->settings.name.arg = &name;
    channel->settings.psk.funcs.encode = encode_bytes;
    channel->settings.psk.arg = &psk;

    uint8_t response[128];
    pb_ostream_t os = pb_ostream_from_buffer(response, sizeof(response));
    TEST_ASSERT_TRUE(pb_encode(&os, meshtastic_AdminMessage_fields, &am));

    protocol_handle_admin(&app, response, os.bytes_written);

    TEST_ASSERT_TRUE(app.cfg_ch_known);
    TEST_ASSERT_EQUAL_STRING("private-main", app.cfg_ch_name);
    TEST_ASSERT_EQUAL_UINT(sizeof(expected_psk), app.cfg_ch_psk_len);
    TEST_ASSERT_EQUAL_MEMORY(expected_psk, app.cfg_ch_psk, sizeof(expected_psk));
    TEST_ASSERT_EQUAL_UINT8(1, app.cfg_ch_private);
    TEST_ASSERT_EQUAL_UINT8(1, app.cfg_ch_pos);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_text_packet_uses_selected_channel);
    RUN_TEST(test_private_channel_is_not_sent_without_secure_entropy);
    RUN_TEST(test_channel_response_retains_name_and_psk_callbacks);
    return UNITY_END();
}
