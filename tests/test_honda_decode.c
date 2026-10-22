/* See COPYING.txt for license details. */

#include "unity.h"
#include "m1_sub_ghz_decenc.h"
#include "subghz_raw_decoder.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

bool m1_honda_static_parse_packet(const uint8_t packet[9], bool reverse,
                                  uint32_t *serial, uint32_t *counter,
                                  uint8_t *button, uint8_t *checksum);
bool m1_honda_v1_parse_frame(const uint8_t *bits, uint8_t bit_count,
                             uint64_t *key, uint32_t *serial,
                             uint16_t *counter, uint8_t *button, uint8_t *crc);
bool m1_honda_v2_parse_frame(const uint8_t raw[10], bool extra_bit,
                             uint64_t *key, uint16_t *tail,
                             uint32_t *signature, uint32_t *serial,
                             uint32_t *count, uint8_t *button, uint8_t *check);
uint8_t subghz_decode_honda_static(uint16_t protocol_index, uint16_t pulse_count);
uint8_t subghz_decode_honda_v1(uint16_t protocol_index, uint16_t pulse_count);
uint8_t subghz_decode_honda_v2(uint16_t protocol_index, uint16_t pulse_count);

static uint16_t s_pulse_count;

static bool honda_static_raw_try_decode(const uint16_t *pulse_buf,
                                        uint16_t pulse_count,
                                        SubGhzRawDecodeResult *result,
                                        void *user_ctx)
{
    (void)user_ctx;
    memcpy(subghz_decenc_ctl.pulse_times, pulse_buf,
           pulse_count * sizeof(*pulse_buf));
    if (subghz_decode_honda_static(HONDA_STATIC, pulse_count) != 0)
        return false;

    result->protocol = HONDA_STATIC;
    result->key = subghz_decenc_ctl.n64_decodedvalue;
    result->bit_len = subghz_decenc_ctl.ndecodedbitlength;
    result->te = 63;
    return true;
}

static void emit_merge(bool level, uint32_t duration)
{
    TEST_ASSERT_GREATER_THAN_UINT32(0, duration);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(UINT16_MAX, duration);
    if (s_pulse_count == 0)
    {
        TEST_ASSERT_TRUE(level);
        subghz_decenc_ctl.pulse_times[s_pulse_count++] = (uint16_t)duration;
        return;
    }

    const bool previous_level = ((s_pulse_count - 1u) & 1u) == 0u;
    if (level == previous_level)
    {
        subghz_decenc_ctl.pulse_times[s_pulse_count - 1u] += (uint16_t)duration;
    }
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

static void set_bits(uint8_t *data, uint8_t start, uint8_t count, uint32_t value)
{
    for (uint8_t i = 0; i < count; i++)
    {
        const uint8_t index = start + i;
        const uint8_t shift = ((uint8_t)~index) & 7u;
        if ((value >> (count - 1u - i)) & 1u)
            data[index >> 3] |= (uint8_t)(1u << shift);
    }
}

static void make_static_packet(uint8_t packet[9], uint32_t serial,
                               uint32_t counter, uint8_t button)
{
    memset(packet, 0, 9);
    set_bits(packet, 0, 4, button);
    set_bits(packet, 4, 28, serial);
    set_bits(packet, 32, 24, counter);
    uint8_t checksum = 0;
    for (size_t i = 0; i < 7; i++)
        checksum ^= packet[i];
    set_bits(packet, 56, 8, checksum);
}

static uint64_t static_compact_key(uint32_t serial, uint32_t counter,
                                   uint8_t button)
{
    return ((uint64_t)button << 56) | ((uint64_t)serial << 24) | counter;
}

static uint8_t reverse8(uint8_t value)
{
    value = (uint8_t)((value >> 4) | (value << 4));
    value = (uint8_t)(((value & 0xCCu) >> 2) | ((value & 0x33u) << 2));
    return (uint8_t)(((value & 0xAAu) >> 1) | ((value & 0x55u) << 1));
}

static void build_static_waveform(const uint8_t packet[8])
{
    s_pulse_count = 0;
    emit_merge(true, 700);
    for (uint16_t i = 0; i < 160; i++)
        emit_merge((i & 1u) != 0u, 63);

    for (uint8_t bit = 0; bit < 64; bit++)
    {
        const bool value = (packet[bit >> 3] >> (((uint8_t)~bit) & 7u)) & 1u;
        emit_merge(!value, 63);
        emit_merge(value, 63);
    }
    emit_merge((packet[7] & 1u) == 0u, 700);
}

void test_honda_static_packet_and_reverse_variant(void)
{
    uint8_t packet[9];
    make_static_packet(packet, 0x1234567, 0xABCDEF, 2);

    uint32_t serial = 0;
    uint32_t counter = 0;
    uint8_t button = 0;
    uint8_t checksum = 0;
    TEST_ASSERT_TRUE(m1_honda_static_parse_packet(
        packet, false, &serial, &counter, &button, &checksum));
    TEST_ASSERT_EQUAL_UINT32(0x1234567, serial);
    TEST_ASSERT_EQUAL_UINT32(0xABCDEF, counter);
    TEST_ASSERT_EQUAL_UINT8(2, button);

    uint8_t reversed[9];
    for (size_t i = 0; i < 9; i++)
        reversed[i] = reverse8(packet[i]);
    TEST_ASSERT_TRUE(m1_honda_static_parse_packet(
        reversed, true, &serial, &counter, &button, &checksum));
    TEST_ASSERT_EQUAL_UINT32(0x1234567, serial);
    TEST_ASSERT_EQUAL_UINT32(0xABCDEF, counter);

    packet[7] ^= 1u;
    TEST_ASSERT_FALSE(m1_honda_static_parse_packet(
        packet, false, &serial, &counter, &button, &checksum));
    make_static_packet(packet, 0, 0xABCDEF, 2);
    TEST_ASSERT_FALSE(m1_honda_static_parse_packet(
        packet, false, &serial, &counter, &button, &checksum));
    make_static_packet(packet, 0x1234567, 0xABCDEF, 0);
    TEST_ASSERT_FALSE(m1_honda_static_parse_packet(
        packet, false, &serial, &counter, &button, &checksum));
}

void test_honda_static_waveform_decode_and_checksum_rejection(void)
{
    uint8_t packet[9];
    make_static_packet(packet, 0x1234567, 0xABCDEF, 2);
    build_static_waveform(packet);

    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_honda_static(HONDA_STATIC, s_pulse_count));
    TEST_ASSERT_EQUAL_UINT64(static_compact_key(0x1234567, 0xABCDEF, 2),
                             subghz_decenc_ctl.n64_decodedvalue);
    TEST_ASSERT_EQUAL_UINT32(0x1234567, subghz_decenc_ctl.n32_serialnumber);
    TEST_ASSERT_EQUAL_UINT32(0xABCDEF, subghz_decenc_ctl.n32_rollingcode);
    TEST_ASSERT_EQUAL_UINT8(2, subghz_decenc_ctl.n8_buttonid);
    TEST_ASSERT_EQUAL_UINT16(64, subghz_decenc_ctl.ndecodedbitlength);

    packet[7] ^= 1u;
    build_static_waveform(packet);
    TEST_ASSERT_EQUAL_UINT8(1, subghz_decode_honda_static(HONDA_STATIC, s_pulse_count));
}

void test_honda_static_short_symbols_survive_raw_decode_pipeline(void)
{
    uint8_t packet[9];
    make_static_packet(packet, 0x1234567, 0xABCDEF, 2);
    build_static_waveform(packet);

    TEST_ASSERT_GREATER_THAN_UINT16(PACKET_PULSE_TIME_MIN, 63);

    int16_t raw_data[PACKET_PULSE_COUNT_MAX];
    for (uint16_t i = 0; i < s_pulse_count; i++)
    {
        const int16_t duration = (int16_t)subghz_decenc_ctl.pulse_times[i];
        raw_data[i] = (i & 1u) ? (int16_t)-duration : duration;
    }

    SubGhzRawDecodeResult result;
    const uint8_t count = subghz_decode_raw_offline(
        raw_data, s_pulse_count, 433920000u, &result, 1,
        honda_static_raw_try_decode, NULL);
    TEST_ASSERT_EQUAL_UINT8(1, count);
    TEST_ASSERT_EQUAL_UINT64(static_compact_key(0x1234567, 0xABCDEF, 2),
                             result.key);
}

static void make_v1_bits(uint8_t bits[12], uint64_t key, uint8_t crc)
{
    memset(bits, 0, 12);
    for (uint8_t i = 0; i < 64; i++)
    {
        if ((key >> (63u - i)) & 1u)
        {
            const uint8_t pos = i + 1u;
            bits[pos >> 3] |= (uint8_t)(1u << (7u - (pos & 7u)));
        }
    }
    for (uint8_t i = 0; i < 4; i++)
    {
        if ((crc >> (3u - i)) & 1u)
        {
            const uint8_t pos = 65u + i;
            bits[pos >> 3] |= (uint8_t)(1u << (7u - (pos & 7u)));
        }
    }
}

typedef struct {
    uint16_t count;
    bool levels[160];
    uint16_t durations[160];
} v1_generated_t;

static void v1_generated_emit(v1_generated_t *generated, bool level)
{
    if (generated->count > 0 && generated->levels[generated->count - 1u] == level)
    {
        generated->durations[generated->count - 1u] += 1000;
        return;
    }
    generated->levels[generated->count] = level;
    generated->durations[generated->count] = 1000;
    generated->count++;
}

static void build_v1_waveform(uint64_t key, uint8_t crc)
{
    uint8_t frame[9];
    for (size_t i = 0; i < 8; i++)
        frame[i] = (uint8_t)(key >> (56u - i * 8u));
    frame[8] = crc << 4;

    s_pulse_count = 0;
    for (uint8_t i = 0; i < 89; i++)
    {
        emit_merge(true, 1000);
        emit_merge(false, 1000);
    }
    emit_merge(true, 1000);
    emit_merge(false, 5000);

    v1_generated_t generated;
    memset(&generated, 0, sizeof(generated));
    for (uint8_t bit_index = 0; bit_index < 80; bit_index++)
    {
        bool bit;
        if (bit_index >= 12)
        {
            const uint8_t data_index = (bit_index - 12u) >> 3;
            const uint8_t shift = (11u - bit_index) & 7u;
            bit = (frame[data_index] >> shift) & 1u;
        }
        else
        {
            bit = ((uint8_t)~bit_index) & 1u;
        }
        v1_generated_emit(&generated, bit);
        v1_generated_emit(&generated, !bit);
    }

    for (uint16_t i = 2; i < generated.count; i++)
        emit_merge(generated.levels[i], generated.durations[i]);

    const bool tail_level = !generated.levels[generated.count - 1u];
    emit_merge(tail_level, 1000);
    if (!tail_level)
        emit_merge(true, 1000);
    emit_merge(false, 5000);
}

void test_honda_v1_frame_alignment_and_button_validation(void)
{
    const uint64_t key = 0x123456789ABCDEF0ULL;
    uint8_t bits[12];
    make_v1_bits(bits, key, 5);

    uint64_t decoded_key = 0;
    uint32_t serial = 0;
    uint16_t counter = 0;
    uint8_t button = 0;
    uint8_t crc = 0;
    TEST_ASSERT_TRUE(m1_honda_v1_parse_frame(
        bits, 69, &decoded_key, &serial, &counter, &button, &crc));
    TEST_ASSERT_EQUAL_UINT64(key, decoded_key);
    TEST_ASSERT_EQUAL_UINT32((key >> 36) & 0x0FFFFFFFu, serial);
    TEST_ASSERT_EQUAL_UINT16(0xDEF0, counter);
    TEST_ASSERT_EQUAL_UINT8(9, button);
    TEST_ASSERT_EQUAL_UINT8(5, crc);

    make_v1_bits(bits, 0x123456785ABCDEF0ULL, 5);
    TEST_ASSERT_FALSE(m1_honda_v1_parse_frame(
        bits, 69, &decoded_key, &serial, &counter, &button, &crc));
    TEST_ASSERT_FALSE(m1_honda_v1_parse_frame(
        bits, 67, &decoded_key, &serial, &counter, &button, &crc));
}

void test_honda_v1_waveform_decode_and_button_rejection(void)
{
    const uint64_t key = 0x123456789ABCDEF0ULL;
    build_v1_waveform(key, 5);

    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_honda_v1(HONDA_V1, s_pulse_count));
    TEST_ASSERT_EQUAL_UINT64(key, subghz_decenc_ctl.n64_decodedvalue);
    TEST_ASSERT_EQUAL_UINT32((key >> 36) & 0x0FFFFFFFu,
                             subghz_decenc_ctl.n32_serialnumber);
    TEST_ASSERT_EQUAL_UINT32(0xDEF0, subghz_decenc_ctl.n32_rollingcode);
    TEST_ASSERT_EQUAL_UINT8(9, subghz_decenc_ctl.n8_buttonid);
    TEST_ASSERT_EQUAL_UINT8(5, subghz_decenc_ctl.n32_key2);
    TEST_ASSERT_EQUAL_UINT16(68, subghz_decenc_ctl.ndecodedbitlength);

    build_v1_waveform(0x123456785ABCDEF0ULL, 5);
    TEST_ASSERT_EQUAL_UINT8(1, subghz_decode_honda_v1(HONDA_V1, s_pulse_count));
}

static uint8_t v2_check(uint32_t count)
{
    const uint8_t c0 = ((count >> 1) ^ (count >> 2) ^ (count >> 3) ^
                        (count >> 4) ^ (count >> 6)) & 1u;
    const uint8_t c1 = ((count >> 0) ^ (count >> 2) ^ (count >> 3) ^
                        (count >> 4) ^ (count >> 5) ^ (count >> 6) ^ 1u) & 1u;
    const uint8_t c2 = ((count >> 1) ^ (count >> 3) ^ (count >> 4) ^
                        (count >> 5) ^ (count >> 6)) & 1u;
    return c0 | (c1 << 1) | (c2 << 2);
}

static uint16_t v2_tail(uint32_t count)
{
    return (((count >> 0) ^ (count >> 2) ^ (count >> 4) ^ (count >> 5)) & 1u)
               ? 0xFFFFu
               : 0x7FFFu;
}

static uint64_t v2_key(uint32_t signature, uint32_t serial, uint32_t count)
{
    uint8_t bytes[8] = {0};
    bytes[0] = signature >> 16;
    bytes[1] = signature >> 8;
    bytes[2] = signature;
    bytes[3] = serial >> 16;
    bytes[4] = serial >> 8;
    bytes[5] = serial;
    bytes[6] = count >> 1;
    bytes[7] = ((count & 1u) ? 0x80u : 0u) | v2_check(count);

    uint64_t key = 0;
    for (size_t i = 0; i < 8; i++)
        key = (key << 8) | bytes[i];
    return key;
}

static void v2_encode_bit(bool *previous_bit, bool bit)
{
    if (!*previous_bit && !bit)
    {
        emit_merge(true, 250);
        emit_merge(false, 250);
    }
    else if (!*previous_bit && bit)
    {
        emit_merge(true, 500);
    }
    else if (*previous_bit && !bit)
    {
        emit_merge(false, 500);
    }
    else
    {
        emit_merge(false, 250);
        emit_merge(true, 250);
    }
    *previous_bit = bit;
}

static void build_v2_waveform(uint64_t key, uint16_t tail, bool extra_bit)
{
    s_pulse_count = 0;
    for (uint16_t i = 0; i < 319; i++)
    {
        emit_merge(true, 250);
        emit_merge(false, 250);
    }
    emit_merge(true, 750);
    emit_merge(false, 750);
    emit_merge(true, 250);

    bool previous_bit = true;
    v2_encode_bit(&previous_bit, false);
    for (uint8_t bit_index = 2; bit_index < 64; bit_index++)
        v2_encode_bit(&previous_bit, (key >> (63u - bit_index)) & 1u);
    for (uint8_t bit_index = 0; bit_index < 16; bit_index++)
        v2_encode_bit(&previous_bit, (tail >> (15u - bit_index)) & 1u);
    v2_encode_bit(&previous_bit, extra_bit);
    emit_merge(false, 50000);
}

static void v2_raw(uint8_t raw[10], uint64_t key, uint16_t tail)
{
    for (size_t i = 0; i < 8; i++)
        raw[i] = (uint8_t)(key >> (56u - i * 8u));
    raw[8] = tail >> 8;
    raw[9] = tail;
}

void test_honda_v2_frame_validation(void)
{
    const uint64_t key = v2_key(0xC20363, 0x123456, 0x125);
    uint8_t raw[10];
    v2_raw(raw, key, v2_tail(0x125));

    uint64_t decoded_key = 0;
    uint16_t tail = 0;
    uint32_t signature = 0;
    uint32_t serial = 0;
    uint32_t count = 0;
    uint8_t button = 0;
    uint8_t check = 0;
    TEST_ASSERT_TRUE(m1_honda_v2_parse_frame(
        raw, true, &decoded_key, &tail, &signature, &serial, &count, &button, &check));
    TEST_ASSERT_EQUAL_UINT64(key, decoded_key);
    TEST_ASSERT_EQUAL_UINT16(v2_tail(0x125), tail);
    TEST_ASSERT_EQUAL_UINT32(0xC20363, signature);
    TEST_ASSERT_EQUAL_UINT32(0x123456, serial);
    TEST_ASSERT_EQUAL_UINT32(0x125, count);
    TEST_ASSERT_EQUAL_UINT8(2, button);

    TEST_ASSERT_FALSE(m1_honda_v2_parse_frame(
        raw, false, &decoded_key, &tail, &signature, &serial, &count, &button, &check));
    raw[7] ^= 1u;
    TEST_ASSERT_FALSE(m1_honda_v2_parse_frame(
        raw, true, &decoded_key, &tail, &signature, &serial, &count, &button, &check));
    raw[7] ^= 1u;
    raw[9] ^= 1u;
    TEST_ASSERT_FALSE(m1_honda_v2_parse_frame(
        raw, true, &decoded_key, &tail, &signature, &serial, &count, &button, &check));
}

void test_honda_v2_waveform_decode_and_check_rejection(void)
{
    const uint32_t count = 0x125;
    const uint64_t key = v2_key(0xC20363, 0x123456, count);
    build_v2_waveform(key, v2_tail(count), true);

    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_honda_v2(HONDA_V2, s_pulse_count));
    TEST_ASSERT_EQUAL_UINT64(key, subghz_decenc_ctl.n64_decodedvalue);
    TEST_ASSERT_EQUAL_UINT32(0x123456, subghz_decenc_ctl.n32_serialnumber);
    TEST_ASSERT_EQUAL_UINT32(count, subghz_decenc_ctl.n32_rollingcode);
    TEST_ASSERT_EQUAL_UINT8(2, subghz_decenc_ctl.n8_buttonid);
    TEST_ASSERT_EQUAL_UINT16(81, subghz_decenc_ctl.ndecodedbitlength);

    const uint64_t unlock_key = v2_key(0xA285E3, 0x654321, count);
    build_v2_waveform(unlock_key, v2_tail(count), true);
    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_honda_v2(HONDA_V2, s_pulse_count));
    TEST_ASSERT_EQUAL_UINT64(unlock_key, subghz_decenc_ctl.n64_decodedvalue);
    TEST_ASSERT_EQUAL_UINT32(0x654321, subghz_decenc_ctl.n32_serialnumber);
    TEST_ASSERT_EQUAL_UINT8(4, subghz_decenc_ctl.n8_buttonid);

    build_v2_waveform(key ^ 1u, v2_tail(count), true);
    TEST_ASSERT_EQUAL_UINT8(1, subghz_decode_honda_v2(HONDA_V2, s_pulse_count));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_honda_static_packet_and_reverse_variant);
    RUN_TEST(test_honda_static_waveform_decode_and_checksum_rejection);
    RUN_TEST(test_honda_static_short_symbols_survive_raw_decode_pipeline);
    RUN_TEST(test_honda_v1_frame_alignment_and_button_validation);
    RUN_TEST(test_honda_v1_waveform_decode_and_button_rejection);
    RUN_TEST(test_honda_v2_frame_validation);
    RUN_TEST(test_honda_v2_waveform_decode_and_check_rejection);
    return UNITY_END();
}
