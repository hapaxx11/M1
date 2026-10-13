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

void test_password_clear_writes_use_password_then_normal_layout(void)
{
    const uint32_t password = 0x12345678u;
    rfid_t5577_password_clear_write_t write;

    TEST_ASSERT_TRUE(rfid_t5577_password_clear_write(0u, password, &write));
    TEST_ASSERT_EQUAL_UINT8(0u, write.page);
    TEST_ASSERT_EQUAL_UINT8(0u, write.block);
    TEST_ASSERT_FALSE(write.lock_bit);
    TEST_ASSERT_EQUAL_HEX32(0x00148040u, write.data);
    TEST_ASSERT_TRUE(write.with_password);
    TEST_ASSERT_EQUAL_HEX32(password, write.password);

    TEST_ASSERT_TRUE(rfid_t5577_password_clear_write(1u, password, &write));
    TEST_ASSERT_EQUAL_UINT8(0u, write.page);
    TEST_ASSERT_EQUAL_UINT8(7u, write.block);
    TEST_ASSERT_FALSE(write.lock_bit);
    TEST_ASSERT_EQUAL_HEX32(0u, write.data);
    TEST_ASSERT_FALSE(write.with_password);
    TEST_ASSERT_EQUAL_HEX32(0u, write.password);
}

void test_password_clear_write_rejects_invalid_index_or_output(void)
{
    rfid_t5577_password_clear_write_t write;

    TEST_ASSERT_FALSE(rfid_t5577_password_clear_write(
        RFID_T5577_PASSWORD_CLEAR_WRITE_COUNT, 0u, &write));
    TEST_ASSERT_FALSE(rfid_t5577_password_clear_write(0u, 0u, NULL));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_password_parse_accepts_upper_and_lower_hex);
    RUN_TEST(test_password_parse_rejects_short_long_and_non_hex_values);
    RUN_TEST(test_password_parse_rejects_null_arguments);
    RUN_TEST(test_password_clear_writes_use_password_then_normal_layout);
    RUN_TEST(test_password_clear_write_rejects_invalid_index_or_output);
    return UNITY_END();
}
