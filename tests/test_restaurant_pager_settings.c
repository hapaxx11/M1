/* See COPYING.txt for license details. */

#include "unity.h"
#include "restaurant_pager_settings.h"

#include <string.h>

void setUp(void) { }
void tearDown(void) { }

void test_parse_and_format_settings(void)
{
    restaurant_pager_settings_t settings;
    char buffer[16];

    TEST_ASSERT_TRUE(restaurant_pager_settings_parse("255 10\n", &settings));
    TEST_ASSERT_EQUAL_UINT16(255U, settings.max_pager);
    TEST_ASSERT_EQUAL_UINT8(10U, settings.tx_repeat_count);
    TEST_ASSERT_TRUE(restaurant_pager_settings_format(&settings, buffer,
                                                       sizeof(buffer)));
    TEST_ASSERT_EQUAL_STRING("255 10\n", buffer);
}

void test_settings_validation_rejects_invalid_values(void)
{
    restaurant_pager_settings_t settings = {
        .max_pager = 255U,
        .tx_repeat_count = 10U,
    };
    char buffer[16];

    TEST_ASSERT_FALSE(restaurant_pager_settings_parse(NULL, &settings));
    TEST_ASSERT_FALSE(restaurant_pager_settings_parse("1024 10", &settings));
    TEST_ASSERT_FALSE(restaurant_pager_settings_parse("255 0", &settings));
    TEST_ASSERT_FALSE(restaurant_pager_settings_parse("255 51", &settings));
    TEST_ASSERT_FALSE(restaurant_pager_settings_parse("255 -1", &settings));
    TEST_ASSERT_FALSE(restaurant_pager_settings_parse("255 10 extra", &settings));
    TEST_ASSERT_FALSE(restaurant_pager_settings_parse(" 10", &settings));
    TEST_ASSERT_FALSE(restaurant_pager_settings_format(&settings, buffer, 1U));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_parse_and_format_settings);
    RUN_TEST(test_settings_validation_rejects_invalid_values);
    return UNITY_END();
}
