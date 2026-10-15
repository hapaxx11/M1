/* See COPYING.txt for license details. */

#include "unity.h"

#include "m1_rpc_transport.h"

void setUp(void) {}
void tearDown(void) {}

void test_transport_can_send_only_when_its_callback_is_registered(void)
{
    const uint32_t registered =
        M1_RPC_TRANSPORT_BIT(M1_RPC_TRANSPORT_USB) |
        M1_RPC_TRANSPORT_BIT(M1_RPC_TRANSPORT_BLE);

    TEST_ASSERT_TRUE(m1_rpc_transport_can_send(M1_RPC_TRANSPORT_USB,
                                                registered));
    TEST_ASSERT_TRUE(m1_rpc_transport_can_send(M1_RPC_TRANSPORT_BLE,
                                                registered));
    TEST_ASSERT_FALSE(m1_rpc_transport_can_send(M1_RPC_TRANSPORT_WIFI,
                                                 registered));
    TEST_ASSERT_FALSE(m1_rpc_transport_can_send(M1_RPC_TRANSPORT_COUNT,
                                                 UINT32_MAX));
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
    RUN_TEST(test_transport_can_send_only_when_its_callback_is_registered);
    RUN_TEST(test_partial_frame_remains_owned_by_its_ingress_transport);
    return UNITY_END();
}
