/* See COPYING.txt for license details. */

#include "unity.h"

#include "m1_rpc_transport.h"

void setUp(void) {}
void tearDown(void) {}

void test_reply_route_matches_request_source(void)
{
    TEST_ASSERT_EQUAL(M1_RPC_TRANSPORT_USB,
                      m1_rpc_transport_reply_to_request(
                          M1_RPC_TRANSPORT_USB));
    TEST_ASSERT_EQUAL(M1_RPC_TRANSPORT_BLE,
                      m1_rpc_transport_reply_to_request(
                          M1_RPC_TRANSPORT_BLE));
    TEST_ASSERT_EQUAL(M1_RPC_TRANSPORT_WIFI,
                      m1_rpc_transport_reply_to_request(
                          M1_RPC_TRANSPORT_WIFI));
    TEST_ASSERT_EQUAL(M1_RPC_TRANSPORT_ESP32,
                      m1_rpc_transport_reply_to_request(
                          M1_RPC_TRANSPORT_ESP32));
    TEST_ASSERT_EQUAL(M1_RPC_TRANSPORT_USB,
                      m1_rpc_transport_reply_to_request(
                          M1_RPC_TRANSPORT_COUNT));
}

void test_partial_frame_remains_owned_by_its_ingress_transport(void)
{
    TEST_ASSERT_TRUE(m1_rpc_transport_feed_allowed(
        false, M1_RPC_TRANSPORT_USB, M1_RPC_TRANSPORT_BLE));
    TEST_ASSERT_TRUE(m1_rpc_transport_feed_allowed(
        true, M1_RPC_TRANSPORT_BLE, M1_RPC_TRANSPORT_BLE));
    TEST_ASSERT_TRUE(m1_rpc_transport_feed_allowed(
        false, M1_RPC_TRANSPORT_USB, M1_RPC_TRANSPORT_ESP32));
    TEST_ASSERT_FALSE(m1_rpc_transport_feed_allowed(
        true, M1_RPC_TRANSPORT_USB, M1_RPC_TRANSPORT_BLE));
    TEST_ASSERT_FALSE(m1_rpc_transport_feed_allowed(
        true, M1_RPC_TRANSPORT_USB, M1_RPC_TRANSPORT_COUNT));
}

void test_mobile_reply_payload_limit_is_enforced(void)
{
    TEST_ASSERT_TRUE(m1_rpc_transport_reply_payload_fits(
        M1_RPC_TRANSPORT_ESP32, 4075u, 4075u));
    TEST_ASSERT_FALSE(m1_rpc_transport_reply_payload_fits(
        M1_RPC_TRANSPORT_ESP32, 4076u, 4075u));
    TEST_ASSERT_TRUE(m1_rpc_transport_reply_payload_fits(
        M1_RPC_TRANSPORT_USB, 8192u, 0u));
}

void test_file_write_commands_are_isolated_by_transport(void)
{
    TEST_ASSERT_TRUE(m1_rpc_transport_file_write_allowed(
        false, M1_RPC_TRANSPORT_USB, M1_RPC_TRANSPORT_ESP32, true));
    TEST_ASSERT_TRUE(m1_rpc_transport_file_write_allowed(
        true, M1_RPC_TRANSPORT_ESP32, M1_RPC_TRANSPORT_ESP32, true));
    TEST_ASSERT_FALSE(m1_rpc_transport_file_write_allowed(
        true, M1_RPC_TRANSPORT_ESP32, M1_RPC_TRANSPORT_USB, true));
    TEST_ASSERT_TRUE(m1_rpc_transport_file_write_allowed(
        true, M1_RPC_TRANSPORT_ESP32, M1_RPC_TRANSPORT_ESP32, false));
    TEST_ASSERT_FALSE(m1_rpc_transport_file_write_allowed(
        true, M1_RPC_TRANSPORT_ESP32, M1_RPC_TRANSPORT_USB, false));
    TEST_ASSERT_FALSE(m1_rpc_transport_file_write_allowed(
        false, M1_RPC_TRANSPORT_ESP32, M1_RPC_TRANSPORT_ESP32, false));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_reply_route_matches_request_source);
    RUN_TEST(test_partial_frame_remains_owned_by_its_ingress_transport);
    RUN_TEST(test_mobile_reply_payload_limit_is_enforced);
    RUN_TEST(test_file_write_commands_are_isolated_by_transport);
    return UNITY_END();
}
