/* See COPYING.txt for license details. */

#include "unity.h"
#include "restaurant_pager.h"

void setUp(void) { }
void tearDown(void) { }

static uint32_t reverse_bits(uint32_t value, uint8_t bits)
{
    uint32_t reversed = 0U;
    for (uint8_t bit = 0U; bit < bits; ++bit)
    {
        reversed = (reversed << 1U) | (value & 1U);
        value >>= 1U;
    }
    return reversed;
}

void test_td157_decode_and_edit_fields(void)
{
    uint32_t data = (0x2ABUL << 14U) | (0x155UL << 4U) | 2U;
    restaurant_pager_fields_t fields;
    TEST_ASSERT_TRUE(restaurant_pager_decode(RESTAURANT_PAGER_TD157,
                                              data, &fields));
    TEST_ASSERT_EQUAL_UINT16(0x2ABU, fields.station);
    TEST_ASSERT_EQUAL_UINT16(0x155U, fields.pager);
    TEST_ASSERT_EQUAL(RESTAURANT_PAGER_ACTION_RING, fields.action);

    TEST_ASSERT_TRUE(restaurant_pager_set_pager(RESTAURANT_PAGER_TD157,
                                                 &data, 0x234U));
    TEST_ASSERT_TRUE(restaurant_pager_set_action(
        RESTAURANT_PAGER_TD157, &data, RESTAURANT_PAGER_ACTION_ALL_OFF));
    TEST_ASSERT_TRUE(restaurant_pager_decode(RESTAURANT_PAGER_TD157,
                                              data, &fields));
    TEST_ASSERT_EQUAL_UINT16(999U, fields.pager);
    TEST_ASSERT_EQUAL(RESTAURANT_PAGER_ACTION_ALL_OFF, fields.action);
    TEST_ASSERT_FALSE(restaurant_pager_set_pager(RESTAURANT_PAGER_TD157,
                                                  &data, 1024U));
}

void test_td165_reversed_fields_and_mute(void)
{
    uint32_t data = (reverse_bits(0x123U, 13U) << 11U) |
                    (reverse_bits(42U, 10U) << 1U);
    restaurant_pager_fields_t fields;
    TEST_ASSERT_TRUE(restaurant_pager_decode(RESTAURANT_PAGER_TD165,
                                              data, &fields));
    TEST_ASSERT_EQUAL_UINT16(0x123U, fields.station);
    TEST_ASSERT_EQUAL_UINT16(42U, fields.pager);
    TEST_ASSERT_EQUAL(RESTAURANT_PAGER_ACTION_RING, fields.action);

    TEST_ASSERT_TRUE(restaurant_pager_set_action(
        RESTAURANT_PAGER_TD165, &data, RESTAURANT_PAGER_ACTION_MUTE));
    TEST_ASSERT_TRUE(restaurant_pager_decode(RESTAURANT_PAGER_TD165,
                                              data, &fields));
    TEST_ASSERT_EQUAL(RESTAURANT_PAGER_ACTION_MUTE, fields.action);
    TEST_ASSERT_TRUE(restaurant_pager_set_action(
        RESTAURANT_PAGER_TD165, &data, RESTAURANT_PAGER_ACTION_ALL_OFF));
    TEST_ASSERT_TRUE(restaurant_pager_decode(RESTAURANT_PAGER_TD165,
                                              data, &fields));
    TEST_ASSERT_EQUAL_UINT16(1005U, fields.pager);
    TEST_ASSERT_EQUAL(RESTAURANT_PAGER_ACTION_ALL_OFF, fields.action);
}

void test_td174_desync_fields(void)
{
    uint32_t data = (reverse_bits(0x123U, 13U) << 11U) |
                    reverse_bits(88U, 9U);
    restaurant_pager_fields_t fields;
    TEST_ASSERT_TRUE(restaurant_pager_set_action(
        RESTAURANT_PAGER_TD174, &data, RESTAURANT_PAGER_ACTION_DESYNC));
    TEST_ASSERT_TRUE(restaurant_pager_decode(RESTAURANT_PAGER_TD174,
                                              data, &fields));
    TEST_ASSERT_EQUAL_UINT16(237U, fields.pager);
    TEST_ASSERT_EQUAL(RESTAURANT_PAGER_ACTION_DESYNC, fields.action);
    TEST_ASSERT_EQUAL_UINT16(0x123U, fields.station);
}

void test_l8r_and_l8s_pager_bit_order(void)
{
    uint32_t reversed = (reverse_bits(0x456U, 13U) << 11U) |
                        reverse_bits(173U, 8U);
    uint32_t straight = (0x456UL << 11U) | 173U;
    restaurant_pager_fields_t fields;

    TEST_ASSERT_TRUE(restaurant_pager_decode(RESTAURANT_PAGER_L8R,
                                              reversed, &fields));
    TEST_ASSERT_EQUAL_UINT16(0x456U, fields.station);
    TEST_ASSERT_EQUAL_UINT16(173U, fields.pager);
    TEST_ASSERT_EQUAL(RESTAURANT_PAGER_ACTION_RING, fields.action);

    TEST_ASSERT_TRUE(restaurant_pager_decode(RESTAURANT_PAGER_L8S,
                                              straight, &fields));
    TEST_ASSERT_EQUAL_UINT16(0x456U, fields.station);
    TEST_ASSERT_EQUAL_UINT16(173U, fields.pager);
    TEST_ASSERT_EQUAL(RESTAURANT_PAGER_ACTION_UNKNOWN, fields.action);
    TEST_ASSERT_FALSE(restaurant_pager_set_action(
        RESTAURANT_PAGER_L8S, &straight, RESTAURANT_PAGER_ACTION_RING));
}

void test_auto_detect_respects_pager_limit_and_fallback(void)
{
    uint32_t data = (12UL << 14U) | (5UL << 4U) | 2U;
    TEST_ASSERT_EQUAL(RESTAURANT_PAGER_TD157,
                      restaurant_pager_auto_detect(data, 10U));
    TEST_ASSERT_EQUAL(RESTAURANT_PAGER_TD157,
                      restaurant_pager_auto_detect(data, 4U));
    TEST_ASSERT_EQUAL_STRING("TD174",
        restaurant_pager_format_name(RESTAURANT_PAGER_TD174));
    TEST_ASSERT_EQUAL_STRING("UNKNOWN",
        restaurant_pager_action_name(RESTAURANT_PAGER_ACTION_UNKNOWN));
    TEST_ASSERT_EQUAL_STRING("Unknown",
        restaurant_pager_format_name(RESTAURANT_PAGER_FORMAT_COUNT));
}

void test_invalid_inputs_are_rejected(void)
{
    restaurant_pager_fields_t fields;
    uint32_t data = 0U;
    TEST_ASSERT_FALSE(restaurant_pager_decode(RESTAURANT_PAGER_FORMAT_COUNT,
                                               data, &fields));
    TEST_ASSERT_FALSE(restaurant_pager_decode(RESTAURANT_PAGER_TD157,
                                               data, NULL));
    TEST_ASSERT_FALSE(restaurant_pager_set_pager(RESTAURANT_PAGER_FORMAT_COUNT,
                                                  &data, 1U));
    TEST_ASSERT_FALSE(restaurant_pager_set_pager(RESTAURANT_PAGER_TD157,
                                                  NULL, 1U));
    TEST_ASSERT_FALSE(restaurant_pager_set_action(
        RESTAURANT_PAGER_FORMAT_COUNT, &data, RESTAURANT_PAGER_ACTION_RING));
    TEST_ASSERT_FALSE(restaurant_pager_set_action(
        RESTAURANT_PAGER_TD157, &data, RESTAURANT_PAGER_ACTION_MUTE));
    data = reverse_bits(1005U, 10U) << 1U;
    TEST_ASSERT_FALSE(restaurant_pager_set_action(
        RESTAURANT_PAGER_TD165, &data, RESTAURANT_PAGER_ACTION_RING));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_td157_decode_and_edit_fields);
    RUN_TEST(test_td165_reversed_fields_and_mute);
    RUN_TEST(test_td174_desync_fields);
    RUN_TEST(test_l8r_and_l8s_pager_bit_order);
    RUN_TEST(test_auto_detect_respects_pager_limit_and_fallback);
    RUN_TEST(test_invalid_inputs_are_rejected);
    return UNITY_END();
}
