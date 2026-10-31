#include "unity.h"
#include "flex_receiver.h"

#include <string.h>

#define TEST_CAPTURE_CAPACITY 8U
#define TEST_PHASE_WORDS 88U

static pager_message_t captured[TEST_CAPTURE_CAPACITY];
static uint8_t captured_count;

void setUp(void)
{
    memset(captured, 0, sizeof(captured));
    captured_count = 0;
}

void tearDown(void) { }

static void capture_message(void *context, const pager_message_t *message)
{
    (void)context;
    if (captured_count < TEST_CAPTURE_CAPACITY)
        captured[captured_count++] = *message;
}

static uint32_t reverse_bits(uint32_t value)
{
    value = ((value >> 1) & 0x55555555UL) | ((value & 0x55555555UL) << 1);
    value = ((value >> 2) & 0x33333333UL) | ((value & 0x33333333UL) << 2);
    value = ((value >> 4) & 0x0F0F0F0FUL) | ((value & 0x0F0F0F0FUL) << 4);
    value = ((value >> 8) & 0x00FF00FFUL) | ((value & 0x00FF00FFUL) << 8);
    return (value >> 16) | (value << 16);
}

static uint32_t encode_word(uint32_t data21)
{
    const uint32_t reversed = reverse_bits(data21) >> 11;
    uint32_t remainder = reversed << 10;
    for (int8_t bit = 30; bit >= 10; --bit)
    {
        if ((remainder & (1UL << bit)) != 0U)
            remainder ^= 0x769UL << (bit - 10);
    }
    uint32_t codeword = ((reversed << 10) | (remainder & 0x3FFUL)) << 1;
    if (__builtin_popcount(codeword) & 1U)
        codeword |= 1U;
    return codeword;
}

static uint32_t with_checksum(uint32_t data21)
{
    const uint32_t sum = (data21 & 0x0FU) + ((data21 >> 4) & 0x0FU) +
        ((data21 >> 8) & 0x0FU) + ((data21 >> 12) & 0x0FU) +
        ((data21 >> 16) & 0x0FU) + ((data21 >> 20) & 1U);
    return (data21 & ~0x0FUL) | ((0x0FU - (sum & 0x0FU)) & 0x0FU);
}

static uint8_t text_signature(const char *text)
{
    uint32_t sum = 0;
    while (*text != '\0')
        sum += (uint8_t)*text++;
    return (uint8_t)((~sum) & 0x7FU);
}

static uint8_t make_alpha_page(const char *text, uint8_t fragment,
                               bool continuation, uint8_t message_number,
                               uint8_t signature, uint32_t *output,
                               uint8_t capacity)
{
    if (capacity < 2U)
        return 0;
    const size_t length = strlen(text);
    size_t character = 0;
    uint8_t count = 0;
    output[count++] = ((uint32_t)(fragment & 3U) << 11) |
        ((uint32_t)continuation << 10) |
        ((uint32_t)(message_number & 0x3FU) << 13) | (1UL << 19);

    while (count < capacity)
    {
        uint32_t word = 0;
        for (uint8_t slot = 0; slot < 3U; ++slot)
        {
            uint8_t value;
            if (count == 1U && slot == 0U && fragment == 3U)
                value = signature;
            else
                value = character < length ? (uint8_t)text[character++] & 0x7FU : 0x03U;
            word |= (uint32_t)value << (slot * 7U);
        }
        output[count++] = word;
        if (character >= length)
            break;
    }
    if (character < length)
        return 0;

    uint32_t checksum = 0;
    for (uint8_t i = 0; i < count; ++i)
    {
        checksum += output[i] & 0xFFU;
        checksum += (output[i] >> 8) & 0xFFU;
        checksum += (output[i] >> 16) & 0x1FU;
    }
    output[0] |= (~checksum) & 0x3FFU;
    for (uint8_t i = 0; i < count; ++i)
        output[i] = encode_word(output[i]);
    return count;
}

typedef struct {
    uint8_t bytes[1024];
    size_t count;
} test_frame_t;

static void add_byte(test_frame_t *frame, uint8_t value)
{
    frame->bytes[frame->count++] = value;
}

static void add_word(test_frame_t *frame, uint32_t word)
{
    add_byte(frame, (word >> 24) & 0xFFU);
    add_byte(frame, (word >> 16) & 0xFFU);
    add_byte(frame, (word >> 8) & 0xFFU);
    add_byte(frame, word & 0xFFU);
}

static void add_interleaved(test_frame_t *frame, const uint32_t words[8],
                            bool duplicate_symbols)
{
    for (uint8_t bit = 0; bit < 32U; ++bit)
    {
        uint8_t value = 0;
        for (uint8_t word = 0; word < 8U; ++word)
            value |= ((words[word] >> (31U - bit)) & 1U) << (7U - word);
        if (duplicate_symbols)
        {
            uint8_t expanded[2] = {0U, 0U};
            for (uint8_t i = 0; i < 8U; ++i)
            {
                const uint8_t symbol = (value >> (7U - i)) & 1U;
                expanded[i / 4U] |= symbol << (7U - (i % 4U) * 2U);
                expanded[i / 4U] |= symbol << (6U - (i % 4U) * 2U);
            }
            add_byte(frame, expanded[0]);
            add_byte(frame, expanded[1]);
        }
        else
        {
            add_byte(frame, value);
        }
    }
}

static void make_long_address(uint32_t capcode, uint32_t *first, uint32_t *second)
{
    const uint32_t value = capcode - 0x1F9000UL;
    *first = encode_word(value & 0x7FFFUL);
    *second = encode_word(((value >> 15) ^ 0x001FFFFFUL) & 0x001FFFFFUL);
}

static void build_frame(test_frame_t *frame, uint16_t baud, uint32_t capcode,
                        bool long_address, uint8_t type,
                        const uint32_t *page, uint8_t page_count)
{
    static const uint8_t sync1_prefix[] = {0xAA, 0xAA, 0xAA, 0xAA};
    static const uint8_t sync2[] = {0xAE, 0xD8, 0x45, 0x12, 0x7B};
    const uint8_t sync_high = baud == 1600U ? 0x87 : 0x7B;
    const uint8_t sync_low = baud == 1600U ? 0x0C : 0x18;
    memset(frame, 0, sizeof(*frame));
    for (size_t i = 0; i < sizeof(sync1_prefix); ++i)
        add_byte(frame, sync1_prefix[i]);
    add_byte(frame, sync_high);
    add_byte(frame, sync_low);
    add_byte(frame, 0xA6);
    add_byte(frame, 0xC6);
    add_byte(frame, 0xAA);
    add_byte(frame, 0xAA);
    add_byte(frame, (uint8_t)~sync_high);
    add_byte(frame, (uint8_t)~sync_low);
    add_byte(frame, 0xA6);
    add_byte(frame, 0xC6);
    add_word(frame, encode_word(with_checksum((2UL << 4) | (9UL << 8))));
    for (size_t i = 0; i < sizeof(sync2); ++i)
        add_byte(frame, sync2[i]);
    if (baud == 3200U)
    {
        for (size_t i = 0; i < sizeof(sync2); ++i)
            add_byte(frame, sync2[i]);
    }

    uint32_t words[TEST_PHASE_WORDS];
    for (uint8_t i = 0; i < TEST_PHASE_WORDS; ++i)
        words[i] = (i & 1U) ? 0U : 0xFFFFFFFFUL;
    const uint8_t address_start = 1U;
    const uint8_t vector_start = long_address ? 3U : 2U;
    const uint8_t message_start = vector_start + (long_address ? 2U : 1U);
    words[0] = encode_word(with_checksum(
        ((uint32_t)(address_start - 1U) << 8) | ((uint32_t)vector_start << 10)));
    if (long_address)
    {
        make_long_address(capcode, &words[address_start], &words[address_start + 1U]);
    }
    else
    {
        words[address_start] = encode_word((capcode + 0x8000UL) & 0x1FFFFFUL);
    }

    uint32_t vector;
    if (type == 2U)
    {
        vector = (2UL << 4) | (1UL << 7);
    }
    else
    {
        const uint8_t vector_length = type == 3U || type == 4U || type == 7U ?
            page_count - 1U : page_count;
        vector = ((uint32_t)type << 4) | ((uint32_t)message_start << 7) |
            ((uint32_t)vector_length << 14);
    }
    words[vector_start] = encode_word(with_checksum(vector));
    if (long_address && (type == 5U || type == 0U))
    {
        words[vector_start + 1U] = page[0];
        for (uint8_t i = 1U; i < page_count; ++i)
            words[message_start + i - 1U] = page[i];
    }
    else
    {
        for (uint8_t i = 0; i < page_count; ++i)
            words[message_start + i] = page[i];
    }

    for (uint8_t block = 0; block < 11U; ++block)
        add_interleaved(frame, &words[block * 8U], baud == 3200U);
    for (uint8_t i = 0; i < 4U; ++i)
        add_byte(frame, 0xAA);
}

static void feed_frame(flex_receiver_t *receiver, const test_frame_t *frame,
                       uint16_t baud, uint32_t now_ms)
{
    for (size_t i = 0; i < frame->count * 8U; ++i)
    {
        const bool wire_bit = ((frame->bytes[i / 8U] >> (7U - i % 8U)) & 1U) != 0U;
        const uint16_t period = i < 18U * 8U || baud == 1600U ? 625U : 313U;
        const bool level = i < 14U * 8U ? !wire_bit : wire_bit;
        flex_receiver_feed(receiver, level, period, now_ms);
    }
}

static void test_alpha_1600_and_duplicate(void)
{
    flex_receiver_t receiver;
    flex_receiver_init(&receiver, capture_message, NULL);
    uint32_t page[8];
    const uint8_t count = make_alpha_page("HELLO", 3U, false, 1U,
                                          text_signature("HELLO"), page, 8U);
    test_frame_t frame;
    build_frame(&frame, 1600U, 123456UL, false, 5U, page, count);
    feed_frame(&receiver, &frame, 1600U, 1000U);

    TEST_ASSERT_EQUAL_UINT8(1U, captured_count);
    TEST_ASSERT_EQUAL_UINT8(PAGER_PROTOCOL_FLEX, captured[0].protocol);
    TEST_ASSERT_EQUAL_UINT32(123456UL, captured[0].ric);
    TEST_ASSERT_EQUAL_UINT16(1600U, captured[0].baud);
    TEST_ASSERT_EQUAL_UINT8(2U, captured[0].levels);
    TEST_ASSERT_EQUAL_STRING("HELLO", captured[0].text);
    TEST_ASSERT_FALSE(captured[0].partial);

    feed_frame(&receiver, &frame, 1600U, 3000U);
    TEST_ASSERT_EQUAL_UINT8(1U, captured_count);
}

static void test_bch_error_correction(void)
{
    flex_receiver_t receiver;
    flex_receiver_init(&receiver, capture_message, NULL);
    uint32_t page[8];
    const uint8_t count = make_alpha_page("ECC", 3U, false, 1U,
                                          text_signature("ECC"), page, 8U);
    page[1] ^= 1UL;
    test_frame_t frame;
    build_frame(&frame, 1600U, 1234UL, false, 5U, page, count);
    feed_frame(&receiver, &frame, 1600U, 1000U);

    TEST_ASSERT_EQUAL_UINT8(1U, captured_count);
    TEST_ASSERT_EQUAL_STRING("ECC", captured[0].text);
}

static void test_alpha_3200_with_long_capcode(void)
{
    flex_receiver_t receiver;
    flex_receiver_init(&receiver, capture_message, NULL);
    uint32_t page[8];
    const uint8_t count = make_alpha_page("LONG", 3U, false, 2U,
                                          text_signature("LONG"), page, 8U);
    const uint32_t capcode = 0x1F9000UL + 12345UL;
    test_frame_t frame;
    build_frame(&frame, 3200U, capcode, true, 5U, page, count);
    feed_frame(&receiver, &frame, 3200U, 1000U);

    TEST_ASSERT_EQUAL_UINT8(1U, captured_count);
    TEST_ASSERT_EQUAL_UINT32(capcode, captured[0].ric);
    TEST_ASSERT_EQUAL_UINT16(3200U, captured[0].baud);
    TEST_ASSERT_EQUAL_STRING("LONG", captured[0].text);
}

static uint32_t make_numeric_word(const char *digits)
{
    uint32_t word = 0;
    uint8_t bit_offset = 2U;
    for (uint8_t i = 0; i < 4U && digits[i] != '\0'; ++i)
    {
        const uint8_t value = (uint8_t)(digits[i] - '0');
        word |= (uint32_t)value << bit_offset;
        bit_offset += 4U;
    }
    return encode_word(word);
}

static void test_numeric_and_tone_pages(void)
{
    flex_receiver_t receiver;
    flex_receiver_init(&receiver, capture_message, NULL);
    const uint32_t numeric = make_numeric_word("1234");
    test_frame_t frame;
    build_frame(&frame, 1600U, 123456UL, false, 3U, &numeric, 1U);
    feed_frame(&receiver, &frame, 1600U, 1000U);
    TEST_ASSERT_EQUAL_STRING("1234", captured[0].text);
    build_frame(&frame, 1600U, 123456UL, false, 2U, NULL, 0U);
    feed_frame(&receiver, &frame, 1600U, 3000U);

    TEST_ASSERT_EQUAL_UINT8(2U, captured_count);
    TEST_ASSERT_EQUAL_STRING("1234", captured[0].text);
    TEST_ASSERT_EQUAL_STRING("Tone only", captured[1].text);
}

static void test_fragmented_message_join(void)
{
    flex_receiver_t receiver;
    flex_receiver_init(&receiver, capture_message, NULL);
    const char *complete = "HELLO WORLD";
    const uint8_t signature = text_signature(complete);
    uint32_t first[8];
    uint32_t last[8];
    const uint8_t first_count = make_alpha_page("HELLO ", 3U, true, 7U,
                                                signature, first, 8U);
    const uint8_t last_count = make_alpha_page("WORLD", 0U, false, 7U,
                                               0U, last, 8U);
    test_frame_t frame;
    build_frame(&frame, 1600U, 123456UL, false, 5U, first, first_count);
    feed_frame(&receiver, &frame, 1600U, 1000U);
    TEST_ASSERT_EQUAL_UINT8(0U, captured_count);
    build_frame(&frame, 1600U, 123456UL, false, 5U, last, last_count);
    feed_frame(&receiver, &frame, 1600U, 3000U);

    TEST_ASSERT_EQUAL_UINT8(1U, captured_count);
    TEST_ASSERT_EQUAL_STRING(complete, captured[0].text);
    TEST_ASSERT_FALSE(captured[0].partial);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_alpha_1600_and_duplicate);
    RUN_TEST(test_bch_error_correction);
    RUN_TEST(test_alpha_3200_with_long_capcode);
    RUN_TEST(test_numeric_and_tone_pages);
    RUN_TEST(test_fragmented_message_join);
    return UNITY_END();
}
