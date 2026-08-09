/* See COPYING.txt for license details. */

/*
 * test_rpc_usb_epoch.c
 *
 * Regression tests for the USB session epoch decision logic (C3.154's
 * "epoch-reset NACK" fix): a host-side USB bus reset/disconnect must drop
 * STALE deferred-command / partial-file-write state left by the previous
 * session, while NEVER dropping work that legitimately arrived in the new
 * session.
 */

#include "unity.h"
#include "m1_rpc_usb_epoch.h"

void setUp(void) {}
void tearDown(void) {}

/* ── m1_rpc_usb_epoch_advanced() ── */

void test_epoch_advanced_first_call_matches_initial_zero(void)
{
    uint32_t last_seen = 0;
    /* Session epoch still 0 (no reset yet) -> no boundary crossed. */
    TEST_ASSERT_FALSE(m1_rpc_usb_epoch_advanced(&last_seen, 0));
    TEST_ASSERT_EQUAL_UINT32(0, last_seen);
}

void test_epoch_advanced_detects_bump_once(void)
{
    uint32_t last_seen = 0;
    TEST_ASSERT_TRUE(m1_rpc_usb_epoch_advanced(&last_seen, 1));
    TEST_ASSERT_EQUAL_UINT32(1, last_seen);

    /* Second consumer check at the same epoch must NOT re-trigger. */
    TEST_ASSERT_FALSE(m1_rpc_usb_epoch_advanced(&last_seen, 1));
    TEST_ASSERT_EQUAL_UINT32(1, last_seen);
}

void test_epoch_advanced_independent_consumers(void)
{
    /* Parser and rpc_task each track their own last-seen epoch and must
     * both independently observe the same bump exactly once. */
    uint32_t parser_epoch  = 0;
    uint32_t cleanup_epoch = 0;
    uint32_t session_epoch = 3;

    TEST_ASSERT_TRUE(m1_rpc_usb_epoch_advanced(&parser_epoch, session_epoch));
    TEST_ASSERT_TRUE(m1_rpc_usb_epoch_advanced(&cleanup_epoch, session_epoch));
    TEST_ASSERT_FALSE(m1_rpc_usb_epoch_advanced(&parser_epoch, session_epoch));
    TEST_ASSERT_FALSE(m1_rpc_usb_epoch_advanced(&cleanup_epoch, session_epoch));
}

void test_epoch_advanced_multiple_resets_before_consumption(void)
{
    /* Several resets can happen before the consumer next runs; only one
     * cleanup pass is needed once caught up to the latest epoch. */
    uint32_t last_seen = 0;
    TEST_ASSERT_TRUE(m1_rpc_usb_epoch_advanced(&last_seen, 5));
    TEST_ASSERT_EQUAL_UINT32(5, last_seen);
    TEST_ASSERT_FALSE(m1_rpc_usb_epoch_advanced(&last_seen, 5));
}

/* ── m1_rpc_usb_epoch_check() ── */

void test_check_no_deferred_no_file_write_is_noop(void)
{
    S_RpcUsbEpochAction action = m1_rpc_usb_epoch_check(1, false, false, 0, false);
    TEST_ASSERT_FALSE(action.discard_deferred);
    TEST_ASSERT_FALSE(action.close_file_write);
}

void test_check_stale_deferred_command_is_discarded(void)
{
    /* Deferred command tagged with epoch 0, but the session moved to 1:
     * it belongs to the OLD session and must be discarded (NACKed). */
    S_RpcUsbEpochAction action = m1_rpc_usb_epoch_check(1, true, true, 0, false);
    TEST_ASSERT_TRUE(action.discard_deferred);
}

void test_check_current_epoch_deferred_command_is_kept(void)
{
    /* Regression guard for the exact bug C3.154 fixed on the OTHER side:
     * a command that arrived in the CURRENT (new) session must NOT be
     * treated as stale and dropped, even though a session boundary was
     * just crossed. */
    S_RpcUsbEpochAction action = m1_rpc_usb_epoch_check(1, true, true, 1, false);
    TEST_ASSERT_FALSE(action.discard_deferred);
}

void test_check_no_pending_deferred_is_never_discarded(void)
{
    S_RpcUsbEpochAction action = m1_rpc_usb_epoch_check(1, false, true, 0, false);
    TEST_ASSERT_FALSE(action.discard_deferred);
}

void test_check_open_file_write_always_closed_at_boundary(void)
{
    S_RpcUsbEpochAction action = m1_rpc_usb_epoch_check(1, false, false, 0, true);
    TEST_ASSERT_TRUE(action.close_file_write);
}

void test_check_combined_stale_deferred_and_open_file(void)
{
    S_RpcUsbEpochAction action = m1_rpc_usb_epoch_check(2, true, true, 0, true);
    TEST_ASSERT_TRUE(action.discard_deferred);
    TEST_ASSERT_TRUE(action.close_file_write);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_epoch_advanced_first_call_matches_initial_zero);
    RUN_TEST(test_epoch_advanced_detects_bump_once);
    RUN_TEST(test_epoch_advanced_independent_consumers);
    RUN_TEST(test_epoch_advanced_multiple_resets_before_consumption);
    RUN_TEST(test_check_no_deferred_no_file_write_is_noop);
    RUN_TEST(test_check_stale_deferred_command_is_discarded);
    RUN_TEST(test_check_current_epoch_deferred_command_is_kept);
    RUN_TEST(test_check_no_pending_deferred_is_never_discarded);
    RUN_TEST(test_check_open_file_write_always_closed_at_boundary);
    RUN_TEST(test_check_combined_stale_deferred_and_open_file);
    return UNITY_END();
}
