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

void test_reply_routing_contexts_are_independent(void)
{
    m1_rpc_transport_t parser = M1_RPC_TRANSPORT_USB;
    m1_rpc_transport_t rpc_task = M1_RPC_TRANSPORT_ESP32;
    TEST_ASSERT_EQUAL_PTR(&rpc_task, m1_rpc_transport_reply_slot(
        true, &parser, &rpc_task));
    TEST_ASSERT_EQUAL_PTR(&parser, m1_rpc_transport_reply_slot(
        false, &parser, &rpc_task));
}

void test_payload_limits_respect_relay_envelope(void)
{
    TEST_ASSERT_EQUAL_UINT16(8192u, m1_rpc_transport_payload_limit(
        M1_RPC_TRANSPORT_USB, 8192u, 4075u));
    TEST_ASSERT_EQUAL_UINT16(4075u, m1_rpc_transport_payload_limit(
        M1_RPC_TRANSPORT_ESP32, 8192u, 4075u));
    TEST_ASSERT_EQUAL_UINT16(0u, m1_rpc_transport_payload_limit(
        M1_RPC_TRANSPORT_COUNT, 8192u, 4075u));
    TEST_ASSERT_FALSE(m1_rpc_transport_payload_too_large(
        M1_RPC_TRANSPORT_ESP32, 4075u, 8192u, 4075u));
    TEST_ASSERT_TRUE(m1_rpc_transport_payload_too_large(
        M1_RPC_TRANSPORT_ESP32, 4076u, 8192u, 4075u));
}

void test_upload_transport_ownership_is_enforced(void)
{
    TEST_ASSERT_TRUE(m1_rpc_transport_file_write_start_allowed(
        false, M1_RPC_TRANSPORT_USB, M1_RPC_TRANSPORT_ESP32));
    TEST_ASSERT_TRUE(m1_rpc_transport_file_write_start_allowed(
        true, M1_RPC_TRANSPORT_ESP32, M1_RPC_TRANSPORT_ESP32));
    TEST_ASSERT_FALSE(m1_rpc_transport_file_write_start_allowed(
        true, M1_RPC_TRANSPORT_ESP32, M1_RPC_TRANSPORT_USB));
    TEST_ASSERT_TRUE(m1_rpc_transport_file_write_matches(
        true, M1_RPC_TRANSPORT_ESP32, M1_RPC_TRANSPORT_ESP32));
    TEST_ASSERT_FALSE(m1_rpc_transport_file_write_matches(
        true, M1_RPC_TRANSPORT_ESP32, M1_RPC_TRANSPORT_USB));
    TEST_ASSERT_TRUE(m1_rpc_transport_file_write_is_usb(
        true, M1_RPC_TRANSPORT_USB));
    TEST_ASSERT_FALSE(m1_rpc_transport_file_write_is_usb(
        true, M1_RPC_TRANSPORT_ESP32));
}

void test_rpc_poll_backoff_is_bounded_and_deadline_wraps(void)
{
    TEST_ASSERT_EQUAL_UINT32(500u, m1_rpc_poll_backoff_ms(0u));
    TEST_ASSERT_EQUAL_UINT32(1000u, m1_rpc_poll_backoff_ms(1u));
    TEST_ASSERT_EQUAL_UINT32(2000u, m1_rpc_poll_backoff_ms(2u));
    TEST_ASSERT_EQUAL_UINT32(4000u, m1_rpc_poll_backoff_ms(3u));
    TEST_ASSERT_EQUAL_UINT32(5000u, m1_rpc_poll_backoff_ms(4u));
    TEST_ASSERT_EQUAL_UINT32(5000u, m1_rpc_poll_backoff_ms(8u));
    TEST_ASSERT_FALSE(m1_rpc_poll_deadline_due(99u, 100u));
    TEST_ASSERT_TRUE(m1_rpc_poll_deadline_due(100u, 100u));
    TEST_ASSERT_TRUE(m1_rpc_poll_deadline_due(1u, UINT32_MAX));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_reply_route_matches_request_source);
    RUN_TEST(test_partial_frame_remains_owned_by_its_ingress_transport);
    RUN_TEST(test_reply_routing_contexts_are_independent);
    RUN_TEST(test_payload_limits_respect_relay_envelope);
    RUN_TEST(test_upload_transport_ownership_is_enforced);
    RUN_TEST(test_rpc_poll_backoff_is_bounded_and_deadline_wraps);
    return UNITY_END();
}
