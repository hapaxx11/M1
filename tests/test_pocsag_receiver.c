#include "unity.h"
#include "pocsag_receiver.h"

#include <string.h>

#define TEST_SYNC_WORD 0x7CD215D8UL
#define TEST_IDLE_WORD 0x7A89C197UL

void setUp(void) { }
void tearDown(void) { }

static uint8_t reverse_nibble_test(uint8_t value)
{
    value = (uint8_t)(((value & 0x5U) << 1) | ((value & 0xAU) >> 1));
    return (uint8_t)(((value & 0x3U) << 2) | ((value & 0xCU) >> 2));
}

static uint32_t make_address_word(uint32_t ric, uint8_t function)
{
    return ((ric >> 3) << 13) | ((uint32_t)(function & 3U) << 11);
}

static uint32_t make_numeric_word(const char *digits)
{
    uint32_t payload = 0;
    for (uint8_t i = 0; i < 5U; ++i)
    {
        uint8_t digit = (uint8_t)(digits[i] - '0');
        payload |= (uint32_t)reverse_nibble_test(digit) << (16U - (uint32_t)i * 4U);
    }
    return 0x80000000UL | (payload << 11);
}

static uint32_t make_alphanumeric_word(const char *text, uint8_t offset)
{
    uint32_t payload = 0;
    size_t text_length = strlen(text);
    for (uint8_t i = 0; i < 20U; ++i)
    {
        uint8_t absolute_bit = (uint8_t)(offset + i);
        uint8_t character = (uint8_t)(absolute_bit / 7U);
        uint8_t bit = (uint8_t)(6U - (absolute_bit % 7U));
        if (character < text_length && ((text[character] >> bit) & 1U))
            payload |= 1UL << (19U - i);
    }
    return 0x80000000UL | (payload << 11);
}

static void append_word(uint8_t *bits, uint32_t *bit_count, uint32_t word)
{
    for (int8_t i = 31; i >= 0; --i)
        bits[(*bit_count)++] = (uint8_t)((word >> (uint8_t)i) & 1U);
}

static bool feed_stream(pocsag_receiver_t *receiver, const uint8_t *bits,
                        uint32_t bit_count, uint16_t period)
{
    uint32_t run = 1U;
    for (uint32_t i = 1; i <= bit_count; ++i)
    {
        if (i < bit_count && bits[i] == bits[i - 1U])
        {
            run++;
            continue;
        }
        if (pocsag_receiver_feed(receiver, (uint16_t)(period * run)))
            return true;
        run = 1U;
    }
    return false;
}

static bool feed_numeric_frame(pocsag_receiver_t *receiver, uint32_t address,
                               uint32_t message, uint16_t period)
{
    uint8_t bits[24U + 32U + 16U * 32U];
    uint32_t count = 0;
    for (uint8_t i = 0; i < 24U; ++i)
        bits[count++] = (uint8_t)((i & 1U) == 0U);
    append_word(bits, &count, TEST_SYNC_WORD);

    uint32_t words[16] = {
        TEST_IDLE_WORD, TEST_IDLE_WORD, TEST_IDLE_WORD, TEST_IDLE_WORD,
        TEST_IDLE_WORD, TEST_IDLE_WORD, TEST_IDLE_WORD, TEST_IDLE_WORD,
        TEST_IDLE_WORD, TEST_IDLE_WORD, address, message,
        TEST_IDLE_WORD, TEST_IDLE_WORD, TEST_IDLE_WORD, TEST_IDLE_WORD
    };

    for (uint8_t i = 0; i < 16U; ++i)
        append_word(bits, &count, words[i]);
    return feed_stream(receiver, bits, count, period);
}

static void test_decodes_numeric_at_all_supported_baud_rates(void)
{
    const uint16_t periods[] = {1950U, 833U, 417U};
    const uint16_t baud[] = {512U, 1200U, 2400U};

    for (uint8_t i = 0; i < 3U; ++i)
    {
        pocsag_receiver_t receiver;
        pocsag_message_t message;
        pocsag_receiver_reset(&receiver);
        TEST_ASSERT_TRUE(feed_numeric_frame(&receiver,
                                            make_address_word(0x12345U, 0U),
                                            make_numeric_word("12345"),
                                            periods[i]));
        TEST_ASSERT_TRUE(pocsag_receiver_take_message(&receiver, &message));
        TEST_ASSERT_EQUAL_UINT32(0x12345U, message.ric);
        TEST_ASSERT_EQUAL_UINT16(baud[i], message.baud);
        TEST_ASSERT_EQUAL_UINT8(0U, message.function);
        TEST_ASSERT_EQUAL_STRING("12345", message.text);
    }
}

static void test_decodes_alphanumeric_message(void)
{
    pocsag_receiver_t receiver;
    pocsag_message_t message;
    pocsag_receiver_reset(&receiver);

    uint8_t bits[24U + 32U + 16U * 32U];
    uint32_t count = 0;
    for (uint8_t i = 0; i < 24U; ++i)
        bits[count++] = (uint8_t)((i & 1U) == 0U);
    append_word(bits, &count, TEST_SYNC_WORD);
    uint32_t words[16] = {
        TEST_IDLE_WORD, TEST_IDLE_WORD, TEST_IDLE_WORD, TEST_IDLE_WORD,
        TEST_IDLE_WORD, TEST_IDLE_WORD, TEST_IDLE_WORD, TEST_IDLE_WORD,
        TEST_IDLE_WORD, TEST_IDLE_WORD, make_address_word(0x12340U, 3U),
        make_alphanumeric_word("OK", 0U),
        make_alphanumeric_word("OK", 20U),
        TEST_IDLE_WORD, TEST_IDLE_WORD, TEST_IDLE_WORD
    };
    for (uint8_t i = 0; i < 16U; ++i)
        append_word(bits, &count, words[i]);
    TEST_ASSERT_TRUE(feed_stream(&receiver, bits, count, 833U));

    TEST_ASSERT_TRUE(pocsag_receiver_take_message(&receiver, &message));
    TEST_ASSERT_EQUAL_UINT32(0x12345U, message.ric);
    TEST_ASSERT_EQUAL_UINT8(3U, message.function);
    TEST_ASSERT_EQUAL_STRING("OK", message.text);
}

static void test_history_deduplicates_and_evicts_oldest(void)
{
    pocsag_history_t history;
    pocsag_message_t message = {
        .ric = 123U,
        .baud = 1200U,
        .function = 3U,
        .text = "test"
    };
    pocsag_history_reset(&history);

    TEST_ASSERT_TRUE(pocsag_history_add(&history, &message, 439987500U));
    TEST_ASSERT_FALSE(pocsag_history_add(&history, &message, 439987500U));
    TEST_ASSERT_EQUAL_UINT8(1U, history.count);
    TEST_ASSERT_EQUAL_UINT8(2U, pocsag_history_get(&history, 0)->repeat_count);

    for (uint32_t i = 0; i < POCSAG_HISTORY_CAPACITY; ++i)
    {
        message.ric = 200U + i;
        TEST_ASSERT_TRUE(pocsag_history_add(&history, &message, 433920000U));
    }

    TEST_ASSERT_EQUAL_UINT8(POCSAG_HISTORY_CAPACITY, history.count);
    TEST_ASSERT_EQUAL_UINT32(200U + POCSAG_HISTORY_CAPACITY - 1U,
                             pocsag_history_get(&history, 0)->ric);
    TEST_ASSERT_NULL(pocsag_history_get(&history, POCSAG_HISTORY_CAPACITY));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_decodes_numeric_at_all_supported_baud_rates);
    RUN_TEST(test_decodes_alphanumeric_message);
    RUN_TEST(test_history_deduplicates_and_evicts_oldest);
    return UNITY_END();
}
