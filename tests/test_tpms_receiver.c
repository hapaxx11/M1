#include "unity.h"
#include "tpms_receiver.h"

#include <string.h>

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

void test_history_reception_updates_same_serial(void)
{
    tpms_history_t history;
    tpms_history_reset(&history);
    tpms_sensor_t first = sensor(3U, 0x123456U, 0xAAU, 100U);
    tpms_sensor_t next = sensor(3U, 0x123456U, 0xBBU, 200U);

    TEST_ASSERT_EQUAL_INT(0, tpms_history_add(&history, &first));
    TEST_ASSERT_EQUAL_INT(0, tpms_history_add(&history, &next));
    TEST_ASSERT_EQUAL_UINT8(1U, history.count);
    TEST_ASSERT_EQUAL_UINT64(0xBBU, history.entries[0].data);
    TEST_ASSERT_EQUAL_UINT16(2U, history.entries[0].receptions);
    TEST_ASSERT_EQUAL_UINT32(200U, history.entries[0].last_seen_ms);
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

    tpms_sensor_t newest = sensor(1U, 99U, 99U, 99U);
    TEST_ASSERT_EQUAL_INT(0, tpms_history_add(&history, &newest));
    TEST_ASSERT_EQUAL_UINT8(TPMS_HISTORY_CAPACITY, history.count);
    TEST_ASSERT_EQUAL_UINT32(99U, tpms_history_get(&history, 0U)->serial);
    for (uint8_t i = 0; i < history.count; ++i)
        TEST_ASSERT_NOT_EQUAL(1U, tpms_history_get(&history, i)->serial);
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

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_history_reception_updates_same_serial);
    RUN_TEST(test_history_deduplicates_without_serial_by_raw_data);
    RUN_TEST(test_history_evicts_oldest_when_full);
    RUN_TEST(test_age_saturates_and_handles_tick_wrap);
    RUN_TEST(test_history_rejects_null_and_checks_indices);
    return UNITY_END();
}
