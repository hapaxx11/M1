/* See COPYING.txt for license details. */

/*
 * test_esp32_rpc_features.c — host tests for the per-feature M1_RPC action
 * layer (m1_esp32_rpc_features.c) and the feature -> opcode map.
 *
 * A fake transport captures the outgoing REQ frame (so we can assert the opcode
 * and payload each wrapper builds) and replays a canned RESP/NAK frame, letting
 * every wrapper run without an ESP32.
 */

#include <string.h>
#include "unity.h"
#include "m1_esp32_rpc_features.h"

/* ------------------------------------------------------------------ */
/* Link stubs for the on-target symbols m1_esp32_rpc.c references.    */
/* ------------------------------------------------------------------ */

static uint64_t g_bitmap;
uint64_t m1_esp32_caps_get_bitmap(void) { return g_bitmap; }

/* This module never exercises m1_esp32_active_transport(), so pretend the
 * bitmap is already queried -- it just needs to link. */
bool m1_esp32_caps_is_queried(void) { return true; }
void m1_esp32_caps_init(void) {}
uint8_t m1_esp32_get_init_status(void) { return 1u; }

uint8_t spi_AT_send_recv_bin(const uint8_t *tx_buf, int tx_len,
                             uint8_t *rx_buf, int rx_buf_size,
                             int *out_len, int timeout_sec)
{
    (void)tx_buf; (void)tx_len; (void)rx_buf; (void)rx_buf_size;
    (void)timeout_sec;
    if (out_len) *out_len = 0;
    return 1;
}

uint8_t spi_m1link_send_recv_bin(const uint8_t *tx_buf, int tx_len,
                                 uint8_t *rx_buf, int rx_buf_size,
                                 int *out_len, int timeout_sec)
{
    (void)tx_buf; (void)tx_len; (void)rx_buf; (void)rx_buf_size;
    (void)timeout_sec;
    if (out_len) *out_len = 0;
    return 1;
}

/* ------------------------------------------------------------------ */
/* Fake transport: captures TX, replays a canned frame.               */
/* ------------------------------------------------------------------ */

static uint8_t g_canned[2048];
static int     g_canned_len;
static uint8_t g_ret;
static uint8_t g_last_tx[256];
static int     g_last_tx_len;
static int     g_last_timeout_sec;

static uint8_t fake_transport(const uint8_t *tx_buf, int tx_len,
                              uint8_t *rx_buf, int rx_buf_size,
                              int *out_len, int timeout_sec)
{
    g_last_timeout_sec = timeout_sec;
    g_last_tx_len = (tx_len < (int)sizeof(g_last_tx)) ? tx_len
                                                      : (int)sizeof(g_last_tx);
    memcpy(g_last_tx, tx_buf, (size_t)g_last_tx_len);

    if (g_ret != 0) {
        if (out_len) *out_len = 0;
        return g_ret;
    }
    int n = (g_canned_len < rx_buf_size) ? g_canned_len : rx_buf_size;
    memcpy(rx_buf, g_canned, (size_t)n);
    if (out_len) *out_len = n;
    return 0;
}

/* Build an M1_RPC frame of any msg_type (the REQ builder is REQ-only). */
static int make_frame(uint8_t *buf, uint8_t msg_type, uint16_t msg_id,
                      const uint8_t *payload, uint16_t plen)
{
    buf[0] = (uint8_t)(M1_ESP32_RPC_MAGIC & 0xFFu);
    buf[1] = (uint8_t)((M1_ESP32_RPC_MAGIC >> 8u) & 0xFFu);
    buf[2] = M1_ESP32_RPC_VERSION;
    buf[3] = msg_type;
    buf[4] = (uint8_t)(msg_id & 0xFFu);
    buf[5] = (uint8_t)((msg_id >> 8u) & 0xFFu);
    buf[6] = (uint8_t)(plen & 0xFFu);
    buf[7] = (uint8_t)((plen >> 8u) & 0xFFu);
    if (payload && plen)
        memcpy(buf + M1_ESP32_RPC_HDR_SIZE, payload, plen);
    uint16_t crc = m1_esp32_rpc_crc16(buf, M1_ESP32_RPC_HDR_SIZE + plen);
    buf[M1_ESP32_RPC_HDR_SIZE + plen]      = (uint8_t)(crc & 0xFFu);
    buf[M1_ESP32_RPC_HDR_SIZE + plen + 1u] = (uint8_t)((crc >> 8u) & 0xFFu);
    return (int)(M1_ESP32_RPC_HDR_SIZE + plen + M1_ESP32_RPC_CRC_SIZE);
}

/* Canned RESP with an empty body. */
static void canned_ok(uint16_t msg_id)
{
    g_canned_len = make_frame(g_canned, M1_ESP32_RPC_RESP, msg_id, NULL, 0u);
}

/* Accessors for the captured request frame. */
static uint16_t tx_msg_id(void)
{
    return (uint16_t)(g_last_tx[4] | ((uint16_t)g_last_tx[5] << 8));
}
static uint16_t tx_plen(void)
{
    return (uint16_t)(g_last_tx[6] | ((uint16_t)g_last_tx[7] << 8));
}
static const uint8_t *tx_payload(void)
{
    return &g_last_tx[M1_ESP32_RPC_HDR_SIZE];
}

void setUp(void)
{
    g_bitmap = 0u;
    g_canned_len = 0;
    g_ret = 0;
    g_last_tx_len = 0;
    memset(g_canned, 0, sizeof(g_canned));
    memset(g_last_tx, 0, sizeof(g_last_tx));
    m1_esp32_rpc_set_transport(fake_transport);
}

void tearDown(void)
{
    m1_esp32_rpc_set_transport(NULL);
}

/* ================================================================== */
/* Feature -> opcode map                                              */
/* ================================================================== */

void test_opcode_map_known_features(void)
{
    m1_esp32_rpc_id_t op;

    TEST_ASSERT_TRUE(esp32_feature_rpc_opcode(ESP32_FEATURE_WIFI_SCAN, &op));
    TEST_ASSERT_EQUAL_HEX16(M1_ESP32_RPC_WIFI_SCAN, op);

    TEST_ASSERT_TRUE(esp32_feature_rpc_opcode(ESP32_FEATURE_DEAUTH, &op));
    TEST_ASSERT_EQUAL_HEX16(M1_ESP32_RPC_OFF_DEAUTH_START, op);

    TEST_ASSERT_TRUE(esp32_feature_rpc_opcode(ESP32_FEATURE_BLE_HID, &op));
    TEST_ASSERT_EQUAL_HEX16(M1_ESP32_RPC_BLE_HID_INIT, op);

    TEST_ASSERT_TRUE(esp32_feature_rpc_opcode(ESP32_FEATURE_802154, &op));
    TEST_ASSERT_EQUAL_HEX16(M1_ESP32_RPC_ZB_SNIFF_START, op);

    TEST_ASSERT_TRUE(esp32_feature_rpc_opcode(ESP32_FEATURE_ESPNOW, &op));
    TEST_ASSERT_EQUAL_HEX16(M1_ESP32_RPC_NOW_START, op);

    TEST_ASSERT_TRUE(esp32_feature_rpc_opcode(ESP32_FEATURE_WIFI_HOTSPOT, &op));
    TEST_ASSERT_EQUAL_HEX16(M1_ESP32_RPC_SOFTAP_START, op);
}

void test_opcode_map_every_id_resolves_or_declines(void)
{
    /* No feature id may crash the map; each either resolves or returns false. */
    for (int i = 0; i < (int)ESP32_FEATURE_COUNT; i++) {
        m1_esp32_rpc_id_t op = (m1_esp32_rpc_id_t)0;
        bool ok = esp32_feature_rpc_opcode((esp32_feature_id_t)i, &op);
        if (ok)
            TEST_ASSERT_NOT_EQUAL(0, (int)op);
    }
}

void test_opcode_map_unmapped_and_oob(void)
{
    m1_esp32_rpc_id_t op = (m1_esp32_rpc_id_t)0xABCD;

    TEST_ASSERT_FALSE(esp32_feature_rpc_opcode(ESP32_FEATURE_NETSCAN, &op));
    TEST_ASSERT_FALSE(esp32_feature_rpc_opcode(ESP32_FEATURE_BLE_GATT, &op));
    TEST_ASSERT_FALSE(esp32_feature_rpc_opcode(ESP32_FEATURE_BT_MANAGE, &op));
    TEST_ASSERT_FALSE(esp32_feature_rpc_opcode(ESP32_FEATURE_COUNT, &op));
    /* out param untouched when no mapping */
    TEST_ASSERT_EQUAL_HEX16(0xABCD, op);

    /* NULL out param is allowed (existence query). */
    TEST_ASSERT_TRUE(esp32_feature_rpc_opcode(ESP32_FEATURE_WIFI_SCAN, NULL));
}

/* ================================================================== */
/* No-payload triggers                                                */
/* ================================================================== */

void test_trigger_builds_correct_opcode_and_empty_payload(void)
{
    canned_ok(M1_ESP32_RPC_OFF_DEAUTH_STOP);
    TEST_ASSERT_EQUAL(M1_ESP32_RPC_OK, m1_esp32_rpc_deauth_stop());
    TEST_ASSERT_EQUAL_HEX16(M1_ESP32_RPC_OFF_DEAUTH_STOP, tx_msg_id());
    TEST_ASSERT_EQUAL_UINT16(0u, tx_plen());
}

void test_trigger_variants_route_expected_opcodes(void)
{
    struct { m1_esp32_rpc_status_t (*fn)(void); uint16_t op; } cases[] = {
        { m1_esp32_rpc_wifi_disconnect, M1_ESP32_RPC_WIFI_DISCONNECT },
        { m1_esp32_rpc_softap_stop,     M1_ESP32_RPC_SOFTAP_STOP     },
        { m1_esp32_rpc_monitor_stop,    M1_ESP32_RPC_OFF_MONITOR_STOP},
        { m1_esp32_rpc_beacon_stop,     M1_ESP32_RPC_OFF_BEACON_STOP },
        { m1_esp32_rpc_probe_stop,      M1_ESP32_RPC_OFF_PROBE_STOP  },
        { m1_esp32_rpc_karma_stop,      M1_ESP32_RPC_OFF_KARMA_STOP  },
        { m1_esp32_rpc_captive_stop,    M1_ESP32_RPC_OFF_CAPTIVE_STOP},
        { m1_esp32_rpc_handshake_stop,  M1_ESP32_RPC_OFF_HS_STOP     },
        { m1_esp32_rpc_ble_init,        M1_ESP32_RPC_BLE_INIT        },
        { m1_esp32_rpc_ble_adv_stop,    M1_ESP32_RPC_BLE_ADV_STOP    },
        { m1_esp32_rpc_ble_spam_stop,   M1_ESP32_RPC_BLE_SPAM_STOP   },
        { m1_esp32_rpc_ble_hid_deinit,  M1_ESP32_RPC_BLE_HID_DEINIT  },
        { m1_esp32_rpc_zb_init,         M1_ESP32_RPC_ZB_INIT         },
        { m1_esp32_rpc_zb_sniff_stop,   M1_ESP32_RPC_ZB_SNIFF_STOP   },
        { m1_esp32_rpc_zb_flood_stop,   M1_ESP32_RPC_ZB_FLOOD_STOP   },
    };
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        canned_ok(cases[i].op);
        TEST_ASSERT_EQUAL(M1_ESP32_RPC_OK, cases[i].fn());
        TEST_ASSERT_EQUAL_HEX16(cases[i].op, tx_msg_id());
        TEST_ASSERT_EQUAL_UINT16(0u, tx_plen());
    }
}

void test_trigger_propagates_nak(void)
{
    const uint8_t body[] = { M1_ESP32_RPC_ERR_BUSY };
    g_canned_len = make_frame(g_canned, M1_ESP32_RPC_NAK,
                              M1_ESP32_RPC_OFF_MONITOR_STOP, body, sizeof(body));
    TEST_ASSERT_EQUAL(M1_ESP32_RPC_ERR_BUSY, m1_esp32_rpc_monitor_stop());
}

void test_trigger_transport_failure(void)
{
    g_ret = 1; /* transport error */
    TEST_ASSERT_EQUAL(M1_ESP32_RPC_ERR_TRANSPORT, m1_esp32_rpc_zb_sniff_stop());
}

/* ================================================================== */
/* Scalar-payload commands                                            */
/* ================================================================== */

void test_ble_scan_start_encodes_dur_s(void)
{
    /* Wire format: [dur_lo:1][dur_hi:1] u16 LE */
    canned_ok(M1_ESP32_RPC_BLE_SCAN_START);
    TEST_ASSERT_EQUAL(M1_ESP32_RPC_OK, m1_esp32_rpc_ble_scan_start(300u));
    TEST_ASSERT_EQUAL_HEX16(M1_ESP32_RPC_BLE_SCAN_START, tx_msg_id());
    TEST_ASSERT_EQUAL_UINT16(2u, tx_plen());
    TEST_ASSERT_EQUAL_UINT8(0x2Cu, tx_payload()[0]);   /* 300 = 0x012C lo */
    TEST_ASSERT_EQUAL_UINT8(0x01u, tx_payload()[1]);   /* 300 = 0x012C hi */
}

void test_channel_commands_encode_single_byte(void)
{
    struct { m1_esp32_rpc_status_t (*fn)(uint8_t); uint16_t op; } cases[] = {
        { m1_esp32_rpc_monitor_start,   M1_ESP32_RPC_OFF_MONITOR_START },
        { m1_esp32_rpc_zb_sniff_start,  M1_ESP32_RPC_ZB_SNIFF_START    },
        { m1_esp32_rpc_zb_flood_start,  M1_ESP32_RPC_ZB_FLOOD_START    },
    };
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        canned_ok(cases[i].op);
        TEST_ASSERT_EQUAL(M1_ESP32_RPC_OK, cases[i].fn(17));
        TEST_ASSERT_EQUAL_HEX16(cases[i].op, tx_msg_id());
        TEST_ASSERT_EQUAL_UINT16(1u, tx_plen());
        TEST_ASSERT_EQUAL_UINT8(17u, tx_payload()[0]);
    }
}

void test_set_mac_encodes_six_bytes(void)
{
    const uint8_t mac[6] = { 0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02 };
    canned_ok(M1_ESP32_RPC_WIFI_GET_MAC);
    TEST_ASSERT_EQUAL(M1_ESP32_RPC_OK, m1_esp32_rpc_wifi_set_mac(mac));
    TEST_ASSERT_EQUAL_HEX16(M1_ESP32_RPC_WIFI_GET_MAC, tx_msg_id());
    TEST_ASSERT_EQUAL_UINT16(6u, tx_plen());
    TEST_ASSERT_EQUAL_UINT8_ARRAY(mac, tx_payload(), 6);
}

void test_set_mac_null_rejected(void)
{
    TEST_ASSERT_EQUAL(M1_ESP32_RPC_ERR_INVALID, m1_esp32_rpc_wifi_set_mac(NULL));
}

void test_monitor_read_decodes_frame(void)
{
    /* Wire format: [ch:1][rssi:i8][len:2 LE][frame bytes] */
    static const uint8_t frame[] = { 0x80, 0x00, 0x01, 0x02, 0x03, 0x04 };
    uint8_t body[16];
    body[0] = 6u;
    body[1] = (uint8_t)(-55);
    body[2] = (uint8_t)(sizeof(frame) & 0xFFu);
    body[3] = (uint8_t)((sizeof(frame) >> 8u) & 0xFFu);
    memcpy(&body[4], frame, sizeof(frame));

    g_canned_len = make_frame(g_canned, M1_ESP32_RPC_RESP,
                              M1_ESP32_RPC_OFF_MONITOR_READ,
                              body, (uint16_t)(4u + sizeof(frame)));

    uint8_t out_frame[16];
    uint16_t out_len = 0u;
    uint8_t ch = 0u;
    int8_t rssi = 0;
    TEST_ASSERT_EQUAL(M1_ESP32_RPC_OK,
                      m1_esp32_rpc_monitor_read(out_frame, sizeof(out_frame),
                                                &out_len, &ch, &rssi));
    TEST_ASSERT_EQUAL_UINT16(sizeof(frame), out_len);
    TEST_ASSERT_EQUAL_UINT8(6u, ch);
    TEST_ASSERT_EQUAL_INT8(-55, rssi);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(frame, out_frame, sizeof(frame));
    TEST_ASSERT_EQUAL_HEX16(M1_ESP32_RPC_OFF_MONITOR_READ, tx_msg_id());
    TEST_ASSERT_EQUAL_UINT16(0u, tx_plen());
}

void test_monitor_read_empty_response_is_ok(void)
{
    g_canned_len = make_frame(g_canned, M1_ESP32_RPC_RESP,
                              M1_ESP32_RPC_OFF_MONITOR_READ, NULL, 0u);

    uint16_t out_len = 0xABCDu;
    uint8_t ch = 0xFFu;
    int8_t rssi = 0x7F;
    TEST_ASSERT_EQUAL(M1_ESP32_RPC_OK,
                      m1_esp32_rpc_monitor_read(NULL, 0u, &out_len, &ch, &rssi));
    TEST_ASSERT_EQUAL_UINT16(0u, out_len);
    TEST_ASSERT_EQUAL_UINT8(0u, ch);
    TEST_ASSERT_EQUAL_INT8(0, rssi);
}

void test_monitor_read_truncated_header_rejects(void)
{
    const uint8_t body[] = { 6u, (uint8_t)(-55) }; /* missing len */
    g_canned_len = make_frame(g_canned, M1_ESP32_RPC_RESP,
                              M1_ESP32_RPC_OFF_MONITOR_READ, body, sizeof(body));

    uint8_t out_frame[8];
    TEST_ASSERT_EQUAL(M1_ESP32_RPC_ERR_BAD_FRAME,
                      m1_esp32_rpc_monitor_read(out_frame, sizeof(out_frame),
                                                NULL, NULL, NULL));
}

void test_monitor_read_oversized_len_rejects(void)
{
    /* frame_len claims 0x0100 bytes but the response only carries 4 payload
     * bytes after the 4-byte header. */
    uint8_t body[8] = { 6u, (uint8_t)(-55), 0x00, 0x01, 1, 2, 3, 4 };
    g_canned_len = make_frame(g_canned, M1_ESP32_RPC_RESP,
                              M1_ESP32_RPC_OFF_MONITOR_READ, body, sizeof(body));

    uint8_t out_frame[8];
    TEST_ASSERT_EQUAL(M1_ESP32_RPC_ERR_BAD_FRAME,
                      m1_esp32_rpc_monitor_read(out_frame, sizeof(out_frame),
                                                NULL, NULL, NULL));
}

void test_monitor_read_truncates_to_frame_max_and_reports_copied_len(void)
{
    static const uint8_t frame[] = { 0x80, 0x00, 0x01, 0x02, 0x03, 0x04 };
    uint8_t body[16];
    body[0] = 11u;
    body[1] = (uint8_t)(-42);
    body[2] = (uint8_t)(sizeof(frame) & 0xFFu);
    body[3] = (uint8_t)((sizeof(frame) >> 8u) & 0xFFu);
    memcpy(&body[4], frame, sizeof(frame));

    g_canned_len = make_frame(g_canned, M1_ESP32_RPC_RESP,
                              M1_ESP32_RPC_OFF_MONITOR_READ,
                              body, (uint16_t)(4u + sizeof(frame)));

    uint8_t out_frame[4] = { 0 };
    uint16_t out_len = 0u;
    uint8_t ch = 0u;
    int8_t rssi = 0;
    TEST_ASSERT_EQUAL(M1_ESP32_RPC_OK,
                      m1_esp32_rpc_monitor_read(out_frame, sizeof(out_frame),
                                                &out_len, &ch, &rssi));
    TEST_ASSERT_EQUAL_UINT16(sizeof(out_frame), out_len);
    TEST_ASSERT_EQUAL_UINT8(11u, ch);
    TEST_ASSERT_EQUAL_INT8(-42, rssi);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(frame, out_frame, sizeof(out_frame));
}

void test_ble_adv_start_encodes_name(void)
{
    const char name[] = "M1Adv";
    canned_ok(M1_ESP32_RPC_BLE_ADV_START);
    TEST_ASSERT_EQUAL(M1_ESP32_RPC_OK, m1_esp32_rpc_ble_adv_start(name));
    TEST_ASSERT_EQUAL_HEX16(M1_ESP32_RPC_BLE_ADV_START, tx_msg_id());
    TEST_ASSERT_EQUAL_UINT16(strlen(name), tx_plen());
    TEST_ASSERT_EQUAL_UINT8_ARRAY((const uint8_t *)name, tx_payload(), strlen(name));
}

void test_ble_adv_start_null_name_empty_payload(void)
{
    canned_ok(M1_ESP32_RPC_BLE_ADV_START);
    TEST_ASSERT_EQUAL(M1_ESP32_RPC_OK, m1_esp32_rpc_ble_adv_start(NULL));
    TEST_ASSERT_EQUAL_UINT16(0u, tx_plen());
}

void test_beacon_start_caps_payload_and_patches_count(void)
{
    char ssids[8][33];
    for (uint8_t i = 0u; i < 8u; i++) {
        memset(ssids[i], 'A' + (char)i, 32u);
        ssids[i][32] = '\0';
    }

    canned_ok(M1_ESP32_RPC_OFF_BEACON_START);
    TEST_ASSERT_EQUAL(M1_ESP32_RPC_OK,
                      m1_esp32_rpc_beacon_start((const char (*)[33])ssids, 8u));
    TEST_ASSERT_EQUAL_HEX16(M1_ESP32_RPC_OFF_BEACON_START, tx_msg_id());
    TEST_ASSERT_LESS_OR_EQUAL_UINT16(M1_ESP32_RPC_PAYLOAD_MAX, tx_plen());
    TEST_ASSERT_EQUAL_UINT8(7u, tx_payload()[0]);
}

void test_probe_start_caps_payload_and_patches_count(void)
{
    char ssids[8][33];
    for (uint8_t i = 0u; i < 8u; i++) {
        memset(ssids[i], 'a' + (char)i, 32u);
        ssids[i][32] = '\0';
    }

    canned_ok(M1_ESP32_RPC_OFF_PROBE_START);
    TEST_ASSERT_EQUAL(M1_ESP32_RPC_OK,
                      m1_esp32_rpc_probe_start(6u, (const char (*)[33])ssids, 8u));
    TEST_ASSERT_EQUAL_HEX16(M1_ESP32_RPC_OFF_PROBE_START, tx_msg_id());
    TEST_ASSERT_LESS_OR_EQUAL_UINT16(M1_ESP32_RPC_PAYLOAD_MAX, tx_plen());
    TEST_ASSERT_EQUAL_UINT8(6u, tx_payload()[0]);
    TEST_ASSERT_EQUAL_UINT8(7u, tx_payload()[1]);
}

void test_probe_start_rejects_missing_ssids_or_zero_count(void)
{
    char ssids[1][33] = {{0}};
    TEST_ASSERT_EQUAL(M1_ESP32_RPC_ERR_INVALID,
                      m1_esp32_rpc_probe_start(1u, NULL, 1u));
    TEST_ASSERT_EQUAL(M1_ESP32_RPC_ERR_INVALID,
                      m1_esp32_rpc_probe_start(1u, (const char (*)[33])ssids, 0u));
}

/* ================================================================== */
/* Deauth struct payload                                              */
/* ================================================================== */

void test_deauth_start_serialises_struct(void)
{
    m1_esp32_rpc_deauth_req_t req;
    memset(&req, 0, sizeof(req));
    const uint8_t bssid[6] = { 1, 2, 3, 4, 5, 6 };
    memcpy(req.bssid, bssid, 6);
    req.channel = 6;
    memset(req.station, 0xFF, 6);
    req.count = 100;
    req.interval_ms = 10;

    canned_ok(M1_ESP32_RPC_OFF_DEAUTH_START);
    TEST_ASSERT_EQUAL(M1_ESP32_RPC_OK, m1_esp32_rpc_deauth_start(&req));
    TEST_ASSERT_EQUAL_HEX16(M1_ESP32_RPC_OFF_DEAUTH_START, tx_msg_id());
    TEST_ASSERT_EQUAL_UINT16(sizeof(req), tx_plen());
    TEST_ASSERT_EQUAL_UINT8_ARRAY((const uint8_t *)&req, tx_payload(), sizeof(req));
}

void test_deauth_start_null_rejected(void)
{
    TEST_ASSERT_EQUAL(M1_ESP32_RPC_ERR_INVALID, m1_esp32_rpc_deauth_start(NULL));
}

/* ================================================================== */
/* Handshake capture (hs_start)                                       */
/* ================================================================== */

void test_hs_start_encodes_9_byte_payload(void)
{
    /* Wire format: [bssid:6][channel:1][deauth_count:2 LE] */
    static const uint8_t bssid[6] = { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF };
    canned_ok(M1_ESP32_RPC_OFF_HS_START);
    TEST_ASSERT_EQUAL(M1_ESP32_RPC_OK,
                      m1_esp32_rpc_hs_start(bssid, 6u, 5u));
    TEST_ASSERT_EQUAL_HEX16(M1_ESP32_RPC_OFF_HS_START, tx_msg_id());
    TEST_ASSERT_EQUAL_UINT16(9u, tx_plen());
    TEST_ASSERT_EQUAL_MEMORY(bssid, tx_payload(), 6u);
    TEST_ASSERT_EQUAL_UINT8(6u, tx_payload()[6]);           /* channel */
    TEST_ASSERT_EQUAL_UINT8(5u, tx_payload()[7]);           /* deauth_count lo */
    TEST_ASSERT_EQUAL_UINT8(0u, tx_payload()[8]);           /* deauth_count hi */
}

void test_hs_start_null_bssid_rejected(void)
{
    TEST_ASSERT_EQUAL(M1_ESP32_RPC_ERR_INVALID, m1_esp32_rpc_hs_start(NULL, 6u, 3u));
}



void test_ble_hid_key_encodes_modifier_and_keys(void)
{
    const uint8_t keys[3] = { 0x04, 0x05, 0x06 };
    canned_ok(M1_ESP32_RPC_BLE_HID_KEY);
    TEST_ASSERT_EQUAL(M1_ESP32_RPC_OK,
                      m1_esp32_rpc_ble_hid_key(0x02, keys, 3));
    TEST_ASSERT_EQUAL_HEX16(M1_ESP32_RPC_BLE_HID_KEY, tx_msg_id());
    TEST_ASSERT_EQUAL_UINT16(5u, tx_plen());          /* mod + count + 3 keys */
    TEST_ASSERT_EQUAL_UINT8(0x02, tx_payload()[0]);
    TEST_ASSERT_EQUAL_UINT8(3u,   tx_payload()[1]);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(keys, &tx_payload()[2], 3);
}

void test_ble_hid_key_no_keys_ok(void)
{
    canned_ok(M1_ESP32_RPC_BLE_HID_KEY);
    TEST_ASSERT_EQUAL(M1_ESP32_RPC_OK,
                      m1_esp32_rpc_ble_hid_key(0x00, NULL, 0));
    TEST_ASSERT_EQUAL_UINT16(2u, tx_plen());
}

void test_ble_hid_key_rejects_bad_args(void)
{
    const uint8_t keys[3] = { 1, 2, 3 };
    TEST_ASSERT_EQUAL(M1_ESP32_RPC_ERR_INVALID,
                      m1_esp32_rpc_ble_hid_key(0, keys, 7));   /* > 6 keys */
    TEST_ASSERT_EQUAL(M1_ESP32_RPC_ERR_INVALID,
                      m1_esp32_rpc_ble_hid_key(0, NULL, 3));   /* NULL + count */
}

void test_ble_hid_init_truncates_long_name(void)
{
    char name[64];
    memset(name, 'A', sizeof(name));
    name[63] = '\0';
    canned_ok(M1_ESP32_RPC_BLE_HID_INIT);
    TEST_ASSERT_EQUAL(M1_ESP32_RPC_OK, m1_esp32_rpc_ble_hid_init(name));
    TEST_ASSERT_EQUAL_UINT16(31u, tx_plen());   /* clamped to 31 bytes */
}

void test_ble_hid_init_null_name_empty_payload(void)
{
    canned_ok(M1_ESP32_RPC_BLE_HID_INIT);
    TEST_ASSERT_EQUAL(M1_ESP32_RPC_OK, m1_esp32_rpc_ble_hid_init(NULL));
    TEST_ASSERT_EQUAL_UINT16(0u, tx_plen());
}

/* ================================================================== */
/* WiFi scan response decode                                          */
/* ================================================================== */

static uint16_t build_scan_resp(uint8_t *body, uint8_t n_entries)
{
    /* [count:2 LE] then n_entries × (fixed entry + ssid_len ssid bytes) —
     * matches the brain firmware's handle_wifi_scan() wire format. */
    uint16_t off = 0u;
    body[off++] = n_entries;
    body[off++] = 0u;
    for (uint8_t i = 0; i < n_entries; i++) {
        m1_esp32_rpc_scan_entry_t e;
        memset(&e, 0, sizeof(e));
        e.bssid[0] = (uint8_t)(0x10 + i);
        e.rssi = (int8_t)(-40 - i);
        e.channel = (uint8_t)(i + 1);
        e.authmode = 3;
        e.ssid_len = 2;                 /* "S<i>" */
        memcpy(&body[off], &e, sizeof(e));
        off = (uint16_t)(off + sizeof(e));
        body[off++] = 'S';
        body[off++] = (uint8_t)('0' + i);
    }
    return off;
}

void test_wifi_scan_decodes_entries(void)
{
    uint8_t body[128];
    uint16_t blen = build_scan_resp(body, 3);
    g_canned_len = make_frame(g_canned, M1_ESP32_RPC_RESP,
                              M1_ESP32_RPC_WIFI_SCAN, body, blen);

    m1_esp32_rpc_wifi_scan_result_t out[8];
    uint8_t count = 0;
    TEST_ASSERT_EQUAL(M1_ESP32_RPC_OK,
                      m1_esp32_rpc_wifi_scan(out, 8, &count));
    TEST_ASSERT_EQUAL_UINT8(3u, count);
    TEST_ASSERT_EQUAL_UINT8(0x10, out[0].bssid[0]);
    TEST_ASSERT_EQUAL_INT8(-40, out[0].rssi);
    TEST_ASSERT_EQUAL_UINT8(0x12, out[2].bssid[0]);
    TEST_ASSERT_EQUAL_UINT8(3u, out[2].channel);
    TEST_ASSERT_EQUAL_STRING("S0", out[0].ssid);
    TEST_ASSERT_EQUAL_STRING("S2", out[2].ssid);
}

void test_wifi_scan_caps_to_max(void)
{
    uint8_t body[128];
    uint16_t blen = build_scan_resp(body, 4);
    g_canned_len = make_frame(g_canned, M1_ESP32_RPC_RESP,
                              M1_ESP32_RPC_WIFI_SCAN, body, blen);

    m1_esp32_rpc_wifi_scan_result_t out[2];
    uint8_t count = 0;
    TEST_ASSERT_EQUAL(M1_ESP32_RPC_OK,
                      m1_esp32_rpc_wifi_scan(out, 2, &count));
    TEST_ASSERT_EQUAL_UINT8(2u, count);
}

void test_wifi_scan_null_out_rejected(void)
{
    uint8_t count = 9;
    TEST_ASSERT_EQUAL(M1_ESP32_RPC_ERR_INVALID,
                      m1_esp32_rpc_wifi_scan(NULL, 4, &count));
    TEST_ASSERT_EQUAL_UINT8(0u, count);
}

/* Regression guard (issue #719 Phase 5): field read-back showed WIFI_SCAN
 * failing with "op0103 no-reply st253 r0 p0" — the transport's poll budget
 * (scaled from the caller's timeout_sec) expired before the brain's
 * synchronous full-channel scan replied. m1_esp32_rpc_wifi_scan() must pass
 * the longer, WIFI_SCAN-specific timeout rather than the generic prompt-
 * command M1_ESP32_RPC_FEATURE_TIMEOUT_S. */
void test_wifi_scan_uses_extended_timeout(void)
{
    uint8_t body[128];
    uint16_t blen = build_scan_resp(body, 1);
    g_canned_len = make_frame(g_canned, M1_ESP32_RPC_RESP,
                              M1_ESP32_RPC_WIFI_SCAN, body, blen);

    m1_esp32_rpc_wifi_scan_result_t out[4];
    uint8_t count = 0;
    TEST_ASSERT_EQUAL(M1_ESP32_RPC_OK,
                      m1_esp32_rpc_wifi_scan(out, 4, &count));
    TEST_ASSERT_EQUAL_INT(M1_ESP32_RPC_WIFI_SCAN_TIMEOUT_S, g_last_timeout_sec);
    TEST_ASSERT_GREATER_THAN_INT(M1_ESP32_RPC_FEATURE_TIMEOUT_S,
                                 g_last_timeout_sec);
}

void test_wifi_scan_propagates_nak(void)
{
    const uint8_t body[] = { M1_ESP32_RPC_ERR_NOT_INIT };
    g_canned_len = make_frame(g_canned, M1_ESP32_RPC_NAK,
                              M1_ESP32_RPC_WIFI_SCAN, body, sizeof(body));
    m1_esp32_rpc_wifi_scan_result_t out[4];
    uint8_t count = 5;
    TEST_ASSERT_EQUAL(M1_ESP32_RPC_ERR_NOT_INIT,
                      m1_esp32_rpc_wifi_scan(out, 4, &count));
    TEST_ASSERT_EQUAL_UINT8(0u, count);
}

void test_wifi_scan_truncated_ssid_entry_stops_before_oob(void)
{
    uint8_t body[32];
    body[0] = 1u;
    body[1] = 0u;
    m1_esp32_rpc_scan_entry_t e = {
        .bssid = { 1u, 2u, 3u, 4u, 5u, 6u },
        .rssi = -55,
        .channel = 6u,
        .authmode = 3u,
        .ssid_len = 5u,
    };
    memcpy(&body[2], &e, sizeof(e));
    memcpy(&body[2 + sizeof(e)], "ABC", 3u);
    g_canned_len = make_frame(g_canned, M1_ESP32_RPC_RESP,
                              M1_ESP32_RPC_WIFI_SCAN, body,
                              (uint16_t)(2u + sizeof(e) + 3u));

    m1_esp32_rpc_wifi_scan_result_t out[2];
    memset(out, 0xA5, sizeof(out));
    uint8_t count = 9u;
    TEST_ASSERT_EQUAL(M1_ESP32_RPC_OK,
                      m1_esp32_rpc_wifi_scan(out, 2, &count));
    TEST_ASSERT_EQUAL_UINT8(0u, count);
}

/* Regression guard: the brain firmware (handle_wifi_scan() in
 * bedge117/m1-esp32-brain main.c) returns the ENTIRE AP list as one logical
 * RPC response (up to its own M1_SCAN_RESP_MAX == 1800 payload bytes), not
 * chunked behind separate *_GET calls. The old M1_ESP32_RPC_PAYLOAD_MAX (246
 * bytes) reception ceiling silently failed every scan whose encoded payload
 * exceeded it -- roughly 20 APs at this test's fixed entry size, but far
 * fewer with typical (longer) real-world SSIDs -- which is why "AP scan
 * failed. Please try again." reproduced in every non-empty RF environment.
 * 30 entries encode to 362 payload bytes, comfortably over the old 246-byte
 * cap but well under M1_ESP32_RPC_RESP_PAYLOAD_MAX. */
void test_wifi_scan_many_aps_exceeds_old_payload_cap(void)
{
    uint8_t body[512];
    uint16_t blen = build_scan_resp(body, 30);
    TEST_ASSERT_GREATER_THAN_UINT16(246u, blen);
    g_canned_len = make_frame(g_canned, M1_ESP32_RPC_RESP,
                              M1_ESP32_RPC_WIFI_SCAN, body, blen);

    m1_esp32_rpc_wifi_scan_result_t out[32];
    uint8_t count = 0;
    TEST_ASSERT_EQUAL(M1_ESP32_RPC_OK,
                      m1_esp32_rpc_wifi_scan(out, 32, &count));
    TEST_ASSERT_EQUAL_UINT8(30u, count);
    TEST_ASSERT_EQUAL_UINT8(0x10, out[0].bssid[0]);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)(0x10 + 29), out[29].bssid[0]);
}

/* ================================================================== */
/* Zigbee sniff response decode                                       */
/* ================================================================== */

static uint16_t build_zb_resp(uint8_t *body, uint8_t n)
{
    uint16_t off = 0u;
    body[off++] = n;
    for (uint8_t i = 0; i < n; i++) {
        m1_esp32_rpc_zb_device_t d;
        memset(&d, 0, sizeof(d));
        d.addr_mode = 2;
        d.addr[0] = (uint8_t)(0xA0 + i);
        d.panid = (uint16_t)(0x1234 + i);
        d.channel = (uint8_t)(11 + i);
        d.rssi = (int8_t)(-50 - i);
        d.proto = 'Z';
        memcpy(&body[off], &d, sizeof(d));
        off = (uint16_t)(off + sizeof(d));
    }
    return off;
}

void test_zb_sniff_get_decodes_devices(void)
{
    uint8_t body[128];
    uint16_t blen = build_zb_resp(body, 3);
    g_canned_len = make_frame(g_canned, M1_ESP32_RPC_RESP,
                              M1_ESP32_RPC_ZB_SNIFF_GET, body, blen);

    m1_esp32_rpc_zb_device_t out[8];
    uint8_t count = 0;
    TEST_ASSERT_EQUAL(M1_ESP32_RPC_OK,
                      m1_esp32_rpc_zb_sniff_get(out, 8, &count));
    TEST_ASSERT_EQUAL_UINT8(3u, count);
    TEST_ASSERT_EQUAL_UINT8(0xA0, out[0].addr[0]);
    TEST_ASSERT_EQUAL_UINT16(0x1234, out[0].panid);
    TEST_ASSERT_EQUAL_UINT8(13u, out[2].channel);
    TEST_ASSERT_EQUAL_UINT8('Z', out[2].proto);
    TEST_ASSERT_EQUAL_HEX16(M1_ESP32_RPC_ZB_SNIFF_GET, tx_msg_id());
    TEST_ASSERT_EQUAL_UINT16(0u, tx_plen());
}

void test_zb_sniff_get_caps_to_max(void)
{
    uint8_t body[128];
    uint16_t blen = build_zb_resp(body, 5);
    g_canned_len = make_frame(g_canned, M1_ESP32_RPC_RESP,
                              M1_ESP32_RPC_ZB_SNIFF_GET, body, blen);

    m1_esp32_rpc_zb_device_t out[2];
    uint8_t count = 0;
    TEST_ASSERT_EQUAL(M1_ESP32_RPC_OK,
                      m1_esp32_rpc_zb_sniff_get(out, 2, &count));
    TEST_ASSERT_EQUAL_UINT8(2u, count);
}

void test_zb_sniff_get_null_rejected(void)
{
    uint8_t count = 7;
    TEST_ASSERT_EQUAL(M1_ESP32_RPC_ERR_INVALID,
                      m1_esp32_rpc_zb_sniff_get(NULL, 4, &count));
    TEST_ASSERT_EQUAL_UINT8(0u, count);
}

void test_zb_sniff_get_zero_devices(void)
{
    const uint8_t body[] = { 0u };
    g_canned_len = make_frame(g_canned, M1_ESP32_RPC_RESP,
                              M1_ESP32_RPC_ZB_SNIFF_GET, body, sizeof(body));

    m1_esp32_rpc_zb_device_t out[2];
    memset(out, 0xA5, sizeof(out));
    uint8_t count = 9u;
    TEST_ASSERT_EQUAL(M1_ESP32_RPC_OK,
                      m1_esp32_rpc_zb_sniff_get(out, 2, &count));
    TEST_ASSERT_EQUAL_UINT8(0u, count);
    TEST_ASSERT_EQUAL_HEX16(M1_ESP32_RPC_ZB_SNIFF_GET, tx_msg_id());
    TEST_ASSERT_EQUAL_UINT16(0u, tx_plen());
}

/* ================================================================== */
/* Runner                                                             */
/* ================================================================== */

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_opcode_map_known_features);
    RUN_TEST(test_opcode_map_every_id_resolves_or_declines);
    RUN_TEST(test_opcode_map_unmapped_and_oob);

    RUN_TEST(test_trigger_builds_correct_opcode_and_empty_payload);
    RUN_TEST(test_trigger_variants_route_expected_opcodes);
    RUN_TEST(test_trigger_propagates_nak);
    RUN_TEST(test_trigger_transport_failure);

    RUN_TEST(test_channel_commands_encode_single_byte);
    RUN_TEST(test_ble_scan_start_encodes_dur_s);
    RUN_TEST(test_set_mac_encodes_six_bytes);
    RUN_TEST(test_set_mac_null_rejected);

    RUN_TEST(test_monitor_read_decodes_frame);
    RUN_TEST(test_monitor_read_empty_response_is_ok);
    RUN_TEST(test_monitor_read_truncated_header_rejects);
    RUN_TEST(test_monitor_read_oversized_len_rejects);
    RUN_TEST(test_monitor_read_truncates_to_frame_max_and_reports_copied_len);

    RUN_TEST(test_ble_adv_start_encodes_name);
    RUN_TEST(test_ble_adv_start_null_name_empty_payload);

    RUN_TEST(test_deauth_start_serialises_struct);
    RUN_TEST(test_deauth_start_null_rejected);

    RUN_TEST(test_hs_start_encodes_9_byte_payload);
    RUN_TEST(test_hs_start_null_bssid_rejected);

    RUN_TEST(test_ble_hid_key_encodes_modifier_and_keys);
    RUN_TEST(test_ble_hid_key_no_keys_ok);
    RUN_TEST(test_ble_hid_key_rejects_bad_args);
    RUN_TEST(test_ble_hid_init_truncates_long_name);
    RUN_TEST(test_ble_hid_init_null_name_empty_payload);

    RUN_TEST(test_wifi_scan_decodes_entries);
    RUN_TEST(test_wifi_scan_caps_to_max);
    RUN_TEST(test_wifi_scan_uses_extended_timeout);
    RUN_TEST(test_wifi_scan_null_out_rejected);
    RUN_TEST(test_wifi_scan_propagates_nak);
    RUN_TEST(test_wifi_scan_truncated_ssid_entry_stops_before_oob);
    RUN_TEST(test_wifi_scan_many_aps_exceeds_old_payload_cap);

    RUN_TEST(test_zb_sniff_get_decodes_devices);
    RUN_TEST(test_zb_sniff_get_caps_to_max);
    RUN_TEST(test_zb_sniff_get_null_rejected);
    RUN_TEST(test_zb_sniff_get_zero_devices);

    return UNITY_END();
}
