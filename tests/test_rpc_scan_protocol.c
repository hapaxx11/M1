/* See COPYING.txt for license details. */

#include "unity.h"

#include "m1_rpc.h"
#include "m1_rpc_scan.h"

typedef struct {
    uint8_t error_address;
    uint8_t calls;
} scan_probe_fixture_t;

static m1_i2c_scan_probe_result_t fake_i2c_probe(uint8_t address,
                                                  void *context)
{
    scan_probe_fixture_t *fixture = (scan_probe_fixture_t *)context;
    fixture->calls++;
    if (address == fixture->error_address)
        return M1_I2C_SCAN_PROBE_ERROR;
    if (address == 0x09u || address == 0x0Bu)
        return M1_I2C_SCAN_PROBE_ACK;
    return M1_I2C_SCAN_PROBE_NACK;
}

void setUp(void) {}
void tearDown(void) {}

void test_gpio_scan_encodes_pin_count_and_little_endian_levels(void)
{
    uint8_t response[RPC_GPIO_SCAN_RESULT_SIZE] = {0};

    TEST_ASSERT_TRUE(rpc_gpio_scan_encode(0x4123u, response, sizeof(response)));
    TEST_ASSERT_EQUAL_UINT8(RPC_GPIO_SCAN_PIN_COUNT, response[0]);
    TEST_ASSERT_EQUAL_UINT8(0x23u, response[1]);
    TEST_ASSERT_EQUAL_UINT8(0x41u, response[2]);
}

void test_scan_command_ids_use_reserved_hardware_range(void)
{
    TEST_ASSERT_EQUAL_HEX8(0x70u, RPC_CMD_GPIO_SCAN);
    TEST_ASSERT_EQUAL_HEX8(0x71u, RPC_CMD_GPIO_SCAN_RESP);
    TEST_ASSERT_EQUAL_HEX8(0x72u, RPC_CMD_I2C_SCAN);
    TEST_ASSERT_EQUAL_HEX8(0x73u, RPC_CMD_I2C_SCAN_RESP);
}

void test_gpio_scan_rejects_short_response_buffer(void)
{
    uint8_t response[RPC_GPIO_SCAN_RESULT_SIZE - 1u] = {0};

    TEST_ASSERT_FALSE(rpc_gpio_scan_encode(0u, response, sizeof(response)));
}

void test_i2c_scan_request_accepts_valid_inclusive_range(void)
{
    const uint8_t payload[] = {0x20u, 0x24u};
    uint8_t first = 0u;
    uint8_t last = 0u;

    TEST_ASSERT_TRUE(rpc_i2c_scan_request_decode(payload, sizeof(payload),
                                                 &first, &last));
    TEST_ASSERT_EQUAL_UINT8(0x20u, first);
    TEST_ASSERT_EQUAL_UINT8(0x24u, last);
}

void test_i2c_scan_request_rejects_invalid_ranges_and_payload_lengths(void)
{
    const uint8_t reserved_start[] = {0x07u, 0x20u};
    const uint8_t reserved_end[] = {0x20u, 0x78u};
    const uint8_t reversed[] = {0x30u, 0x20u};
    const uint8_t valid[] = {0x20u, 0x24u};
    uint8_t first;
    uint8_t last;

    TEST_ASSERT_FALSE(rpc_i2c_scan_request_decode(reserved_start,
                                                  sizeof(reserved_start),
                                                  &first, &last));
    TEST_ASSERT_FALSE(rpc_i2c_scan_request_decode(reserved_end,
                                                  sizeof(reserved_end),
                                                  &first, &last));
    TEST_ASSERT_FALSE(rpc_i2c_scan_request_decode(reversed,
                                                  sizeof(reversed),
                                                  &first, &last));
    TEST_ASSERT_FALSE(rpc_i2c_scan_request_decode(valid, 1u, &first, &last));
    TEST_ASSERT_FALSE(rpc_i2c_scan_request_decode(valid, 3u, &first, &last));
    TEST_ASSERT_FALSE(rpc_i2c_scan_request_decode(NULL, 0u, &first, &last));
}

void test_i2c_scan_response_encodes_ascending_addresses(void)
{
    const uint8_t addresses[] = {0x10u, 0x44u, 0x77u};
    uint8_t response[RPC_I2C_SCAN_MAX_RESPONSE_SIZE] = {0};
    uint16_t response_len = 0u;

    TEST_ASSERT_TRUE(rpc_i2c_scan_response_encode(addresses,
                                                  sizeof(addresses),
                                                  response, sizeof(response),
                                                  &response_len));
    TEST_ASSERT_EQUAL_UINT16(4u, response_len);
    TEST_ASSERT_EQUAL_UINT8(3u, response[0]);
    TEST_ASSERT_EQUAL_UINT8(0x10u, response[1]);
    TEST_ASSERT_EQUAL_UINT8(0x44u, response[2]);
    TEST_ASSERT_EQUAL_UINT8(0x77u, response[3]);
}

void test_i2c_scan_response_encodes_empty_result(void)
{
    uint8_t response[RPC_I2C_SCAN_MAX_RESPONSE_SIZE] = {0xFFu};
    uint16_t response_len = 0u;

    TEST_ASSERT_TRUE(rpc_i2c_scan_response_encode(NULL, 0u, response,
                                                  sizeof(response),
                                                  &response_len));
    TEST_ASSERT_EQUAL_UINT16(1u, response_len);
    TEST_ASSERT_EQUAL_UINT8(0u, response[0]);
}

void test_i2c_scan_response_rejects_invalid_results_or_capacity(void)
{
    const uint8_t duplicate[] = {0x20u, 0x20u};
    const uint8_t out_of_range[] = {0x08u, 0x78u};
    const uint8_t valid[] = {0x20u};
    uint8_t response[RPC_I2C_SCAN_MAX_RESPONSE_SIZE] = {0};
    uint16_t response_len = 99u;

    TEST_ASSERT_FALSE(rpc_i2c_scan_response_encode(
        duplicate, sizeof(duplicate), response, sizeof(response), &response_len));
    TEST_ASSERT_FALSE(rpc_i2c_scan_response_encode(
        out_of_range, sizeof(out_of_range), response, sizeof(response), &response_len));
    TEST_ASSERT_FALSE(rpc_i2c_scan_response_encode(
        valid, sizeof(valid), response, 1u, &response_len));
    TEST_ASSERT_FALSE(rpc_i2c_scan_response_encode(
        NULL, 1u, response, sizeof(response), &response_len));
}

void test_i2c_scan_collect_returns_only_acknowledged_addresses(void)
{
    scan_probe_fixture_t fixture = {.error_address = 0xFFu, .calls = 0u};
    uint8_t addresses[4] = {0};
    uint8_t count = 0u;

    TEST_ASSERT_TRUE(m1_i2c_scan_collect(0x08u, 0x0Bu, fake_i2c_probe,
                                         &fixture, addresses,
                                         sizeof(addresses), &count));
    TEST_ASSERT_EQUAL_UINT8(2u, count);
    TEST_ASSERT_EQUAL_UINT8(4u, fixture.calls);
    TEST_ASSERT_EQUAL_UINT8(0x09u, addresses[0]);
    TEST_ASSERT_EQUAL_UINT8(0x0Bu, addresses[1]);
}

void test_i2c_scan_collect_aborts_on_bus_error_without_partial_count(void)
{
    scan_probe_fixture_t fixture = {.error_address = 0x0Au, .calls = 0u};
    uint8_t addresses[4] = {0};
    uint8_t count = 9u;

    TEST_ASSERT_FALSE(m1_i2c_scan_collect(0x08u, 0x0Bu, fake_i2c_probe,
                                          &fixture, addresses,
                                          sizeof(addresses), &count));
    TEST_ASSERT_EQUAL_UINT8(0u, count);
    TEST_ASSERT_EQUAL_UINT8(3u, fixture.calls);
}

void test_i2c_scan_collect_rejects_insufficient_capacity(void)
{
    scan_probe_fixture_t fixture = {.error_address = 0xFFu, .calls = 0u};
    uint8_t addresses[3] = {0};
    uint8_t count = 9u;

    TEST_ASSERT_FALSE(m1_i2c_scan_collect(0x08u, 0x0Bu, fake_i2c_probe,
                                          &fixture, addresses,
                                          sizeof(addresses), &count));
    TEST_ASSERT_EQUAL_UINT8(0u, count);
    TEST_ASSERT_EQUAL_UINT8(0u, fixture.calls);
}

void test_i2c_scan_probe_classifies_completed_nack_and_real_timeout(void)
{
    TEST_ASSERT_EQUAL(M1_I2C_SCAN_PROBE_NACK,
                      m1_i2c_scan_probe_classify(true, true));
    TEST_ASSERT_EQUAL(M1_I2C_SCAN_PROBE_ERROR,
                      m1_i2c_scan_probe_classify(false, true));
    TEST_ASSERT_EQUAL(M1_I2C_SCAN_PROBE_ACK,
                      m1_i2c_scan_probe_classify(true, false));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_gpio_scan_encodes_pin_count_and_little_endian_levels);
    RUN_TEST(test_scan_command_ids_use_reserved_hardware_range);
    RUN_TEST(test_gpio_scan_rejects_short_response_buffer);
    RUN_TEST(test_i2c_scan_request_accepts_valid_inclusive_range);
    RUN_TEST(test_i2c_scan_request_rejects_invalid_ranges_and_payload_lengths);
    RUN_TEST(test_i2c_scan_response_encodes_ascending_addresses);
    RUN_TEST(test_i2c_scan_response_encodes_empty_result);
    RUN_TEST(test_i2c_scan_response_rejects_invalid_results_or_capacity);
    RUN_TEST(test_i2c_scan_collect_returns_only_acknowledged_addresses);
    RUN_TEST(test_i2c_scan_collect_aborts_on_bus_error_without_partial_count);
    RUN_TEST(test_i2c_scan_collect_rejects_insufficient_capacity);
    RUN_TEST(test_i2c_scan_probe_classifies_completed_nack_and_real_timeout);
    return UNITY_END();
}
