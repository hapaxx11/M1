/* See COPYING.txt for license details. */

/**
 * test_wifi_deauth_cmd.c
 *
 * Unit tests for the pure-logic helpers in wifi_deauth_cmd.h / wifi_deauth_cmd.c:
 *   wifi_deauth_add_target()         — append one target to a CMD_DEAUTH_MULTI payload
 *   wifi_build_selected_deauth_cmd() — build a complete CMD_DEAUTH_MULTI from selected lists
 *
 * Build with the host-side CMake:
 *   cmake -B build-tests -S tests && cmake --build build-tests
 *   ctest --test-dir build-tests --output-on-failure
 */

#include "unity.h"
#include "wifi_deauth_cmd.h"
#include <string.h>

void setUp(void)    {}
void tearDown(void) {}

/* =========================================================================
 * Helpers
 * ========================================================================= */

static m1_cmd_t make_cmd(void)
{
    m1_cmd_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.magic    = M1_CMD_MAGIC;
    cmd.cmd_id   = CMD_DEAUTH_MULTI;
    cmd.payload_len = 1;
    return cmd;
}

static uint8_t nonzero_bssid[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
static uint8_t zero_bssid[6]    = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static uint8_t sta_mac[6]       = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};

/* Build a wifi_ap_t with the given selected flag, channel, and bssid */
static wifi_ap_t make_ap(bool selected, uint8_t channel, const uint8_t bssid[6])
{
    wifi_ap_t ap;
    memset(&ap, 0, sizeof(ap));
    ap.selected = selected;
    ap.channel  = channel;
    memcpy(ap.bssid, bssid, 6);
    return ap;
}

/* Build a wifi_sta_t with the given selected flag, channel, bssid, and mac */
static wifi_sta_t make_sta(bool selected, uint8_t channel,
                            const uint8_t bssid[6], const uint8_t mac[6])
{
    wifi_sta_t sta;
    memset(&sta, 0, sizeof(sta));
    sta.selected = selected;
    sta.channel  = channel;
    memcpy(sta.bssid, bssid, 6);
    memcpy(sta.mac,   mac,   6);
    return sta;
}

/* =========================================================================
 * wifi_deauth_add_target — no-op conditions
 * ========================================================================= */

static void test_add_target_null_bssid_is_noop(void)
{
    m1_cmd_t cmd = make_cmd();
    uint8_t ret = wifi_deauth_add_target(&cmd, 0, 0, 6, NULL, NULL);
    TEST_ASSERT_EQUAL_UINT8(0, ret);
    TEST_ASSERT_EQUAL_UINT8(1, cmd.payload_len);  /* unchanged */
}

static void test_add_target_zero_bssid_is_noop(void)
{
    m1_cmd_t cmd = make_cmd();
    uint8_t ret = wifi_deauth_add_target(&cmd, 0, 0, 6, zero_bssid, NULL);
    TEST_ASSERT_EQUAL_UINT8(0, ret);
    TEST_ASSERT_EQUAL_UINT8(1, cmd.payload_len);
}

static void test_add_target_zero_channel_is_noop(void)
{
    m1_cmd_t cmd = make_cmd();
    uint8_t ret = wifi_deauth_add_target(&cmd, 0, 0, 0, nonzero_bssid, NULL);
    TEST_ASSERT_EQUAL_UINT8(0, ret);
    TEST_ASSERT_EQUAL_UINT8(1, cmd.payload_len);
}

static void test_add_target_max_count_is_noop(void)
{
    m1_cmd_t cmd = make_cmd();
    uint8_t ret = wifi_deauth_add_target(&cmd, DEAUTH_MULTI_MAX_TARGETS, 0, 6, nonzero_bssid, NULL);
    TEST_ASSERT_EQUAL_UINT8(DEAUTH_MULTI_MAX_TARGETS, ret);
}

/* =========================================================================
 * wifi_deauth_add_target — successful append
 * ========================================================================= */

static void test_add_target_first_entry_increments_count(void)
{
    m1_cmd_t cmd = make_cmd();
    uint8_t ret = wifi_deauth_add_target(&cmd, 0, 0, 11, nonzero_bssid, NULL);
    TEST_ASSERT_EQUAL_UINT8(1, ret);
    TEST_ASSERT_EQUAL_UINT8(1, cmd.payload[0]);   /* count field */
}

static void test_add_target_payload_len_updated(void)
{
    m1_cmd_t cmd = make_cmd();
    wifi_deauth_add_target(&cmd, 0, 0, 6, nonzero_bssid, NULL);
    /* 1 (count byte) + 1 * DEAUTH_MULTI_TARGET_BYTES */
    TEST_ASSERT_EQUAL_UINT8(1 + DEAUTH_MULTI_TARGET_BYTES, cmd.payload_len);
}

static void test_add_target_mode_written(void)
{
    m1_cmd_t cmd = make_cmd();
    wifi_deauth_add_target(&cmd, 0, 1 /*mode=1*/, 6, nonzero_bssid, sta_mac);
    /* payload[1] = mode for first entry */
    TEST_ASSERT_EQUAL_UINT8(1, cmd.payload[1]);
}

static void test_add_target_channel_written(void)
{
    m1_cmd_t cmd = make_cmd();
    wifi_deauth_add_target(&cmd, 0, 0, 11, nonzero_bssid, NULL);
    /* payload[2] = channel for first entry */
    TEST_ASSERT_EQUAL_UINT8(11, cmd.payload[2]);
}

static void test_add_target_bssid_written(void)
{
    m1_cmd_t cmd = make_cmd();
    wifi_deauth_add_target(&cmd, 0, 0, 6, nonzero_bssid, NULL);
    /* payload[3..8] = bssid */
    TEST_ASSERT_EQUAL_MEMORY(nonzero_bssid, &cmd.payload[3], 6);
}

static void test_add_target_sta_mac_written_when_mode1(void)
{
    m1_cmd_t cmd = make_cmd();
    wifi_deauth_add_target(&cmd, 0, 1, 6, nonzero_bssid, sta_mac);
    /* payload[9..14] = sta mac */
    TEST_ASSERT_EQUAL_MEMORY(sta_mac, &cmd.payload[9], 6);
}

static void test_add_target_two_entries_second_offset(void)
{
    m1_cmd_t cmd = make_cmd();
    uint8_t bssid2[6] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC};
    wifi_deauth_add_target(&cmd, 0, 0, 6,  nonzero_bssid, NULL);
    wifi_deauth_add_target(&cmd, 1, 0, 11, bssid2,        NULL);
    TEST_ASSERT_EQUAL_UINT8(2, cmd.payload[0]);
    /* Second entry starts at 1 + 1*14 = 15 */
    TEST_ASSERT_EQUAL_UINT8(11, cmd.payload[16]);  /* channel of second entry */
    TEST_ASSERT_EQUAL_MEMORY(bssid2, &cmd.payload[17], 6);
}

/* =========================================================================
 * wifi_build_selected_deauth_cmd — empty / null inputs
 * ========================================================================= */

static void test_build_null_lists_returns_zero(void)
{
    m1_cmd_t cmd;
    uint8_t  ap_t, sta_t;
    uint16_t sel;
    uint8_t count = wifi_build_selected_deauth_cmd(&cmd, NULL, 0, NULL, 0,
                                                    &ap_t, &sta_t, &sel);
    TEST_ASSERT_EQUAL_UINT8(0, count);
    TEST_ASSERT_EQUAL_UINT8(M1_CMD_MAGIC,    cmd.magic);
    TEST_ASSERT_EQUAL_UINT8(CMD_DEAUTH_MULTI, cmd.cmd_id);
    TEST_ASSERT_EQUAL_UINT8(0, ap_t);
    TEST_ASSERT_EQUAL_UINT8(0, sta_t);
    TEST_ASSERT_EQUAL_UINT16(0, sel);
}

static void test_build_none_selected_returns_zero(void)
{
    wifi_ap_t  ap  = make_ap(false,  6, nonzero_bssid);
    wifi_sta_t sta = make_sta(false, 6, nonzero_bssid, sta_mac);
    m1_cmd_t cmd;
    uint8_t count = wifi_build_selected_deauth_cmd(&cmd, &ap, 1, &sta, 1,
                                                    NULL, NULL, NULL);
    TEST_ASSERT_EQUAL_UINT8(0, count);
}

/* =========================================================================
 * wifi_build_selected_deauth_cmd — AP targets
 * ========================================================================= */

static void test_build_one_ap_selected(void)
{
    wifi_ap_t ap = make_ap(true, 6, nonzero_bssid);
    m1_cmd_t cmd;
    uint8_t ap_t = 0, sta_t = 99;
    uint16_t sel = 0;
    uint8_t count = wifi_build_selected_deauth_cmd(&cmd, &ap, 1, NULL, 0,
                                                    &ap_t, &sta_t, &sel);
    TEST_ASSERT_EQUAL_UINT8(1, count);
    TEST_ASSERT_EQUAL_UINT8(1, ap_t);
    TEST_ASSERT_EQUAL_UINT8(0, sta_t);
    TEST_ASSERT_EQUAL_UINT16(1, sel);
}

static void test_build_ap_mode_is_zero(void)
{
    wifi_ap_t ap = make_ap(true, 6, nonzero_bssid);
    m1_cmd_t cmd;
    wifi_build_selected_deauth_cmd(&cmd, &ap, 1, NULL, 0, NULL, NULL, NULL);
    /* mode byte for first target = payload[1] */
    TEST_ASSERT_EQUAL_UINT8(0, cmd.payload[1]);
}

/* =========================================================================
 * wifi_build_selected_deauth_cmd — STA targets
 * ========================================================================= */

static void test_build_one_sta_selected(void)
{
    wifi_sta_t sta = make_sta(true, 11, nonzero_bssid, sta_mac);
    m1_cmd_t cmd;
    uint8_t ap_t = 99, sta_t = 0;
    uint8_t count = wifi_build_selected_deauth_cmd(&cmd, NULL, 0, &sta, 1,
                                                    &ap_t, &sta_t, NULL);
    TEST_ASSERT_EQUAL_UINT8(1, count);
    TEST_ASSERT_EQUAL_UINT8(0, ap_t);
    TEST_ASSERT_EQUAL_UINT8(1, sta_t);
}

static void test_build_sta_mode_is_one(void)
{
    wifi_sta_t sta = make_sta(true, 6, nonzero_bssid, sta_mac);
    m1_cmd_t cmd;
    wifi_build_selected_deauth_cmd(&cmd, NULL, 0, &sta, 1, NULL, NULL, NULL);
    TEST_ASSERT_EQUAL_UINT8(1, cmd.payload[1]);   /* mode byte = 1 for STA */
}

/* =========================================================================
 * wifi_build_selected_deauth_cmd — mixed AP + STA; cap at max
 * ========================================================================= */

static void test_build_capped_at_max_targets(void)
{
    /* 3 APs + 3 STAs = 6 entries, but max is DEAUTH_MULTI_MAX_TARGETS (4) */
    uint8_t b1[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x01};
    uint8_t b2[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x02};
    uint8_t b3[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x03};
    uint8_t m1[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x01};
    uint8_t m2[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x02};
    uint8_t m3[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x03};
    wifi_ap_t  aps[3];
    wifi_sta_t stas[3];
    aps[0]  = make_ap(true,  6, b1);
    aps[1]  = make_ap(true,  6, b2);
    aps[2]  = make_ap(true,  6, b3);
    stas[0] = make_sta(true, 6, b1, m1);
    stas[1] = make_sta(true, 6, b2, m2);
    stas[2] = make_sta(true, 6, b3, m3);

    m1_cmd_t cmd;
    uint8_t count = wifi_build_selected_deauth_cmd(&cmd, aps, 3, stas, 3,
                                                    NULL, NULL, NULL);
    TEST_ASSERT_EQUAL_UINT8(DEAUTH_MULTI_MAX_TARGETS, count);
    TEST_ASSERT_EQUAL_UINT8(DEAUTH_MULTI_MAX_TARGETS, cmd.payload[0]);
}

static void test_build_output_args_nullable(void)
{
    wifi_ap_t ap = make_ap(true, 6, nonzero_bssid);
    m1_cmd_t cmd;
    /* Should not crash with NULL out-pointers */
    wifi_build_selected_deauth_cmd(&cmd, &ap, 1, NULL, 0, NULL, NULL, NULL);
    TEST_ASSERT_EQUAL_UINT8(1, cmd.payload[0]);
}

/* =========================================================================
 * main
 * ========================================================================= */

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_add_target_null_bssid_is_noop);
    RUN_TEST(test_add_target_zero_bssid_is_noop);
    RUN_TEST(test_add_target_zero_channel_is_noop);
    RUN_TEST(test_add_target_max_count_is_noop);
    RUN_TEST(test_add_target_first_entry_increments_count);
    RUN_TEST(test_add_target_payload_len_updated);
    RUN_TEST(test_add_target_mode_written);
    RUN_TEST(test_add_target_channel_written);
    RUN_TEST(test_add_target_bssid_written);
    RUN_TEST(test_add_target_sta_mac_written_when_mode1);
    RUN_TEST(test_add_target_two_entries_second_offset);

    RUN_TEST(test_build_null_lists_returns_zero);
    RUN_TEST(test_build_none_selected_returns_zero);
    RUN_TEST(test_build_one_ap_selected);
    RUN_TEST(test_build_ap_mode_is_zero);
    RUN_TEST(test_build_one_sta_selected);
    RUN_TEST(test_build_sta_mode_is_one);
    RUN_TEST(test_build_capped_at_max_targets);
    RUN_TEST(test_build_output_args_nullable);

    return UNITY_END();
}
