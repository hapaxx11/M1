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
    TEST_ASSERT_FALSE(m1_rpc_transport_feed_allowed(
        true, M1_RPC_TRANSPORT_USB, M1_RPC_TRANSPORT_BLE));
    TEST_ASSERT_FALSE(m1_rpc_transport_feed_allowed(
        true, M1_RPC_TRANSPORT_USB, M1_RPC_TRANSPORT_COUNT));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_reply_route_matches_request_source);
    RUN_TEST(test_partial_frame_remains_owned_by_its_ingress_transport);
    return UNITY_END();
}
