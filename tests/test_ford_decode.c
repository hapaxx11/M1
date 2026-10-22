/* See COPYING.txt for license details. */

#include "unity.h"
#include "m1_sub_ghz_decenc.h"
#include "subghz_raw_decoder.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

void m1_ford_v0_decode_fields(uint64_t key1, uint16_t key2,
                              uint32_t *serial, uint8_t *button,
                              uint32_t *count);
bool m1_ford_v0_verify_crc(uint64_t key1, uint16_t key2);
bool m1_ford_v1_parse_frame(const uint8_t raw[17], uint64_t *key1,
                            uint64_t *key2, uint32_t *serial, uint8_t *button,
                            uint32_t *count, uint16_t *crc, bool *fields_valid);
bool m1_ford_v2_parse_frame(const uint8_t raw[13], uint64_t *key,
                            uint64_t *tail_raw, uint32_t *serial,
                            uint16_t *counter, uint32_t *tail31,
                            uint8_t *button);
uint8_t subghz_decode_ford_v0(uint16_t protocol_index, uint16_t pulse_count);
uint8_t subghz_decode_ford_v1(uint16_t protocol_index, uint16_t pulse_count);
uint8_t subghz_decode_ford_v2(uint16_t protocol_index, uint16_t pulse_count);

static uint16_t s_pulse_count;

static bool ford_v1_raw_try_decode(const uint16_t *pulse_buf,
                                   uint16_t pulse_count,
                                   SubGhzRawDecodeResult *result,
                                   void *user_ctx)
{
    (void)user_ctx;
    memcpy(subghz_decenc_ctl.pulse_times, pulse_buf,
           pulse_count * sizeof(*pulse_buf));
    if (subghz_decode_ford_v1(FORD_V1, pulse_count) != 0)
        return false;

    result->protocol = FORD_V1;
    result->key = subghz_decenc_ctl.n64_decodedvalue;
    result->bit_len = subghz_decenc_ctl.ndecodedbitlength;
    result->te = 65;
    result->serial_number = subghz_decenc_ctl.n32_serialnumber;
    return true;
}

static void emit_merge(bool level, uint32_t duration)
{
    TEST_ASSERT_GREATER_THAN_UINT32(0, duration);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(UINT16_MAX, duration);
    if (s_pulse_count == 0)
    {
        if (!level)
        {
            /* M1 pulse arrays treat index 0 as HIGH.  Insert a 1 µs placeholder
             * for protocols (Ford V2) whose encoder starts with a LOW pulse. */
            subghz_decenc_ctl.pulse_times[s_pulse_count++] = 1;
        }
        else
        {
            subghz_decenc_ctl.pulse_times[s_pulse_count++] = (uint16_t)duration;
            return;
        }
    }

    const bool previous_level = ((s_pulse_count - 1u) & 1u) == 0u;
    if (level == previous_level)
        subghz_decenc_ctl.pulse_times[s_pulse_count - 1u] += (uint16_t)duration;
    else
    {
        TEST_ASSERT_LESS_THAN_UINT16(PACKET_PULSE_COUNT_MAX, s_pulse_count);
        subghz_decenc_ctl.pulse_times[s_pulse_count++] = (uint16_t)duration;
    }
}

void setUp(void)
{
    memset(&subghz_decenc_ctl, 0, sizeof(subghz_decenc_ctl));
    s_pulse_count = 0;
}

void tearDown(void) {}

static const uint8_t ford_v0_crc_matrix[64] = {
    0xDA, 0xB5, 0x55, 0x6A, 0xAA, 0xAA, 0xAA, 0xD5,
    0xB6, 0x6C, 0xCC, 0xD9, 0x99, 0x99, 0x99, 0xB3,
    0x71, 0xE3, 0xC3, 0xC7, 0x87, 0x87, 0x87, 0x8F,
    0x0F, 0xE0, 0x3F, 0xC0, 0x7F, 0x80, 0x7F, 0x80,
    0x00, 0x1F, 0xFF, 0xC0, 0x00, 0x7F, 0xFF, 0x80,
    0x00, 0x00, 0x00, 0x3F, 0xFF, 0xFF, 0xFF, 0x80,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7F,
    0x23, 0x12, 0x94, 0x84, 0x35, 0xF4, 0x55, 0x84,
};

static uint8_t parity8(uint8_t value)
{
    uint8_t parity = 0;
    while (value)
    {
        parity ^= value & 1u;
        value >>= 1;
    }
    return parity;
}

static uint8_t v0_crc(uint8_t *buf)
{
    uint8_t crc = 0;
    for (uint8_t row = 0; row < 8; row++)
    {
        uint8_t xor_sum = 0;
        for (uint8_t col = 0; col < 8; col++)
            xor_sum ^= ford_v0_crc_matrix[row * 8u + col] & buf[col + 1u];
        if (parity8(xor_sum))
            crc |= (uint8_t)(1u << row);
    }
    return crc;
}

static uint8_t v0_checksum(uint32_t serial, uint32_t count, uint8_t button)
{
    return (uint8_t)((((count >> 24) & 0xFF) + ((count >> 16) & 0xFF) +
                      ((count >> 8) & 0xFF) + (count & 0xFF) +
                      ((serial >> 24) & 0xFF) + ((serial >> 16) & 0xFF) +
                      ((serial >> 8) & 0xFF) + (serial & 0xFF) +
                      (button << 3)) & 0xFF);
}

static void v0_encode_key(uint8_t header, uint32_t serial, uint8_t button,
                          uint32_t count, uint8_t checksum, uint64_t *key1)
{
    uint8_t buf[8] = {0};
    buf[0] = header;
    buf[1] = serial >> 24;
    buf[2] = serial >> 16;
    buf[3] = serial >> 8;
    buf[4] = serial;
    buf[5] = ((button & 0x0Fu) << 3) | ((count >> 16) & 0x0Fu);

    const uint8_t count_mid = count >> 8;
    const uint8_t count_low = count;
    const uint8_t post6 = (count_mid & 0xAAu) | (count_low & 0x55u);
    const uint8_t post7 = (count_low & 0xAAu) | (count_mid & 0x55u);
    const uint8_t parity = parity8(checksum);

    uint8_t xor_byte;
    if (checksum != 0 && parity)
    {
        xor_byte = post7;
        for (uint8_t i = 1; i <= 5; i++)
            buf[i] ^= xor_byte;
        buf[6] = post6 ^ xor_byte;
        buf[7] = post7;
    }
    else
    {
        xor_byte = post6;
        for (uint8_t i = 1; i <= 5; i++)
            buf[i] ^= xor_byte;
        buf[6] = post6;
        buf[7] = post7 ^ xor_byte;
    }

    *key1 = 0;
    for (uint8_t i = 0; i < 8; i++)
        *key1 = (*key1 << 8) | buf[i];
}

static void v0_add_bit(bool *previous, bool bit)
{
    if (!*previous && !bit)
    {
        emit_merge(true, 250);
        emit_merge(false, 250);
    }
    else if (!*previous && bit)
    {
        emit_merge(true, 500);
    }
    else if (*previous && !bit)
    {
        emit_merge(false, 500);
    }
    else
    {
        emit_merge(false, 250);
        emit_merge(true, 250);
    }
    *previous = bit;
}

static void v0_build_waveform(uint64_t key1, uint16_t key2)
{
    const uint64_t tx_key1 = ~key1;
    const uint16_t tx_key2 = (uint16_t)~key2;

    s_pulse_count = 0;
    emit_merge(true, 250);
    emit_merge(false, 500);
    for (uint8_t i = 0; i < 4; i++)
    {
        emit_merge(true, 500);
        emit_merge(false, 500);
    }
    emit_merge(true, 250);
    emit_merge(false, 3500);

    bool previous = (tx_key1 >> 62) & 1u;
    if (previous)
        emit_merge(true, 500);
    else
    {
        emit_merge(true, 250);
        emit_merge(false, 500);
    }
    for (int bit = 61; bit >= 0; bit--)
        v0_add_bit(&previous, (tx_key1 >> bit) & 1u);
    for (int bit = 15; bit >= 0; bit--)
        v0_add_bit(&previous, (tx_key2 >> bit) & 1u);
}

static void v0_make_frame(uint64_t *key1, uint16_t *key2)
{
    const uint32_t serial = 0x12345678;
    const uint8_t button = 0x02;
    const uint32_t count = 0x1234;
    const uint8_t checksum = v0_checksum(serial, count, button);
    v0_encode_key(0xA5, serial, button, count, checksum, key1);

    uint8_t crc_buf[9] = {0};
    for (uint8_t i = 0; i < 8; i++)
        crc_buf[i] = (uint8_t)(*key1 >> (56u - i * 8u));
    crc_buf[8] = checksum;
    *key2 = ((uint16_t)checksum << 8) | (uint8_t)(v0_crc(crc_buf) ^ 0x80u);
}

void test_ford_v0_fields_crc_and_waveform(void)
{
    uint64_t key1 = 0;
    uint16_t key2 = 0;
    v0_make_frame(&key1, &key2);

    uint32_t serial = 0;
    uint32_t count = 0;
    uint8_t button = 0;
    m1_ford_v0_decode_fields(key1, key2, &serial, &button, &count);
    TEST_ASSERT_EQUAL_UINT32(0x12345678, serial);
    TEST_ASSERT_EQUAL_UINT8(2, button);
    TEST_ASSERT_EQUAL_UINT32(0x1234, count);
    TEST_ASSERT_TRUE(m1_ford_v0_verify_crc(key1, key2));
    TEST_ASSERT_FALSE(m1_ford_v0_verify_crc(key1, key2 ^ 1u));

    v0_build_waveform(key1, key2);
    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_ford_v0(FORD_V0, s_pulse_count));
    TEST_ASSERT_EQUAL_UINT64(key1, subghz_decenc_ctl.n64_decodedvalue);
    TEST_ASSERT_EQUAL_UINT32(0x12345678, subghz_decenc_ctl.n32_serialnumber);
    TEST_ASSERT_EQUAL_UINT32(0x1234, subghz_decenc_ctl.n32_rollingcode);
    TEST_ASSERT_EQUAL_UINT8(2, subghz_decenc_ctl.n8_buttonid);
    TEST_ASSERT_EQUAL_UINT16(key2, subghz_decenc_ctl.n32_key2);
}

void test_ford_v0_rejects_truncated_waveform(void)
{
    uint64_t key1 = 0;
    uint16_t key2 = 0;
    v0_make_frame(&key1, &key2);
    v0_build_waveform(key1, key2);
    s_pulse_count = 40;
    TEST_ASSERT_EQUAL_UINT8(1, subghz_decode_ford_v0(FORD_V0, s_pulse_count));
}

static uint16_t v1_crc16(const uint8_t *data, size_t len)
{
    uint16_t remainder = 0;
    for (size_t byte = 0; byte < len; byte++)
    {
        remainder ^= (uint16_t)data[byte] << 8;
        for (uint8_t bit = 0; bit < 8; bit++)
            remainder = (remainder & 0x8000u)
                            ? (uint16_t)((remainder << 1) ^ 0x1021u)
                            : (uint16_t)(remainder << 1);
    }
    return remainder;
}

static void v1_encode_inverse_block(uint8_t block[9])
{
    uint8_t sum = 0;
    for (uint8_t i = 1; i <= 7; i++)
        sum = (uint8_t)(sum + block[i]);

    const uint8_t p6 = block[6];
    const uint8_t p7 = block[7];
    const uint8_t post6 = (p6 & 0xAAu) | (p7 & 0x55u);
    const uint8_t post7 = (p7 & 0xAAu) | (p6 & 0x55u);
    const uint8_t xorv = post6 ^ post7;

    uint8_t xor_byte;
    if ((parity8(sum) & 1u) != 0u)
    {
        block[6] = xorv;
        block[7] = post7;
        xor_byte = post7;
    }
    else
    {
        block[6] = post6;
        block[7] = xorv;
        xor_byte = post6;
    }
    for (uint8_t i = 1; i <= 5; i++)
        block[i] ^= xor_byte;
}

static void v1_make_raw(uint8_t raw[17], uint32_t serial, uint8_t button,
                        uint32_t count)
{
    uint8_t plain[9] = {0};
    plain[0] = serial;
    plain[1] = serial >> 24;
    plain[2] = serial >> 16;
    plain[3] = serial >> 8;
    plain[4] = plain[0]; /* strict-layout identity byte */
    plain[5] = ((button & 0x0Fu) << 4) | ((count >> 16) & 0x0Fu);
    plain[6] = count >> 8;
    plain[7] = count;

    memset(raw, 0, 17);
    raw[3] = serial >> 24;
    raw[4] = serial >> 16;
    raw[5] = serial >> 8;
    memcpy(&raw[6], plain, 9);
    v1_encode_inverse_block(&raw[6]);

    if ((raw[0] | raw[1] | raw[2]) == 0)
    {
        uint32_t lo = ((uint32_t)raw[3] << 24) | ((uint32_t)raw[4] << 16) |
                      ((uint32_t)raw[5] << 8) | raw[6];
        lo = (lo & 0xFF00FFFFu) | 0x80000u;
        raw[3] = lo >> 24;
        raw[4] = lo >> 16;
        raw[5] = lo >> 8;
        raw[6] = lo;
    }

    const uint16_t crc = v1_crc16(&raw[3], 12);
    raw[15] = crc >> 8;
    raw[16] = crc;
}

static void v1_build_waveform(const uint8_t raw[17])
{
    s_pulse_count = 0;
    for (uint16_t i = 0; i < 400; i++)
    {
        emit_merge(true, 130);
        emit_merge(false, 130);
    }
    emit_merge(true, 130);
    emit_merge(false, 65);
    for (uint8_t byte = 0; byte < 17; byte++)
    {
        for (int bit = 7; bit >= 0; bit--)
        {
            const bool value = (raw[byte] >> bit) & 1u;
            emit_merge(value, 65);
            emit_merge(!value, 65);
        }
    }
    emit_merge(false, 260);
}

void test_ford_v1_frame_and_full_preamble_waveform(void)
{
    uint8_t raw[17];
    v1_make_raw(raw, 0x12345678, 2, 0x1234);

    uint64_t key1 = 0;
    uint64_t key2 = 0;
    uint32_t serial = 0;
    uint32_t count = 0;
    uint16_t crc = 0;
    uint8_t button = 0;
    bool fields_valid = false;
    TEST_ASSERT_TRUE(m1_ford_v1_parse_frame(raw, &key1, &key2, &serial,
                                            &button, &count, &crc, &fields_valid));

    uint64_t expected_key1 = 0;
    uint64_t expected_key2 = 0;
    for (uint8_t i = 0; i < 7; i++)
        expected_key1 = (expected_key1 << 8) | raw[i];
    for (uint8_t i = 0; i < 8; i++)
        expected_key2 = (expected_key2 << 8) | raw[7u + i];
    TEST_ASSERT_EQUAL_UINT64(expected_key1, key1);
    TEST_ASSERT_EQUAL_UINT64(expected_key2, key2);
    TEST_ASSERT_EQUAL_UINT16(v1_crc16(&raw[3], 12), crc);
    TEST_ASSERT_TRUE(fields_valid);
    TEST_ASSERT_EQUAL_UINT32(0x12345678, serial);
    TEST_ASSERT_EQUAL_UINT8(2, button);
    TEST_ASSERT_EQUAL_UINT32(0x1234, count);

    v1_build_waveform(raw);
    TEST_ASSERT_LESS_THAN_UINT16(PACKET_PULSE_COUNT_MAX, s_pulse_count);
    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_ford_v1(FORD_V1, s_pulse_count));
    TEST_ASSERT_EQUAL_UINT64(expected_key1, subghz_decenc_ctl.n64_decodedvalue);
    TEST_ASSERT_EQUAL_UINT64(expected_key2, subghz_decenc_ctl.n32_key2);
    TEST_ASSERT_EQUAL_UINT16(136, subghz_decenc_ctl.ndecodedbitlength);
    TEST_ASSERT_EQUAL_UINT32(0x12345678, subghz_decenc_ctl.n32_serialnumber);
    TEST_ASSERT_EQUAL_UINT32(0x1234, subghz_decenc_ctl.n32_rollingcode);
    TEST_ASSERT_EQUAL_UINT8(2, subghz_decenc_ctl.n8_buttonid);
}

void test_ford_v1_rejects_bad_crc(void)
{
    uint8_t raw[17];
    v1_make_raw(raw, 0x12345678, 2, 0x1234);
    raw[16] ^= 1u;
    TEST_ASSERT_FALSE(m1_ford_v1_parse_frame(raw, NULL, NULL, NULL, NULL,
                                             NULL, NULL, NULL));
    v1_build_waveform(raw);
    TEST_ASSERT_EQUAL_UINT8(1, subghz_decode_ford_v1(FORD_V1, s_pulse_count));
}

static uint8_t v2_parity(uint8_t value)
{
    return parity8(value);
}

static void v2_make_raw(uint8_t raw[13], uint32_t serial, uint8_t button,
                        uint16_t counter, uint32_t tail31)
{
    memset(raw, 0, 13);
    raw[0] = 0x7F;
    raw[1] = 0xA7;
    raw[2] = serial >> 24;
    raw[3] = serial >> 16;
    raw[4] = serial >> 8;
    raw[5] = serial;
    raw[6] = button;
    raw[7] = ((counter >> 9) & 0x7Fu) | (v2_parity(button) << 7);
    raw[8] = counter >> 1;
    raw[9] = ((counter & 1u) << 7) | (uint8_t)(tail31 >> 24);
    raw[10] = tail31 >> 16;
    raw[11] = tail31 >> 8;
    raw[12] = tail31;
}

static void v2_build_waveform(const uint8_t raw[13], uint16_t sync_duration)
{
    s_pulse_count = 0;
    for (uint8_t i = 0; i < 70; i++)
    {
        emit_merge(false, 200);
        emit_merge(true, 200);
    }
    emit_merge(false, sync_duration);
    emit_merge(true, 200);
    for (uint16_t bit_pos = 1; bit_pos < 104; bit_pos++)
    {
        const uint8_t byte = bit_pos / 8u;
        const uint8_t shift = 7u - (bit_pos % 8u);
        const bool bit = (raw[byte] >> shift) & 1u;
        emit_merge(bit, 200);
        emit_merge(!bit, 200);
    }
}

void test_ford_v2_frame_and_waveform(void)
{
    uint8_t raw[13];
    const uint32_t tail31 = 0x5A5A5A5A;
    v2_make_raw(raw, 0x12345678, 0x11, 0x1234, tail31);

    uint64_t key = 0;
    uint64_t tail_raw = 0;
    uint32_t serial = 0;
    uint32_t parsed_tail31 = 0;
    uint16_t counter = 0;
    uint8_t button = 0;
    TEST_ASSERT_TRUE(m1_ford_v2_parse_frame(raw, &key, &tail_raw, &serial,
                                            &counter, &parsed_tail31, &button));

    uint64_t expected_key = 0;
    uint64_t expected_tail = 0;
    for (uint8_t i = 0; i < 8; i++)
        expected_key = (expected_key << 8) | raw[i];
    for (uint8_t i = 0; i < 5; i++)
        expected_tail = (expected_tail << 8) | raw[8u + i];
    TEST_ASSERT_EQUAL_UINT64(expected_key, key);
    TEST_ASSERT_EQUAL_UINT64(expected_tail, tail_raw);
    TEST_ASSERT_EQUAL_UINT32(0x12345678, serial);
    TEST_ASSERT_EQUAL_UINT16(0x1234, counter);
    TEST_ASSERT_EQUAL_UINT32(tail31, parsed_tail31);
    TEST_ASSERT_EQUAL_UINT8(0x11, button);

    v2_build_waveform(raw, 476);
    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_ford_v2(FORD_V2, s_pulse_count));
    TEST_ASSERT_EQUAL_UINT64(expected_key, subghz_decenc_ctl.n64_decodedvalue);
    TEST_ASSERT_EQUAL_UINT64(expected_tail, subghz_decenc_ctl.n32_key2);
    TEST_ASSERT_EQUAL_UINT32(0x12345678, subghz_decenc_ctl.n32_serialnumber);
    TEST_ASSERT_EQUAL_UINT32(0x1234, subghz_decenc_ctl.n32_rollingcode);
    TEST_ASSERT_EQUAL_UINT8(0x11, subghz_decenc_ctl.n8_buttonid);
}

void test_ford_v2_rejects_bad_sync_and_button(void)
{
    uint8_t raw[13];
    v2_make_raw(raw, 0x12345678, 0x11, 0x1234, 0x5A5A5A5A);
    raw[0] = 0x7E;
    TEST_ASSERT_FALSE(m1_ford_v2_parse_frame(raw, NULL, NULL, NULL, NULL,
                                             NULL, NULL));
    raw[0] = 0x7F;
    raw[6] = 0x30;
    TEST_ASSERT_FALSE(m1_ford_v2_parse_frame(raw, NULL, NULL, NULL, NULL,
                                             NULL, NULL));

    v2_make_raw(raw, 0x12345678, 0x11, 0x1234, 0x5A5A5A5A);
    raw[6] = 0x30;
    v2_build_waveform(raw, 476);
    TEST_ASSERT_EQUAL_UINT8(1, subghz_decode_ford_v2(FORD_V2, s_pulse_count));

    v2_make_raw(raw, 0x12345678, 0x11, 0x1234, 0x5A5A5A5A);
    raw[0] = 0x7E;
    v2_build_waveform(raw, 476);
    TEST_ASSERT_EQUAL_UINT8(1, subghz_decode_ford_v2(FORD_V2, s_pulse_count));

    v2_make_raw(raw, 0x12345678, 0x11, 0x1234, 0x5A5A5A5A);
    raw[1] = 0xA6;
    v2_build_waveform(raw, 476);
    TEST_ASSERT_EQUAL_UINT8(1, subghz_decode_ford_v2(FORD_V2, s_pulse_count));
}

void test_ford_v2_sync_timing_jitter_is_accepted(void)
{
    uint8_t raw[13];
    v2_make_raw(raw, 0x12345678, 0x11, 0x1234, 0x5A5A5A5A);
    v2_build_waveform(raw, 456);

    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_ford_v2(FORD_V2, s_pulse_count));
    TEST_ASSERT_EQUAL_UINT32(0x12345678, subghz_decenc_ctl.n32_serialnumber);
}

void test_ford_v1_short_symbols_survive_raw_decode_pipeline(void)
{
    uint8_t raw_frame[17];
    v1_make_raw(raw_frame, 0x12345678, 2, 0x1234);
    v1_build_waveform(raw_frame);

    TEST_ASSERT_GREATER_THAN_UINT16(PACKET_PULSE_TIME_MIN, 65);

    int16_t raw_data[PACKET_PULSE_COUNT_MAX];
    for (uint16_t i = 0; i < s_pulse_count; i++)
    {
        const int16_t duration = (int16_t)subghz_decenc_ctl.pulse_times[i];
        raw_data[i] = (i & 1u) ? (int16_t)-duration : duration;
    }

    SubGhzRawDecodeResult result;
    const uint8_t count = subghz_decode_raw_offline(
        raw_data, s_pulse_count, 433920000u, &result, 1,
        ford_v1_raw_try_decode, NULL);
    TEST_ASSERT_EQUAL_UINT8(1, count);
    TEST_ASSERT_EQUAL_UINT32(0x12345678, result.serial_number);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_ford_v0_fields_crc_and_waveform);
    RUN_TEST(test_ford_v0_rejects_truncated_waveform);
    RUN_TEST(test_ford_v1_frame_and_full_preamble_waveform);
    RUN_TEST(test_ford_v1_rejects_bad_crc);
    RUN_TEST(test_ford_v2_frame_and_waveform);
    RUN_TEST(test_ford_v2_rejects_bad_sync_and_button);
    RUN_TEST(test_ford_v2_sync_timing_jitter_is_accepted);
    RUN_TEST(test_ford_v1_short_symbols_survive_raw_decode_pipeline);
    return UNITY_END();
}
