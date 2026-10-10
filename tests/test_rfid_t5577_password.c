#include "unity.h"

#include "rfid_t5577_password.h"

void setUp(void) {}
void tearDown(void) {}

void test_password_parse_accepts_upper_and_lower_hex(void)
{
    uint32_t password = 0;
    TEST_ASSERT_TRUE(rfid_t5577_password_parse("01aB23fF", &password));
    TEST_ASSERT_EQUAL_HEX32(0x01AB23FFu, password);
}

void test_password_parse_rejects_short_long_and_non_hex_values(void)
{
    uint32_t password = 0x12345678;
    TEST_ASSERT_FALSE(rfid_t5577_password_parse("1234567", &password));
    TEST_ASSERT_FALSE(rfid_t5577_password_parse("123456789", &password));
    TEST_ASSERT_FALSE(rfid_t5577_password_parse("12345g78", &password));
    TEST_ASSERT_EQUAL_HEX32(0x12345678u, password);
}

void test_password_parse_rejects_null_arguments(void)
{
    uint32_t password = 0;
    TEST_ASSERT_FALSE(rfid_t5577_password_parse(NULL, &password));
    TEST_ASSERT_FALSE(rfid_t5577_password_parse("12345678", NULL));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_password_parse_accepts_upper_and_lower_hex);
    RUN_TEST(test_password_parse_rejects_short_long_and_non_hex_values);
    RUN_TEST(test_password_parse_rejects_null_arguments);
    return UNITY_END();
}
