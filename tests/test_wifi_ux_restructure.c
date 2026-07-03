/* See COPYING.txt for license details. */

/*
 * test_wifi_ux_restructure.c
 *
 * Source-level checks for the WiFi UX restructuring (issue #621):
 *   - Top-level menu renamed from "Scan & Connect" to "Networks"
 *   - Sniffers/Attacks/Recon sub-menus prompt disconnect if WiFi is connected
 *   - Net Scan sub-menu requires WiFi connection before showing tools
 *   - wifi_prompt_disconnect() and wifi_require_connected() are declared
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

/*--------------------------------------------------------------------------*/
/* 1. Top-level menu label is "Networks", NOT "Scan & Connect"              */
/*--------------------------------------------------------------------------*/

void test_menu_label_is_networks(void)
{
    char *c = read_file("m1_csrc/m1_wifi_scene_menu.c");
    TEST_ASSERT_NOT_NULL(c);

    /* Must contain the new label */
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(c, "\"Networks\""),
        "Top-level WiFi menu must use 'Networks' label");

    /* Must NOT contain the old label */
    TEST_ASSERT_NULL_MESSAGE(strstr(c, "\"Scan & Connect\""),
        "Old 'Scan & Connect' label must be removed");

    free(c);
}

/*--------------------------------------------------------------------------*/
/* 2. Sniffer sub-menu prompts disconnect                                   */
/*--------------------------------------------------------------------------*/

void test_sniffer_menu_prompts_disconnect(void)
{
    char *c = read_file("m1_csrc/m1_wifi_scene_sniff.c");
    TEST_ASSERT_NOT_NULL(c);

    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(c, "wifi_prompt_disconnect"),
        "Sniffer sub-menu must call wifi_prompt_disconnect");

    free(c);
}

/*--------------------------------------------------------------------------*/
/* 3. Attack sub-menu prompts disconnect                                    */
/*--------------------------------------------------------------------------*/

void test_attack_menu_prompts_disconnect(void)
{
    char *c = read_file("m1_csrc/m1_wifi_scene_attack.c");
    TEST_ASSERT_NOT_NULL(c);

    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(c, "wifi_prompt_disconnect"),
        "Attack sub-menu must call wifi_prompt_disconnect");

    free(c);
}

/*--------------------------------------------------------------------------*/
/* 4. Recon sub-menu prompts disconnect                                     */
/*--------------------------------------------------------------------------*/

void test_recon_menu_prompts_disconnect(void)
{
    char *c = read_file("m1_csrc/m1_wifi_scene_menu.c");
    TEST_ASSERT_NOT_NULL(c);

    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(c, "wifi_prompt_disconnect"),
        "Recon sub-menu (in m1_wifi_scene_menu.c) must call wifi_prompt_disconnect");

    free(c);
}

/*--------------------------------------------------------------------------*/
/* 5. Net Scan sub-menu requires connection                                 */
/*--------------------------------------------------------------------------*/

void test_net_scan_requires_connected(void)
{
    char *c = read_file("m1_csrc/m1_wifi_scene_net.c");
    TEST_ASSERT_NOT_NULL(c);

    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(c, "wifi_require_connected"),
        "Net Scan sub-menu must call wifi_require_connected");

    free(c);
}

/*--------------------------------------------------------------------------*/
/* 6. wifi_prompt_disconnect() and wifi_require_connected() are declared     */
/*--------------------------------------------------------------------------*/

void test_helpers_declared_in_header(void)
{
    char *c = read_file("m1_csrc/m1_wifi.h");
    TEST_ASSERT_NOT_NULL(c);

    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(c, "wifi_prompt_disconnect"),
        "wifi_prompt_disconnect must be declared in m1_wifi.h");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(c, "wifi_require_connected"),
        "wifi_require_connected must be declared in m1_wifi.h");

    free(c);
}

/*--------------------------------------------------------------------------*/
/* 7. Status screen references "Networks" not old name                       */
/*--------------------------------------------------------------------------*/

void test_status_references_networks(void)
{
    char *c = read_file("m1_csrc/m1_wifi.c");
    TEST_ASSERT_NOT_NULL(c);

    /* Should not reference the old menu name in user-visible strings */
    TEST_ASSERT_NULL_MESSAGE(strstr(c, "Use Scan & Connect"),
        "Status screen must reference 'Networks' not old name");

    free(c);
}

/*--------------------------------------------------------------------------*/
/* 8. Connect delegates navigate to Connected menu on success                */
/*--------------------------------------------------------------------------*/

void test_connect_delegates_navigate_to_connected_menu(void)
{
    char *c;

    c = read_file("m1_csrc/m1_wifi_scene_menu.c");
    TEST_ASSERT_NOT_NULL(c);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(c, "m1_scene_replace(app, WifiSceneConnectedMenu)"),
        "Networks scan/connect delegate must replace with Connected menu on successful connect");
    free(c);

    c = read_file("m1_csrc/m1_wifi_scene_connect.c");
    TEST_ASSERT_NOT_NULL(c);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(c, "m1_scene_replace(app, WifiSceneConnectedMenu)"),
        "Saved Networks delegate must replace with Connected menu on successful connect");
    free(c);

    c = read_file("m1_csrc/m1_wifi_scene_general.c");
    TEST_ASSERT_NOT_NULL(c);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(c, "m1_scene_replace(app, WifiSceneConnectedMenu)"),
        "Join WiFi delegate must replace with Connected menu on successful connect");
    free(c);
}

/*--------------------------------------------------------------------------*/
/* 9. General menu does NOT have "Connected" item (#626-2)                  */
/*--------------------------------------------------------------------------*/

void test_general_menu_no_connected_item(void)
{
    char *c = read_file("m1_csrc/m1_wifi_scene_general.c");
    TEST_ASSERT_NOT_NULL(c);

    /* The "Connected" menu item was removed because the Connected menu
     * is reached via post-connection navigation, not a manual menu entry. */
    TEST_ASSERT_NULL_MESSAGE(strstr(c, "\"Connected\""),
        "General menu must NOT have a 'Connected' item (#626-2)");

    free(c);
}

/*--------------------------------------------------------------------------*/
/* 10. Deauth target selection uses "Deauth" label, not "Connect" (#626-6)  */
/*--------------------------------------------------------------------------*/

void test_deauth_uses_deauth_label(void)
{
    char *c = read_file("m1_csrc/m1_wifi.c");
    TEST_ASSERT_NOT_NULL(c);

    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(c, "s_ap_ok_label = \"Deauth\""),
        "Deauth target selection must label OK button as 'Deauth' (#626-6)");

    free(c);
}

/*--------------------------------------------------------------------------*/
/* 11. m1_message_box_choice uses DrawRBox for rounded corners (#626-5)     */
/*--------------------------------------------------------------------------*/

void test_message_box_choice_uses_rounded_corners(void)
{
    char *c = read_file("m1_csrc/m1_display.c");
    TEST_ASSERT_NOT_NULL(c);

    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(c, "u8g2_DrawRBox(&m1_u8g2, box_x"),
        "m1_message_box_choice must use DrawRBox for button highlight (#626-5)");

    /* Must NOT use DrawBox for the button highlight */
    TEST_ASSERT_NULL_MESSAGE(strstr(c, "u8g2_DrawBox(&m1_u8g2, btn_x"),
        "m1_message_box_choice must not use DrawBox for button highlight");

    free(c);
}

/*--------------------------------------------------------------------------*/
/* 12. Deauth/probe flood have AT command fallback paths (#626-6)           */
/*--------------------------------------------------------------------------*/

void test_deauth_has_at_fallback(void)
{
    char *c = read_file("m1_csrc/m1_wifi.c");
    TEST_ASSERT_NOT_NULL(c);

    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(c, "AT+M1DEAUTH="),
        "Deauth must have AT command fallback for dag firmware (#626-6)");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(c, "AT+M1DEAUTHSTOP"),
        "Deauth stop must have AT command fallback (#626-6)");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(c, "AT+M1PROBE"),
        "Probe flood must have AT command fallback (#626-6)");

    free(c);
}

/*--------------------------------------------------------------------------*/
/* 13. Connected AP detail view shows "Scan" button (#626-1)                */
/*--------------------------------------------------------------------------*/

void test_connected_ap_shows_scan_button(void)
{
    char *c = read_file("m1_csrc/m1_wifi.c");
    TEST_ASSERT_NOT_NULL(c);

    /* The AP detail view shows "Scan" in the left slot of the 3-column
     * button bar alongside ok_circle_8x8 — assert on the specific call. */
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(c, "\"Scan\", ok_circle_8x8"),
        "Connected AP detail must show 'Scan' in left slot of button bar (#626-1)");

    free(c);
}

/*--------------------------------------------------------------------------*/
/* 14. Beacon spam has AT+M1BEACON fallback (#626-7)                        */
/*--------------------------------------------------------------------------*/

void test_beacon_has_at_fallback(void)
{
    char *c = read_file("m1_csrc/m1_wifi.c");
    TEST_ASSERT_NOT_NULL(c);

    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(c, "AT+M1BEACON="),
        "Beacon spam must have AT+M1BEACON command for dag firmware (#626-7)");

    free(c);
}

/*--------------------------------------------------------------------------*/
/* 15. Evil portal has AT+M1EVILTWIN fallback (#626-7)                      */
/*--------------------------------------------------------------------------*/

void test_evil_portal_has_at_fallback(void)
{
    char *c = read_file("m1_csrc/m1_wifi.c");
    TEST_ASSERT_NOT_NULL(c);

    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(c, "AT+M1EVILTWIN="),
        "Evil portal must have AT+M1EVILTWIN command for dag firmware (#626-7)");

    free(c);
}

/*--------------------------------------------------------------------------*/
/* 16. Karma has AT+M1KARMA fallback (#626-7)                               */
/*--------------------------------------------------------------------------*/

void test_karma_has_at_fallback(void)
{
    char *c = read_file("m1_csrc/m1_wifi.c");
    TEST_ASSERT_NOT_NULL(c);

    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(c, "AT+M1KARMA="),
        "Karma attack must have AT+M1KARMA command for dag firmware (#626-7)");

    free(c);
}

/*--------------------------------------------------------------------------*/
/* 17. AP clone uses AT+M1BEACON for AT firmware (#626-7)                   */
/*--------------------------------------------------------------------------*/

void test_ap_clone_has_at_fallback(void)
{
    char *c = read_file("m1_csrc/m1_wifi.c");
    TEST_ASSERT_NOT_NULL(c);

    /* Scope the search to wifi_attack_ap_clone() so later AT+M1BEACON uses
     * (e.g. Rickroll) can't produce a false-positive. */
    const char *fn_start = strstr(c, "void wifi_attack_ap_clone(");
    TEST_ASSERT_NOT_NULL_MESSAGE(fn_start,
        "wifi_attack_ap_clone() must exist (#626-7)");

    const char *fn_end = strstr(fn_start, "void wifi_attack_rickroll(");
    TEST_ASSERT_NOT_NULL_MESSAGE(fn_end,
        "wifi_attack_ap_clone() end marker not found");

    size_t slice_len = (size_t)(fn_end - fn_start);
    char *slice = (char *)malloc(slice_len + 1);
    TEST_ASSERT_NOT_NULL(slice);
    memcpy(slice, fn_start, slice_len);
    slice[slice_len] = '\0';

    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(slice, "AT+M1BEACON="),
        "AP clone must use AT+M1BEACON for AT firmware (#626-7)");

    free(slice);
    free(c);
}

/*--------------------------------------------------------------------------*/
/* 18. Rickroll uses AT+M1BEACON for AT firmware (#626-7)                   */
/*--------------------------------------------------------------------------*/

void test_rickroll_has_at_fallback(void)
{
    char *c = read_file("m1_csrc/m1_wifi.c");
    TEST_ASSERT_NOT_NULL(c);

    const char *fn_start = strstr(c, "wifi_attack_rickroll(");
    TEST_ASSERT_NOT_NULL_MESSAGE(fn_start,
        "wifi_attack_rickroll() must exist (#626-7)");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(fn_start, "AT+M1BEACON="),
        "Rickroll must use AT+M1BEACON for AT firmware (#626-7)");

    free(c);
}

/*--------------------------------------------------------------------------*/
/* 19. Station Scan / Station Wardrive are capability-gated                 */
/*                                                                          */
/* Regression test for: "wifi station scan results in start failed when    */
/* running dag esp32".  Station Scan and Station Wardrive both rely on the */
/* binary-SPI-only CMD_STA_SCAN_START command (SiN360 firmware, or         */
/* neddy299's AT+STASCAN fork).  The dag T-800 AT firmware implements      */
/* neither, so M1_ESP32_CAP_STA_SCAN is never set for it.  Before this fix */
/* the scene delegates were plain DELEGATE() wrappers with no capability   */
/* check, so on dag firmware the SPI command silently failed/timed out     */
/* instead of showing the standard "not supported" screen.  They must use  */
/* DELEGATE_FEATURE(..., ESP32_FEATURE_STA_SCAN) like the other            */
/* capability-gated delegates (see m1_bt_scene_badbt.c).                   */
/*--------------------------------------------------------------------------*/

void test_station_scan_is_capability_gated(void)
{
    char *c = read_file("m1_csrc/m1_wifi_scene_menu.c");
    TEST_ASSERT_NOT_NULL(c);

    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(c, "DELEGATE_FEATURE"),
        "m1_wifi_scene_menu.c must define a capability-gated delegate macro");

    TEST_ASSERT_NOT_NULL_MESSAGE(
        strstr(c, "DELEGATE_FEATURE(station_scan, wifi_station_scan, ESP32_FEATURE_STA_SCAN)"),
        "Station Scan delegate must be gated on ESP32_FEATURE_STA_SCAN");

    TEST_ASSERT_NOT_NULL_MESSAGE(
        strstr(c, "DELEGATE_FEATURE(station_wardrive, wifi_station_wardrive, ESP32_FEATURE_STA_SCAN)"),
        "Station Wardrive delegate must be gated on ESP32_FEATURE_STA_SCAN");

    /* Guard against reintroducing the ungated plain DELEGATE() wrapper. */
    TEST_ASSERT_NULL_MESSAGE(strstr(c, "DELEGATE(station_scan,"),
        "Station Scan must not use the ungated DELEGATE() macro");
    TEST_ASSERT_NULL_MESSAGE(strstr(c, "DELEGATE(station_wardrive,"),
        "Station Wardrive must not use the ungated DELEGATE() macro");

    free(c);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_menu_label_is_networks);
    RUN_TEST(test_sniffer_menu_prompts_disconnect);
    RUN_TEST(test_attack_menu_prompts_disconnect);
    RUN_TEST(test_recon_menu_prompts_disconnect);
    RUN_TEST(test_net_scan_requires_connected);
    RUN_TEST(test_helpers_declared_in_header);
    RUN_TEST(test_status_references_networks);
    RUN_TEST(test_connect_delegates_navigate_to_connected_menu);
    RUN_TEST(test_general_menu_no_connected_item);
    RUN_TEST(test_deauth_uses_deauth_label);
    RUN_TEST(test_message_box_choice_uses_rounded_corners);
    RUN_TEST(test_deauth_has_at_fallback);
    RUN_TEST(test_connected_ap_shows_scan_button);
    RUN_TEST(test_beacon_has_at_fallback);
    RUN_TEST(test_evil_portal_has_at_fallback);
    RUN_TEST(test_karma_has_at_fallback);
    RUN_TEST(test_ap_clone_has_at_fallback);
    RUN_TEST(test_rickroll_has_at_fallback);
    RUN_TEST(test_station_scan_is_capability_gated);
    return UNITY_END();
}
