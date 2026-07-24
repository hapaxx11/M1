/* See COPYING.txt for license details. */

/**
 * @file   test_espnow_peer_session.c
 * @brief  Host-side unit tests for the ESP-NOW peer session state machine.
 *
 * Pure logic — no stubs required.
 */

#include "unity.h"
#include "espnow_peer_session.h"

#include <string.h>

void setUp(void)  {}
void tearDown(void) {}

/* =========================================================================
 * Helpers
 * =========================================================================*/

static const uint8_t MAC_A[ESPNOW_MAC_LEN] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x01};
static const uint8_t MAC_B[ESPNOW_MAC_LEN] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x02};

static espnow_session_t s_session;

static void init_session(void)
{
    espnow_session_init(&s_session, "TestDevice", MAC_A, 6);
}

static void add_one_peer(void)
{
    espnow_peer_info_t peer;
    memcpy(peer.mac, MAC_B, ESPNOW_MAC_LEN);
    peer.rssi = -45;
    peer.channel = 6;
    strcpy(peer.name, "PeerDev");
    espnow_session_update_peers(&s_session, &peer, 1);
}

/* =========================================================================
 * Init tests
 * =========================================================================*/

void test_init_sets_idle_state(void)
{
    init_session();
    TEST_ASSERT_EQUAL(ESPNOW_STATE_IDLE, s_session.state);
}

void test_init_copies_name(void)
{
    init_session();
    TEST_ASSERT_EQUAL_STRING("TestDevice", s_session.our_name);
}

void test_init_copies_mac(void)
{
    init_session();
    TEST_ASSERT_EQUAL_MEMORY(MAC_A, s_session.our_mac, ESPNOW_MAC_LEN);
}

void test_init_sets_channel(void)
{
    init_session();
    TEST_ASSERT_EQUAL_UINT8(6, s_session.our_channel);
}

void test_init_truncates_long_name(void)
{
    espnow_session_t s;
    espnow_session_init(&s, "ThisNameIsWayTooLongForTheBuffer!!", MAC_A, 1);
    TEST_ASSERT_EQUAL_UINT(ESPNOW_NAME_MAX, strlen(s.our_name));
}

/* =========================================================================
 * State transition tests
 * =========================================================================*/

void test_start_scan_from_idle(void)
{
    init_session();
    TEST_ASSERT_TRUE(espnow_session_start_scan(&s_session));
    TEST_ASSERT_EQUAL(ESPNOW_STATE_SCANNING, s_session.state);
}

void test_start_scan_from_non_idle_fails(void)
{
    init_session();
    espnow_session_start_scan(&s_session);
    TEST_ASSERT_FALSE(espnow_session_start_scan(&s_session));
}

void test_select_peer_from_scanning(void)
{
    init_session();
    espnow_session_start_scan(&s_session);
    add_one_peer();
    TEST_ASSERT_TRUE(espnow_session_select_peer(&s_session, 0));
    TEST_ASSERT_EQUAL(ESPNOW_STATE_PEER_FOUND, s_session.state);
}

void test_select_peer_invalid_index_fails(void)
{
    init_session();
    espnow_session_start_scan(&s_session);
    add_one_peer();
    TEST_ASSERT_FALSE(espnow_session_select_peer(&s_session, 5));
}

void test_pair_request_sent_from_peer_found(void)
{
    init_session();
    espnow_session_start_scan(&s_session);
    add_one_peer();
    espnow_session_select_peer(&s_session, 0);
    TEST_ASSERT_TRUE(espnow_session_pair_request_sent(&s_session));
    TEST_ASSERT_EQUAL(ESPNOW_STATE_PAIR_SENT, s_session.state);
}

void test_pair_accepted_from_pair_sent(void)
{
    init_session();
    espnow_session_start_scan(&s_session);
    add_one_peer();
    espnow_session_select_peer(&s_session, 0);
    espnow_session_pair_request_sent(&s_session);
    TEST_ASSERT_TRUE(espnow_session_pair_accepted(&s_session));
    TEST_ASSERT_EQUAL(ESPNOW_STATE_PAIRED, s_session.state);
}

void test_pair_rejected_from_pair_sent(void)
{
    init_session();
    espnow_session_start_scan(&s_session);
    add_one_peer();
    espnow_session_select_peer(&s_session, 0);
    espnow_session_pair_request_sent(&s_session);
    TEST_ASSERT_TRUE(espnow_session_pair_rejected(&s_session));
    TEST_ASSERT_EQUAL(ESPNOW_STATE_PAIR_REJECTED, s_session.state);
}

void test_ack_rejection_returns_to_scanning(void)
{
    init_session();
    espnow_session_start_scan(&s_session);
    add_one_peer();
    espnow_session_select_peer(&s_session, 0);
    espnow_session_pair_request_sent(&s_session);
    espnow_session_pair_rejected(&s_session);
    TEST_ASSERT_TRUE(espnow_session_ack_rejection(&s_session));
    TEST_ASSERT_EQUAL(ESPNOW_STATE_SCANNING, s_session.state);
}

void test_stop_from_any_state(void)
{
    init_session();
    espnow_session_start_scan(&s_session);
    add_one_peer();
    espnow_session_select_peer(&s_session, 0);
    espnow_session_stop(&s_session);
    TEST_ASSERT_EQUAL(ESPNOW_STATE_IDLE, s_session.state);
}

/* =========================================================================
 * Peer update tests
 * =========================================================================*/

void test_update_peers_adds_new_peer(void)
{
    init_session();
    espnow_session_start_scan(&s_session);
    add_one_peer();
    TEST_ASSERT_EQUAL_UINT8(1, s_session.peer_count);
    TEST_ASSERT_EQUAL_MEMORY(MAC_B, s_session.peers[0].mac, ESPNOW_MAC_LEN);
    TEST_ASSERT_EQUAL_STRING("PeerDev", s_session.peers[0].name);
}

void test_update_peers_merges_existing(void)
{
    init_session();
    espnow_session_start_scan(&s_session);
    add_one_peer();

    /* Update with same MAC, different RSSI */
    espnow_peer_info_t peer;
    memcpy(peer.mac, MAC_B, ESPNOW_MAC_LEN);
    peer.rssi = -30;
    peer.channel = 6;
    strcpy(peer.name, "PeerDev");
    espnow_session_update_peers(&s_session, &peer, 1);

    TEST_ASSERT_EQUAL_UINT8(1, s_session.peer_count);
    TEST_ASSERT_EQUAL_INT8(-30, s_session.peers[0].rssi);
}

void test_update_peers_max_limit(void)
{
    init_session();
    espnow_session_start_scan(&s_session);

    /* Add max peers */
    for (int i = 0; i < ESPNOW_MAX_PEERS + 2; i++) {
        espnow_peer_info_t peer = {0};
        peer.mac[0] = (uint8_t)i;
        peer.rssi = -50;
        peer.channel = 1;
        snprintf(peer.name, ESPNOW_NAME_MAX + 1, "P%d", i);
        espnow_session_update_peers(&s_session, &peer, 1);
    }
    TEST_ASSERT_EQUAL_UINT8(ESPNOW_MAX_PEERS, s_session.peer_count);
}

/* =========================================================================
 * Confirm code tests
 * =========================================================================*/

void test_confirm_code_deterministic(void)
{
    uint16_t code1 = espnow_compute_confirm_code(MAC_A, MAC_B);
    uint16_t code2 = espnow_compute_confirm_code(MAC_A, MAC_B);
    TEST_ASSERT_EQUAL_UINT16(code1, code2);
}

void test_confirm_code_order_matters(void)
{
    uint16_t code_ab = espnow_compute_confirm_code(MAC_A, MAC_B);
    uint16_t code_ba = espnow_compute_confirm_code(MAC_B, MAC_A);
    /* Different order should produce a different code (very likely) */
    TEST_ASSERT_NOT_EQUAL(code_ab, code_ba);
}

void test_confirm_code_range(void)
{
    uint16_t code = espnow_compute_confirm_code(MAC_A, MAC_B);
    TEST_ASSERT_LESS_THAN_UINT16(10000, code);
}

void test_pair_accepted_sets_confirm_code(void)
{
    init_session();
    espnow_session_start_scan(&s_session);
    add_one_peer();
    espnow_session_select_peer(&s_session, 0);
    espnow_session_pair_request_sent(&s_session);
    espnow_session_pair_accepted(&s_session);

    uint16_t expected = espnow_compute_confirm_code(MAC_A, MAC_B);
    TEST_ASSERT_EQUAL_UINT16(expected, s_session.confirm_code);
}

/* =========================================================================
 * Invalid state transition tests
 * =========================================================================*/

void test_pair_request_sent_from_idle_fails(void)
{
    init_session();
    TEST_ASSERT_FALSE(espnow_session_pair_request_sent(&s_session));
}

void test_pair_accepted_from_idle_fails(void)
{
    init_session();
    TEST_ASSERT_FALSE(espnow_session_pair_accepted(&s_session));
}

void test_ack_rejection_from_scanning_fails(void)
{
    init_session();
    espnow_session_start_scan(&s_session);
    TEST_ASSERT_FALSE(espnow_session_ack_rejection(&s_session));
}

/* =========================================================================
 * Test runner
 * =========================================================================*/

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_init_sets_idle_state);
    RUN_TEST(test_init_copies_name);
    RUN_TEST(test_init_copies_mac);
    RUN_TEST(test_init_sets_channel);
    RUN_TEST(test_init_truncates_long_name);

    RUN_TEST(test_start_scan_from_idle);
    RUN_TEST(test_start_scan_from_non_idle_fails);
    RUN_TEST(test_select_peer_from_scanning);
    RUN_TEST(test_select_peer_invalid_index_fails);
    RUN_TEST(test_pair_request_sent_from_peer_found);
    RUN_TEST(test_pair_accepted_from_pair_sent);
    RUN_TEST(test_pair_rejected_from_pair_sent);
    RUN_TEST(test_ack_rejection_returns_to_scanning);
    RUN_TEST(test_stop_from_any_state);

    RUN_TEST(test_update_peers_adds_new_peer);
    RUN_TEST(test_update_peers_merges_existing);
    RUN_TEST(test_update_peers_max_limit);

    RUN_TEST(test_confirm_code_deterministic);
    RUN_TEST(test_confirm_code_order_matters);
    RUN_TEST(test_confirm_code_range);
    RUN_TEST(test_pair_accepted_sets_confirm_code);

    RUN_TEST(test_pair_request_sent_from_idle_fails);
    RUN_TEST(test_pair_accepted_from_idle_fails);
    RUN_TEST(test_ack_rejection_from_scanning_fails);

    return UNITY_END();
}
