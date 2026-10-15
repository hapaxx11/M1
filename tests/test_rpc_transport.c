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

void test_usb_epoch_reset_clears_only_usb_owned_partial_frame(void)
{
    TEST_ASSERT_TRUE(m1_rpc_transport_parser_reset_on_usb_epoch(
        true, true, M1_RPC_TRANSPORT_USB));
    TEST_ASSERT_FALSE(m1_rpc_transport_parser_reset_on_usb_epoch(
        true, true, M1_RPC_TRANSPORT_ESP32));
    TEST_ASSERT_FALSE(m1_rpc_transport_parser_reset_on_usb_epoch(
        false, true, M1_RPC_TRANSPORT_USB));
    TEST_ASSERT_TRUE(m1_rpc_transport_upload_allowed(
        true, M1_RPC_TRANSPORT_USB, M1_RPC_TRANSPORT_USB));
    TEST_ASSERT_FALSE(m1_rpc_transport_upload_allowed(
        true, M1_RPC_TRANSPORT_USB, M1_RPC_TRANSPORT_ESP32));
    TEST_ASSERT_TRUE(m1_rpc_transport_upload_allowed(
        true, M1_RPC_TRANSPORT_ESP32, M1_RPC_TRANSPORT_ESP32));
}

void test_transport_response_payload_limits_include_relay_overhead(void)
{
    TEST_ASSERT_FALSE(m1_rpc_transport_response_too_large(
        M1_RPC_TRANSPORT_ESP32, 4075u, 8192u, 4082u, 7u));
    TEST_ASSERT_TRUE(m1_rpc_transport_response_too_large(
        M1_RPC_TRANSPORT_ESP32, 4076u, 8192u, 4082u, 7u));
    TEST_ASSERT_FALSE(m1_rpc_transport_response_too_large(
        M1_RPC_TRANSPORT_USB, 8192u, 8192u, 4082u, 7u));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_reply_route_matches_request_source);
    RUN_TEST(test_partial_frame_remains_owned_by_its_ingress_transport);
    RUN_TEST(test_usb_epoch_reset_clears_only_usb_owned_partial_frame);
    RUN_TEST(test_transport_response_payload_limits_include_relay_overhead);
    return UNITY_END();
}
