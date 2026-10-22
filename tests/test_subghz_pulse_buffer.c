/* See COPYING.txt for license details. */

#include "unity.h"
#include "subghz_pulse_buffer.h"
#include "m1_sub_ghz_decenc.h"

#include <stdint.h>

void setUp(void) {}
void tearDown(void) {}

void test_pulse_buffer_rejects_append_at_capacity(void)
{
    uint16_t pulse_times[PACKET_PULSE_COUNT_MAX] = {0};
    uint16_t pulse_count = 0;

    for (uint16_t i = 0; i < PACKET_PULSE_COUNT_MAX; i++)
        TEST_ASSERT_TRUE(subghz_pulse_buffer_append(
            pulse_times, &pulse_count, PACKET_PULSE_COUNT_MAX, (uint16_t)(i + 1u)));

    TEST_ASSERT_EQUAL_UINT16(PACKET_PULSE_COUNT_MAX, pulse_count);
    TEST_ASSERT_FALSE(subghz_pulse_buffer_append(
        pulse_times, &pulse_count, PACKET_PULSE_COUNT_MAX, 0xFFFFu));
    TEST_ASSERT_EQUAL_UINT16(PACKET_PULSE_COUNT_MAX, pulse_count);
    TEST_ASSERT_EQUAL_UINT16(PACKET_PULSE_COUNT_MAX, pulse_times[pulse_count - 1u]);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_pulse_buffer_rejects_append_at_capacity);
    return UNITY_END();
}
