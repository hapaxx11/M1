/* See COPYING.txt for license details. */

/*
 * test_esp32_dag_gating_audit.c
 *
 * Source-level checks for a comprehensive audit of ESP32-dependent scene
 * delegates that must be properly capability-gated so they show the
 * standard "not supported" screen on dag T-800 AT firmware (and other
 * firmware variants lacking the relevant capability) instead of silently
 * attempting a binary-SPI-only command and failing.
 *
 * This is a follow-up to the Station Scan fix (issue: "wifi station scan
 * results in start failed when running dag esp32") which covers the rest
 * of the ESP32 feature surface:
 *   - WiFi Sniffers (All/Beacon/Probe/Deauth/EAPOL/Pwnagotchi/SAE)
 *   - WiFi MAC Track / Signal Monitor (CMD_PKTMON_*)
 *   - WiFi Net Scan (Ping/ARP/SSH/Telnet/Port)
 *   - WiFi 802.15.4 (Zigbee/Thread)
 *   - Bluetooth BLE Scan / BLE Advertise
 *   - Bluetooth BLE Sniffers (Analyzer/Generic/Flipper/AirTag)
 *
 * Functions that are pure local placeholders ("not yet implemented" stubs
 * with no ESP32 SPI traffic — e.g. ble_monitor_airtag, ble_wardrive,
 * ble_detect_*, ble_spoof_airtag, bluetooth_config) are intentionally left
 * ungated and are NOT covered here.
 *
 * Functions that already implement a dual AT/binary-SPI dispatch path
 * internally (deauth, beacon, karma, evil portal, probe flood, ap clone,
 * rickroll, pmkid_at, wifi_scan_ap/wifi_do_scan-based Networks/Survey/
 * Wardrive, and all ble_spam_* variants) are also NOT covered here — they
 * already work correctly on dag firmware without scene-level gating.
 */

#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M1_ROOT
#error "M1_ROOT must be defined by CMake"
#endif

void setUp(void) {}
void tearDown(void) {}

static char *read_file(const char *relpath)
{
    char path[512];
    FILE *fp;
    long size;
    char *buf;

    snprintf(path, sizeof(path), "%s/%s", M1_ROOT, relpath);
    fp = fopen(path, "rb");
    TEST_ASSERT_NOT_NULL_MESSAGE(fp, path);

    TEST_ASSERT_EQUAL_INT(0, fseek(fp, 0, SEEK_END));
    size = ftell(fp);
    TEST_ASSERT_GREATER_THAN_INT(0, size);
    TEST_ASSERT_EQUAL_INT(0, fseek(fp, 0, SEEK_SET));

    buf = (char *)malloc((size_t)size + 1U);
    TEST_ASSERT_NOT_NULL(buf);
    TEST_ASSERT_EQUAL_size_t((size_t)size, fread(buf, 1U, (size_t)size, fp));
    buf[size] = '\0';
    fclose(fp);
    return buf;
}

static void expect_gated(const char *relpath, const char *needle)
{
    char *c = read_file(relpath);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(c, needle), needle);
    free(c);
}

static void expect_not_present(const char *relpath, const char *needle)
{
    char *c = read_file(relpath);
    TEST_ASSERT_NULL_MESSAGE(strstr(c, needle), needle);
    free(c);
}

/*--------------------------------------------------------------------------*/
/* WiFi Sniffers — CMD_PKTMON_START/NEXT/STOP, ESP32_FEATURE_PKTMON         */
/*--------------------------------------------------------------------------*/

void test_wifi_sniffers_capability_gated(void)
{
    const char *path = "m1_csrc/m1_wifi_scene_sniff.c";

    expect_gated(path, "DELEGATE_FEATURE(sniff_all,        wifi_sniff_all,        ESP32_FEATURE_PKTMON)");
    expect_gated(path, "DELEGATE_FEATURE(sniff_beacon,     wifi_sniff_beacon,     ESP32_FEATURE_PKTMON)");
    expect_gated(path, "DELEGATE_FEATURE(sniff_probe,      wifi_sniff_probe,      ESP32_FEATURE_PKTMON)");
    expect_gated(path, "DELEGATE_FEATURE(sniff_deauth,     wifi_sniff_deauth,     ESP32_FEATURE_PKTMON)");
    expect_gated(path, "DELEGATE_FEATURE(sniff_eapol,      wifi_sniff_eapol,      ESP32_FEATURE_PKTMON)");
    expect_gated(path, "DELEGATE_FEATURE(sniff_pwnagotchi, wifi_sniff_pwnagotchi, ESP32_FEATURE_PKTMON)");
    expect_gated(path, "DELEGATE_FEATURE(sniff_sae,        wifi_sniff_sae,        ESP32_FEATURE_PKTMON)");
}

/*--------------------------------------------------------------------------*/
/* WiFi MAC Track / Signal Monitor — CMD_PKTMON_*, ESP32_FEATURE_PKTMON     */
/*--------------------------------------------------------------------------*/

void test_wifi_mac_track_and_signal_monitor_capability_gated(void)
{
    const char *path = "m1_csrc/m1_wifi_scene_menu.c";

    expect_gated(path, "DELEGATE_FEATURE(mac_track, wifi_mac_track, ESP32_FEATURE_PKTMON)");
    expect_gated(path, "DELEGATE_FEATURE(signal_monitor, wifi_signal_monitor, ESP32_FEATURE_PKTMON)");

    expect_not_present(path, "DELEGATE(mac_track,");
    expect_not_present(path, "DELEGATE(signal_monitor,");
}

/*--------------------------------------------------------------------------*/
/* WiFi 802.15.4 — AT+ZIGSNIFF, ESP32_FEATURE_802154                        */
/*--------------------------------------------------------------------------*/

void test_wifi_802154_capability_gated(void)
{
    const char *path = "m1_csrc/m1_wifi_scene_menu.c";

    expect_gated(path, "DELEGATE_FEATURE(zigbee, zigbee_scan, ESP32_FEATURE_802154)");
    expect_gated(path, "DELEGATE_FEATURE(thread, thread_scan, ESP32_FEATURE_802154)");

    expect_not_present(path, "DELEGATE(zigbee,");
    expect_not_present(path, "DELEGATE(thread,");
}

/*--------------------------------------------------------------------------*/
/* WiFi Net Scan — CMD_NETSCAN_START/NEXT, ESP32_FEATURE_NETSCAN            */
/*--------------------------------------------------------------------------*/

void test_wifi_netscan_capability_gated(void)
{
    const char *path = "m1_csrc/m1_wifi_scene_net.c";

    expect_gated(path, "DELEGATE_FEATURE(net_ping,   wifi_scan_ping,   ESP32_FEATURE_NETSCAN)");
    expect_gated(path, "DELEGATE_FEATURE(net_arp,    wifi_scan_arp,    ESP32_FEATURE_NETSCAN)");
    expect_gated(path, "DELEGATE_FEATURE(net_ssh,    wifi_scan_ssh,    ESP32_FEATURE_NETSCAN)");
    expect_gated(path, "DELEGATE_FEATURE(net_telnet, wifi_scan_telnet, ESP32_FEATURE_NETSCAN)");
    expect_gated(path, "DELEGATE_FEATURE(net_ports,  wifi_scan_ports,  ESP32_FEATURE_NETSCAN)");
}

/*--------------------------------------------------------------------------*/
/* Bluetooth BLE Scan / BLE Advertise                                       */
/*--------------------------------------------------------------------------*/

void test_bt_scan_and_advertise_capability_gated(void)
{
    const char *path = "m1_csrc/m1_bt_scene_menu.c";

    expect_gated(path, "DELEGATE_FEATURE(scan,      bluetooth_scan,      ESP32_FEATURE_BLE_SCAN)");
    expect_gated(path, "DELEGATE_FEATURE(advertise, bluetooth_advertise, ESP32_FEATURE_BLE_ADV)");

    expect_not_present(path, "DELEGATE(scan,");
    expect_not_present(path, "DELEGATE(advertise,");

    /* BT Config is a local settings editor with no ESP32 SPI traffic and
     * must remain ungated. */
    expect_gated(path, "DELEGATE(config,    bluetooth_config)");
}

/*--------------------------------------------------------------------------*/
/* Bluetooth BLE Sniffers — CMD_BLE_SCAN_START/NEXT_RAW,                   */
/* ESP32_FEATURE_BLE_SCAN.  Placeholder stubs must remain ungated.          */
/*--------------------------------------------------------------------------*/

void test_bt_sniffers_capability_gated(void)
{
    const char *path = "m1_csrc/m1_bt_scene_sniff.c";

    expect_gated(path, "DELEGATE_FEATURE(sniff_analyzer, ble_sniff_analyzer, ESP32_FEATURE_BLE_SCAN)");
    expect_gated(path, "DELEGATE_FEATURE(sniff_generic,  ble_sniff_generic,  ESP32_FEATURE_BLE_SCAN)");
    expect_gated(path, "DELEGATE_FEATURE(sniff_flipper,  ble_sniff_flipper,  ESP32_FEATURE_BLE_SCAN)");
    expect_gated(path, "DELEGATE_FEATURE(sniff_airtag,   ble_sniff_airtag,   ESP32_FEATURE_BLE_SCAN)");

    /* Placeholder ("not yet implemented") stubs — no ESP32 SPI traffic, must
     * remain on the plain DELEGATE() macro. */
    expect_gated(path, "DELEGATE(monitor_airtag,  ble_monitor_airtag)");
    expect_gated(path, "DELEGATE(sniff_flock,     ble_sniff_flock)");
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_wifi_sniffers_capability_gated);
    RUN_TEST(test_wifi_mac_track_and_signal_monitor_capability_gated);
    RUN_TEST(test_wifi_802154_capability_gated);
    RUN_TEST(test_wifi_netscan_capability_gated);
    RUN_TEST(test_bt_scan_and_advertise_capability_gated);
    RUN_TEST(test_bt_sniffers_capability_gated);
    return UNITY_END();
}
