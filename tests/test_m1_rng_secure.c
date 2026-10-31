#include "unity.h"
#include "m1_rng.h"
#include "stm32h5xx_hal.h"
#include <string.h>

RCC_TypeDef test_rng_rcc;
RNG_TypeDef test_rng_regs = {
    .SR = RNG_SR_DRDY,
    .DR = 0x12345678u,
};

void setUp(void) {}
void tearDown(void) {}

void test_secure_random_bytes_uses_hardware_output(void) {
    uint8_t bytes[4] = {0};

    TEST_ASSERT_TRUE(m1_crypto_random_bytes(bytes, sizeof(bytes)));
    TEST_ASSERT_EQUAL_HEX8(0x78, bytes[0]);
    TEST_ASSERT_EQUAL_HEX8(0x56, bytes[1]);
    TEST_ASSERT_EQUAL_HEX8(0x34, bytes[2]);
    TEST_ASSERT_EQUAL_HEX8(0x12, bytes[3]);
}

void test_secure_random_bytes_fails_and_clears_output_on_entropy_error(void) {
    uint8_t bytes[8];
    memset(bytes, 0xA5, sizeof(bytes));
    test_rng_regs.SR = RNG_SR_SEIS;

    TEST_ASSERT_FALSE(m1_crypto_random_bytes(bytes, sizeof(bytes)));
    for(size_t i = 0; i < sizeof(bytes); i++) {
        TEST_ASSERT_EQUAL_HEX8(0, bytes[i]);
    }

    memset(bytes, 0xA5, sizeof(bytes));
    TEST_ASSERT_FALSE(m1_crypto_random_bytes(bytes, sizeof(bytes)));
    for(size_t i = 0; i < sizeof(bytes); i++) {
        TEST_ASSERT_EQUAL_HEX8(0, bytes[i]);
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_secure_random_bytes_uses_hardware_output);
    RUN_TEST(test_secure_random_bytes_fails_and_clears_output_on_entropy_error);
    return UNITY_END();
}
