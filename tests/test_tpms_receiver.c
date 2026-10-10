#include "unity.h"
#include "tpms_receiver.h"
#include "m1_sub_ghz_decenc.h"

#include <string.h>

extern SubGHz_protocol_t *subghz_protocols_list_ptr;

void setUp(void) {}
void tearDown(void) {}

static tpms_sensor_t sensor(uint16_t protocol, uint32_t serial,
                            uint64_t data, uint32_t seen)
{
    tpms_sensor_t value;
    memset(&value, 0, sizeof(value));
    value.protocol = protocol;
    value.serial = serial;
    value.data = data;
    value.last_seen_ms = seen;
    value.receptions = 1U;
    return value;
}

void test_history_refresh_moves_sensor_to_front_and_preserves_count(void)
{
    tpms_history_t history;
    tpms_history_reset(&history);
    tpms_sensor_t first = sensor(3U, 0x123456U, 0xAAU, 100U);
    tpms_sensor_t other = sensor(3U, 0xABCDEFU, 0xCCU, 150U);
    tpms_sensor_t next = sensor(3U, 0x123456U, 0xBBU, 200U);

    TEST_ASSERT_EQUAL_INT(0, tpms_history_add(&history, &first));
    TEST_ASSERT_EQUAL_INT(0, tpms_history_add(&history, &other));
    TEST_ASSERT_EQUAL_INT(0, tpms_history_add(&history, &next));
    TEST_ASSERT_EQUAL_UINT8(2U, history.count);
    TEST_ASSERT_EQUAL_UINT64(0xBBU, history.entries[0].data);
    TEST_ASSERT_EQUAL_UINT16(2U, history.entries[0].receptions);
    TEST_ASSERT_EQUAL_UINT32(200U, history.entries[0].last_seen_ms);
    TEST_ASSERT_EQUAL_UINT32(0xABCDEFU, history.entries[1].serial);
}

void test_history_deduplicates_without_serial_by_raw_data(void)
{
    tpms_history_t history;
    tpms_history_reset(&history);
    tpms_sensor_t first = sensor(4U, 0U, 0x1234U, 100U);
    tpms_sensor_t same = sensor(4U, 0U, 0x1234U, 200U);
    tpms_sensor_t other = sensor(4U, 0U, 0x5678U, 300U);

    TEST_ASSERT_EQUAL_INT(0, tpms_history_add(&history, &first));
    TEST_ASSERT_EQUAL_INT(0, tpms_history_add(&history, &same));
    TEST_ASSERT_EQUAL_INT(0, tpms_history_add(&history, &other));
    TEST_ASSERT_EQUAL_UINT8(2U, history.count);
    TEST_ASSERT_EQUAL_UINT64(0x5678U, tpms_history_get(&history, 0U)->data);
    TEST_ASSERT_EQUAL_UINT16(2U, tpms_history_get(&history, 1U)->receptions);
}

void test_history_evicts_oldest_when_full(void)
{
    tpms_history_t history;
    tpms_history_reset(&history);
    for (uint8_t i = 0; i < TPMS_HISTORY_CAPACITY; ++i)
    {
        tpms_sensor_t value = sensor(1U, i + 1U, i + 1U, i);
        TEST_ASSERT_EQUAL_INT(0, tpms_history_add(&history, &value));
    }

    tpms_sensor_t refreshed_oldest = sensor(1U, 1U, 101U, 99U);
    TEST_ASSERT_EQUAL_INT(0, tpms_history_add(&history, &refreshed_oldest));
    TEST_ASSERT_EQUAL_UINT32(1U, tpms_history_get(&history, 0U)->serial);
    TEST_ASSERT_EQUAL_UINT16(2U, tpms_history_get(&history, 0U)->receptions);

    tpms_sensor_t newest = sensor(1U, 99U, 99U, 100U);
    TEST_ASSERT_EQUAL_INT(0, tpms_history_add(&history, &newest));
    TEST_ASSERT_EQUAL_UINT8(TPMS_HISTORY_CAPACITY, history.count);
    TEST_ASSERT_EQUAL_UINT32(99U, tpms_history_get(&history, 0U)->serial);
    TEST_ASSERT_EQUAL_UINT32(1U, tpms_history_get(&history, 1U)->serial);
    for (uint8_t i = 0; i < history.count; ++i)
        TEST_ASSERT_NOT_EQUAL(2U, tpms_history_get(&history, i)->serial);
}

void test_history_selection_tracks_identity_and_falls_back_if_evicted(void)
{
    tpms_history_t history;
    tpms_history_reset(&history);
    tpms_sensor_t selected = sensor(1U, 1U, 1U, 1U);
    TEST_ASSERT_EQUAL_INT(0, tpms_history_add(&history, &selected));
    for (uint32_t id = 2U; id <= 3U; ++id)
    {
        tpms_sensor_t value = sensor(1U, id, id, id);
        TEST_ASSERT_EQUAL_INT(0, tpms_history_add(&history, &value));
    }
    TEST_ASSERT_EQUAL_UINT8(2U,
        tpms_history_selection_index(&history, &selected));

    for (uint32_t id = 4U; id <= TPMS_HISTORY_CAPACITY + 1U; ++id)
    {
        tpms_sensor_t value = sensor(1U, id, id, id);
        TEST_ASSERT_EQUAL_INT(0, tpms_history_add(&history, &value));
    }
    TEST_ASSERT_EQUAL_UINT8(0U,
        tpms_history_selection_index(&history, &selected));
}

void test_age_saturates_and_handles_tick_wrap(void)
{
    tpms_sensor_t value = sensor(1U, 1U, 1U, UINT32_MAX - 30000U);
    TEST_ASSERT_EQUAL_UINT8(1U, tpms_sensor_age_min(&value, 30000U));
    value.last_seen_ms = 0U;
    TEST_ASSERT_EQUAL_UINT8(99U, tpms_sensor_age_min(&value, 7000000U));
}

void test_history_rejects_null_and_checks_indices(void)
{
    tpms_history_t history;
    tpms_sensor_t value = sensor(1U, 1U, 1U, 0U);
    tpms_history_reset(&history);
    TEST_ASSERT_EQUAL_INT(-1, tpms_history_add(NULL, &value));
    TEST_ASSERT_EQUAL_INT(-1, tpms_history_add(&history, NULL));
    TEST_ASSERT_NULL(tpms_history_get(NULL, 0U));
    TEST_ASSERT_NULL(tpms_history_get(&history, 0U));
}

void test_schrader_gg4_parses_momentum_example(void)
{
    tpms_telemetry_t telemetry;

    TEST_ASSERT_TRUE(tpms_schrader_gg4_parse(
        UINT64_C(0x3000878456094cd0), 64U, &telemetry));
    TEST_ASSERT_TRUE(telemetry.valid);
    TEST_ASSERT_EQUAL_HEX32(0x00878456U, telemetry.serial);
    TEST_ASSERT_EQUAL_UINT16(155U, telemetry.pressure_hundredths_bar);
    TEST_ASSERT_EQUAL_INT16(26, telemetry.temperature_c);
}

void test_schrader_gg4_converts_other_sample_and_temperature_offset(void)
{
    tpms_telemetry_t telemetry;

    TEST_ASSERT_TRUE(tpms_schrader_gg4_parse(
        UINT64_C(0x3000878456084ecb), 64U, &telemetry));
    TEST_ASSERT_EQUAL_HEX32(0x00878456U, telemetry.serial);
    TEST_ASSERT_EQUAL_UINT16(138U, telemetry.pressure_hundredths_bar);
    TEST_ASSERT_EQUAL_INT16(28, telemetry.temperature_c);
}

void test_schrader_gg4_rejects_bad_crc_and_wrong_length(void)
{
    tpms_telemetry_t telemetry = {
        .valid = true,
        .serial = 1U,
        .pressure_hundredths_bar = 1U,
        .temperature_c = 1,
    };

    TEST_ASSERT_FALSE(tpms_schrader_gg4_parse(
        UINT64_C(0x3000878456094cd1), 64U, &telemetry));
    TEST_ASSERT_FALSE(telemetry.valid);
    TEST_ASSERT_EQUAL_UINT32(0U, telemetry.serial);
    TEST_ASSERT_FALSE(tpms_schrader_gg4_parse(
        UINT64_C(0x3000878456094cd0), 40U, &telemetry));
    TEST_ASSERT_FALSE(tpms_schrader_gg4_parse(
        UINT64_C(0x3000878456094cd0), 64U, NULL));
}

static uint16_t build_schrader_pulses(uint64_t data)
{
    uint16_t pulse_count = 0U;
    uint8_t last_level = 1U;
    for (int8_t bit = 63; bit >= 0; --bit)
    {
        uint8_t value = (uint8_t)((data >> bit) & 1U);
        if (value == last_level)
        {
            subghz_decenc_ctl.pulse_times[pulse_count++] = 120U;
            subghz_decenc_ctl.pulse_times[pulse_count++] = 120U;
        }
        else
        {
            subghz_decenc_ctl.pulse_times[pulse_count++] = 240U;
            last_level ^= 1U;
        }
    }
    return pulse_count;
}

void test_schrader_decoder_rejects_bad_crc_in_64_bit_legacy_fallback(void)
{
    subghz_protocols_list_ptr[0] = (SubGHz_protocol_t) {
        .te_short = 120U,
        .te_long = 240U,
        .te_tolerance = 25U,
        .preamble_bits = 0U,
        .data_bits = 64U,
    };
    memset(&subghz_decenc_ctl, 0, sizeof(subghz_decenc_ctl));
    uint16_t pulse_count =
        build_schrader_pulses(UINT64_C(0x3000878456094cd1));

    TEST_ASSERT_EQUAL_UINT8(1U, subghz_decode_schrader(0U, pulse_count));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_history_refresh_moves_sensor_to_front_and_preserves_count);
    RUN_TEST(test_history_deduplicates_without_serial_by_raw_data);
    RUN_TEST(test_history_evicts_oldest_when_full);
    RUN_TEST(test_history_selection_tracks_identity_and_falls_back_if_evicted);
    RUN_TEST(test_age_saturates_and_handles_tick_wrap);
    RUN_TEST(test_history_rejects_null_and_checks_indices);
    RUN_TEST(test_schrader_gg4_parses_momentum_example);
    RUN_TEST(test_schrader_gg4_converts_other_sample_and_temperature_offset);
    RUN_TEST(test_schrader_gg4_rejects_bad_crc_and_wrong_length);
    RUN_TEST(test_schrader_decoder_rejects_bad_crc_in_64_bit_legacy_fallback);
    return UNITY_END();
}
