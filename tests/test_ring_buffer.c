/* See COPYING.txt for license details. */

#include "unity.h"
#include "m1_ring_buffer.h"

#include <string.h>

void setUp(void) { }
void tearDown(void) { }

void test_peek_preserves_tail_and_wrapped_multibyte_samples(void)
{
    uint16_t storage[8];
    uint16_t initial[6] = { 10, 11, 12, 13, 14, 15 };
    uint16_t wrapped[4] = { 20, 21, 22, 23 };
    uint16_t expected[6] = { 14, 15, 20, 21, 22, 23 };
    uint16_t actual[6] = { 0 };
    uint16_t consumed[4];
    S_M1_RingBuffer ring;

    m1_ringbuffer_init(&ring, (uint8_t *)storage, 8, sizeof(uint16_t));
    TEST_ASSERT_EQUAL_UINT16(6, m1_ringbuffer_write(&ring, (uint8_t *)initial, 6));
    TEST_ASSERT_EQUAL_UINT16(4, m1_ringbuffer_read(&ring, (uint8_t *)consumed, 4));
    TEST_ASSERT_EQUAL_UINT16(4, m1_ringbuffer_write(&ring, (uint8_t *)wrapped, 4));
    uint32_t tail = ring.tail;

    TEST_ASSERT_EQUAL_UINT16(6, m1_ringbuffer_peek(&ring, (uint8_t *)actual, 6));
    TEST_ASSERT_EQUAL_UINT32(tail, ring.tail);
    TEST_ASSERT_EQUAL_UINT16_ARRAY(expected, actual, 6);

    memset(actual, 0, sizeof(actual));
    TEST_ASSERT_EQUAL_UINT16(6, m1_ringbuffer_read(&ring, (uint8_t *)actual, 6));
    TEST_ASSERT_EQUAL_UINT16_ARRAY(expected, actual, 6);
}

void test_peek_limits_short_and_empty_reads(void)
{
    uint16_t storage[4] = { 30, 31, 32, 33 };
    uint16_t actual[4] = { 0 };
    S_M1_RingBuffer ring;

    m1_ringbuffer_init(&ring, (uint8_t *)storage, 4, sizeof(uint16_t));
    TEST_ASSERT_EQUAL_UINT16(0, m1_ringbuffer_peek(&ring, (uint8_t *)actual, 4));
    ring.head = 3U * sizeof(uint16_t);
    TEST_ASSERT_EQUAL_UINT16(2, m1_ringbuffer_peek(&ring, (uint8_t *)actual, 2));
    TEST_ASSERT_EQUAL_UINT16(30, actual[0]);
    TEST_ASSERT_EQUAL_UINT16(31, actual[1]);
    TEST_ASSERT_EQUAL_UINT32(0, ring.tail);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_peek_preserves_tail_and_wrapped_multibyte_samples);
    RUN_TEST(test_peek_limits_short_and_empty_reads);
    return UNITY_END();
}
