/* See COPYING.txt for license details. */

/*
 * test_wifi_ux_restructure.c
 *
 * Source-level checks for the WiFi UX restructuring (issues #621, #680):
 *   - Top-level menu item renamed from "Networks" to "Scan & Connect"
 *   - Wardrive promoted to its own top-level sub-menu (Phase 1 of #680)
 *   - Sniffers merged into Recon; Wardrive absorbs AP/SSID list management;
 *     General slimmed; 802.15.4 gains a Flood entry (Phase 2 of #680)
 *   - Attacks/Recon sub-menus prompt disconnect if WiFi is connected
 *   - Net Scan sub-menu requires WiFi connection before showing tools
 *   - wifi_prompt_disconnect() and wifi_require_connected() are declared
 */

#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp32_feature_map.h"

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
/* 1. Top-level menu label is "Scan & Connect", NOT "Networks"              */
/*--------------------------------------------------------------------------*/

void test_menu_label_is_scan_connect(void)
{
    char *c = read_file("m1_csrc/m1_wifi_scene_menu.c");
    TEST_ASSERT_NOT_NULL(c);

    /* Must contain the new label */
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(c, "\"Scan & Connect\""),
        "Top-level WiFi menu must use 'Scan & Connect' label");

    /* Must NOT contain the old top-level "Networks" label entry */
    TEST_ASSERT_NULL_MESSAGE(strstr(c, "\n    \"Networks\","),
        "Old top-level 'Networks' label must be removed");

    free(c);
}

/*--------------------------------------------------------------------------*/
/* 2. Sniffers merged into Recon (Phase 2); no standalone Sniffers menu      */
/*--------------------------------------------------------------------------*/

void test_sniffers_merged_into_recon(void)
{
    char *menu = read_file("m1_csrc/m1_wifi_scene_menu.c");
    TEST_ASSERT_NOT_NULL(menu);

    /* Top-level menu no longer offers a standalone "Sniffers" item. */
    TEST_ASSERT_NULL_MESSAGE(strstr(menu, "\"Sniffers\""),
        "Top-level WiFi menu must no longer have a 'Sniffers' item (Phase 2)");
    TEST_ASSERT_NULL_MESSAGE(strstr(menu, "WifiSceneSnifferMenu"),
        "Top-level WiFi menu must not target the removed Sniffers sub-menu");

    /* Recon now lists the merged passive captures with C3-style naming and
     * routes them to the existing sniffer delegates. */
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(menu, "\"Packet Monitor\""),
        "Recon must list 'Packet Monitor' (merged from Sniffers)");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(menu, "\"SAE / WPA3\""),
        "Recon must list 'SAE / WPA3' (merged from Sniffers)");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(menu, "WifiSceneSniffAll"),
        "Recon target table must route to the sniffer delegates");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(menu, "WifiSceneSniffSae"),
        "Recon target table must route to the sniffer delegates");
    free(menu);

    /* The standalone Sniffers sub-menu scene is gone; sniff.c keeps only the
     * delegates, which inherit Recon's disconnect prompt. */
    char *sniff = read_file("m1_csrc/m1_wifi_scene_sniff.c");
    TEST_ASSERT_NOT_NULL(sniff);
    TEST_ASSERT_NULL_MESSAGE(strstr(sniff, "wifi_scene_sniffer_menu_handlers"),
        "Standalone Sniffers sub-menu handlers must be removed (Phase 2)");
    free(sniff);
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
/* 7. Status/hint screens reference "Scan & Connect" not old menu name       */
/*--------------------------------------------------------------------------*/

void test_status_references_scan_connect(void)
{
    char *c = read_file("m1_csrc/m1_wifi.c");
    TEST_ASSERT_NOT_NULL(c);

    /* Should not reference the old "Use Networks" wording in hint strings */
    TEST_ASSERT_NULL_MESSAGE(strstr(c, "Use Networks"),
        "Hint screens must reference 'Scan & Connect' not the old 'Networks' name");

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

/*--------------------------------------------------------------------------*/
/* 20. Wardrive is a top-level menu; wardrive tools moved out of Recon      */
/*     (Phase 1 of the WiFi cleanup plan)                                   */
/*--------------------------------------------------------------------------*/

void test_wardrive_is_top_level_menu(void)
{
    char *c = read_file("m1_csrc/m1_wifi_scene_menu.c");
    TEST_ASSERT_NOT_NULL(c);

    /* Top-level menu offers a "Wardrive" item targeting its own sub-menu. */
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(c, "\"Wardrive\""),
        "Top-level WiFi menu must offer a 'Wardrive' item");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(c, "WifiSceneWardriveMenu"),
        "Wardrive menu scene target must be wired");

    /* Wardrive tools moved out of the Recon sub-menu into Wardrive. */
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(c, "wardrive_targets"),
        "Wardrive sub-menu target table must exist");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(c, "\"AP Wardrive\""),
        "Wardrive sub-menu must list 'AP Wardrive'");
    TEST_ASSERT_NULL_MESSAGE(strstr(c, "\"Station Wardrive\",\n    \"Signal Monitor\""),
        "Recon sub-menu must no longer contain the wardrive items");

    free(c);
}

/*--------------------------------------------------------------------------*/
/* 21. Wardrive absorbs AP/SSID list-management items (Phase 2 §3.5)         */
/*--------------------------------------------------------------------------*/

void test_wardrive_absorbs_list_mgmt(void)
{
    char *menu = read_file("m1_csrc/m1_wifi_scene_menu.c");
    TEST_ASSERT_NOT_NULL(menu);

    /* Wardrive sub-menu now lists the list-management items and routes them
     * to the existing General delegates. */
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(menu, "\"Save APs\""),
        "Wardrive sub-menu must list 'Save APs' (moved from General)");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(menu, "\"Clear SSIDs\""),
        "Wardrive sub-menu must list 'Clear SSIDs' (moved from General)");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(menu, "WifiSceneGeneralSaveAps"),
        "Wardrive target table must route 'Save APs' to its General delegate");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(menu, "WifiSceneGeneralClearSsids"),
        "Wardrive target table must route 'Clear SSIDs' to its General delegate");
    free(menu);
}

/*--------------------------------------------------------------------------*/
/* 22. General is slimmed: list mgmt removed, Status/Disconnect added        */
/*--------------------------------------------------------------------------*/

void test_general_slimmed(void)
{
    char *c = read_file("m1_csrc/m1_wifi_scene_general.c");
    TEST_ASSERT_NOT_NULL(c);

    /* The list-management labels no longer appear as menu entries in General
     * (they now live under Wardrive). */
    TEST_ASSERT_NULL_MESSAGE(strstr(c, "\"Save APs\","),
        "General menu must no longer list 'Save APs' (moved to Wardrive)");
    TEST_ASSERT_NULL_MESSAGE(strstr(c, "\"Load SSIDs\","),
        "General menu must no longer list 'Load SSIDs' (moved to Wardrive)");

    /* Status and Disconnect are surfaced in General. */
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(c, "\"Status\""),
        "General menu must offer 'Status'");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(c, "\"Disconnect\""),
        "General menu must offer 'Disconnect'");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(c, "WifiSceneStatus"),
        "General target table must route 'Status' to WifiSceneStatus");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(c, "WifiSceneDisconnect"),
        "General target table must route 'Disconnect' to WifiSceneDisconnect");

    free(c);
}

/*--------------------------------------------------------------------------*/
/* 23. 802.15.4 gains a capability-gated Flood entry (Phase 2 §3.6)          */
/*--------------------------------------------------------------------------*/

void test_802154_has_flood(void)
{
    char *menu = read_file("m1_csrc/m1_wifi_scene_menu.c");
    TEST_ASSERT_NOT_NULL(menu);

    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(menu, "\"802.15.4 Flood\""),
        "802.15.4 sub-menu must offer an '802.15.4 Flood' item");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(menu, "WifiScene802154Flood"),
        "802.15.4 Flood scene target must be wired");
    /* Flood is capability-gated like the Zigbee/Thread scans. */
    TEST_ASSERT_NOT_NULL_MESSAGE(
        strstr(menu, "DELEGATE_FEATURE(flood_802154, ieee802154_flood, ESP32_FEATURE_802154)"),
        "802.15.4 Flood delegate must be gated on ESP32_FEATURE_802154");
    free(menu);

    /* The flood implementation exists and drives the ESP32 flood command. */
    char *impl = read_file("m1_csrc/m1_802154.c");
    TEST_ASSERT_NOT_NULL(impl);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(impl, "void ieee802154_flood("),
        "ieee802154_flood() must be implemented");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(impl, "AT+ZIGFLOOD"),
        "ieee802154_flood() must drive the AT+ZIGFLOOD command");
    free(impl);
}

/*--------------------------------------------------------------------------*/
/* 24. Phase 3: selected-network Target scene exists (Target + Connect)      */
/*--------------------------------------------------------------------------*/

void test_target_scene_has_target_and_connect_groups(void)
{
    char *c = read_file("m1_csrc/m1_wifi_scene_target.c");
    TEST_ASSERT_NOT_NULL(c);

    /* Connect group (auth) routes to the existing Connected menu on success. */
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(c, "\"Connect\""),
        "Target scene must offer a 'Connect' item (Connect group)");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(c, "m1_scene_replace(app, WifiSceneConnectedMenu)"),
        "Target 'Connect' must replace into the Connected menu on success");

    /* Target group (needs a specific AP) — per-AP verbs + Cycle AP. */
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(c, "\"Deauth\""),
        "Target scene must offer per-AP 'Deauth'");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(c, "\"Handshake\""),
        "Target scene must offer 'Handshake'");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(c, "\"Beacon\""),
        "Target scene must offer 'Beacon'");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(c, "\"PMKID\""),
        "Target scene must offer 'PMKID'");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(c, "\"Cycle AP\""),
        "Target scene must offer 'Cycle AP'");

    free(c);
}

/*--------------------------------------------------------------------------*/
/* 25. Phase 3: Scan & Connect opens the Target context on selection        */
/*--------------------------------------------------------------------------*/

void test_scan_connect_opens_target_menu(void)
{
    char *c = read_file("m1_csrc/m1_wifi_scene_menu.c");
    TEST_ASSERT_NOT_NULL(c);

    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(c, "wifi_scan_ap_target_selected()"),
        "Scan & Connect delegate must consult wifi_scan_ap_target_selected()");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(c, "m1_scene_push(app, WifiSceneTargetMenu)"),
        "Scan & Connect delegate must push the Target menu on selection");

    free(c);
}

/*--------------------------------------------------------------------------*/
/* 26. Phase 3: Target actions are only reachable via a selected network     */
/*--------------------------------------------------------------------------*/

void test_target_actions_require_selected_network(void)
{
    /* The pending-target flag is only set by the scan loop, so the Target
     * context cannot be opened without first selecting a network. */
    char *w = read_file("m1_csrc/m1_wifi.c");
    TEST_ASSERT_NOT_NULL(w);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(w, "s_wifi_target_selected = true;"),
        "wifi_scan_ap() must flag a pending target on OK for a selected AP");
    /* Every Target action guards on the highlighted AP. */
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(w, "bool wifi_target_valid(void)"),
        "wifi.c must expose wifi_target_valid() to guard Target actions");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(w, "wifi_ap_cycle_next(ap_list, ap_count, ap_view_idx)"),
        "Cycle AP must use the pure wifi_ap_cycle_next() helper");
    free(w);

    /* Top-level menu must NOT list the Target menu directly (unreachable
     * except via a selected network). */
    char *m = read_file("m1_csrc/m1_wifi_scene_menu.c");
    TEST_ASSERT_NOT_NULL(m);
    TEST_ASSERT_NULL_MESSAGE(strstr(m, "WifiSceneTargetMenu,\n"),
        "Top-level menu target table must not list the Target menu");
    free(m);
}

/*--------------------------------------------------------------------------*/
/* 27. Phase 4: Karma+Portal removed from attack menu; Karma uses toggle    */
/*--------------------------------------------------------------------------*/

void test_karma_portal_removed_from_menu(void)
{
    char *c = read_file("m1_csrc/m1_wifi_scene_attack.c");
    TEST_ASSERT_NOT_NULL(c);

    /* "Karma+Portal" must no longer appear as a menu label. */
    TEST_ASSERT_NULL_MESSAGE(strstr(c, "\"Karma+Portal\""),
        "Attack menu must no longer list 'Karma+Portal' (Phase 4 §3.7)");

    /* The menu still has "Karma" with the toggle function. */
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(c, "\"Karma\""),
        "Attack menu must still list 'Karma'");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(c, "wifi_attack_karma_with_portal"),
        "Karma delegate must call wifi_attack_karma_with_portal()");

    /* Item count must be 8 after removing Karma+Portal. */
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(c, "#define ATTACK_ITEM_COUNT  8"),
        "Attack menu item count must be 8 (Phase 4)");

    free(c);
}

/*--------------------------------------------------------------------------*/
/* 28. Phase 4: wifi_attack_karma_with_portal() declared in m1_wifi.h       */
/*--------------------------------------------------------------------------*/

void test_karma_with_portal_declared(void)
{
    char *c = read_file("m1_csrc/m1_wifi.h");
    TEST_ASSERT_NOT_NULL(c);

    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(c, "wifi_attack_karma_with_portal"),
        "wifi_attack_karma_with_portal() must be declared in m1_wifi.h");

    free(c);
}

/*--------------------------------------------------------------------------*/
/* 29. Phase 4: wifi_attack_karma_with_portal() shows portal prompt          */
/*--------------------------------------------------------------------------*/

void test_karma_with_portal_prompts(void)
{
    char *c = read_file("m1_csrc/m1_wifi.c");
    TEST_ASSERT_NOT_NULL(c);

    const char *fn = strstr(c, "void wifi_attack_karma_with_portal(");
    TEST_ASSERT_NOT_NULL_MESSAGE(fn,
        "wifi_attack_karma_with_portal() must be implemented in m1_wifi.c");

    /* Prompts with "Serve portal?" and offers Yes/No choice. */
    const char *prompt = strstr(fn, "Serve portal?");
    TEST_ASSERT_NOT_NULL_MESSAGE(prompt,
        "wifi_attack_karma_with_portal() must prompt 'Serve portal?'");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(fn, "wifi_attack_karma_portal"),
        "wifi_attack_karma_with_portal() must call wifi_attack_karma_portal() on Yes");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(fn, "wifi_attack_karma()"),
        "wifi_attack_karma_with_portal() must call wifi_attack_karma() on No");

    free(c);
}

/*--------------------------------------------------------------------------*/
/* 30. Phase 4: post-deauth chaining prompt in wifi_target_deauth()          */
/*--------------------------------------------------------------------------*/

void test_target_deauth_has_chaining_prompt(void)
{
    char *c = read_file("m1_csrc/m1_wifi.c");
    TEST_ASSERT_NOT_NULL(c);

    const char *fn = strstr(c, "void wifi_target_deauth(");
    TEST_ASSERT_NOT_NULL_MESSAGE(fn,
        "wifi_target_deauth() must be implemented in m1_wifi.c");

    /* Chaining prompt appears after wifi_deauth_run() returns. */
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(fn, "Deauth done"),
        "wifi_target_deauth() must show post-deauth 'Deauth done' prompt (§3.8)");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(fn, "Handshake"),
        "Post-deauth chaining prompt must offer 'Handshake' follow-up");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(fn, "Evil Portal"),
        "Post-deauth chaining prompt must offer 'Evil Portal' follow-up");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(fn, "wifi_sniff_eapol"),
        "Post-deauth chaining must call wifi_sniff_eapol() for Handshake");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(fn, "wifi_evil_portal"),
        "Post-deauth chaining must call wifi_evil_portal() for Evil Portal");

    free(c);
}

/*--------------------------------------------------------------------------*/
/* 31. Phase 4: wifi_multi_target_build() declared in wifi_multi_target.h   */
/*--------------------------------------------------------------------------*/

void test_multi_target_build_declared(void)
{
    char *c = read_file("m1_csrc/wifi_multi_target.h");
    TEST_ASSERT_NOT_NULL(c);

    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(c, "wifi_multi_target_build"),
        "wifi_multi_target_build() must be declared in wifi_multi_target.h");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(c, "wifi_ap_t"),
        "wifi_multi_target_build() must operate on wifi_ap_t arrays");

    free(c);
}

/*--------------------------------------------------------------------------*/
/* 32. Phase 5: WiFi Hotspot capability-gated General menu entry            */
/*--------------------------------------------------------------------------*/

void test_general_menu_has_hotspot_entry(void)
{
    char *c = read_file("m1_csrc/m1_wifi_scene_general.c");
    TEST_ASSERT_NOT_NULL(c);

    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(c, "\"WiFi Hotspot\""),
        "General menu must offer a 'WiFi Hotspot' entry (Phase 5, §3.9)");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(c, "WifiSceneGeneralHotspot"),
        "General target table must route 'WiFi Hotspot' to WifiSceneGeneralHotspot");
    TEST_ASSERT_NOT_NULL_MESSAGE(
        strstr(c, "DELEGATE_FEATURE(gen_hotspot,     wifi_general_hotspot,        ESP32_FEATURE_WIFI_HOTSPOT)"),
        "WiFi Hotspot delegate must be capability-gated on ESP32_FEATURE_WIFI_HOTSPOT");

    free(c);
}

void test_hotspot_capability_hides_feature_when_absent(void)
{
    /* No shipped firmware self-reports M1_ESP32_CAP_WIFI_HOTSPOT yet, so the
     * feature gate must fail closed (deferred / optional, plan §3.9). */
    TEST_ASSERT_FALSE(esp32_feature_supported(0u, ESP32_FEATURE_WIFI_HOTSPOT));
    TEST_ASSERT_TRUE(esp32_feature_supported(
        esp32_feature_required_caps(ESP32_FEATURE_WIFI_HOTSPOT),
        ESP32_FEATURE_WIFI_HOTSPOT));
}

void test_wifi_general_hotspot_declared(void)
{
    char *h = read_file("m1_csrc/m1_wifi.h");
    TEST_ASSERT_NOT_NULL(h);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(h, "void wifi_general_hotspot(void);"),
        "wifi_general_hotspot() must be declared in m1_wifi.h");
    free(h);

    char *c = read_file("m1_csrc/m1_wifi.c");
    TEST_ASSERT_NOT_NULL(c);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(c, "void wifi_general_hotspot(void)"),
        "wifi_general_hotspot() must be implemented in m1_wifi.c");
    free(c);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_menu_label_is_scan_connect);
    RUN_TEST(test_sniffers_merged_into_recon);
    RUN_TEST(test_attack_menu_prompts_disconnect);
    RUN_TEST(test_recon_menu_prompts_disconnect);
    RUN_TEST(test_net_scan_requires_connected);
    RUN_TEST(test_helpers_declared_in_header);
    RUN_TEST(test_status_references_scan_connect);
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
    RUN_TEST(test_wardrive_is_top_level_menu);
    RUN_TEST(test_wardrive_absorbs_list_mgmt);
    RUN_TEST(test_general_slimmed);
    RUN_TEST(test_802154_has_flood);
    RUN_TEST(test_target_scene_has_target_and_connect_groups);
    RUN_TEST(test_scan_connect_opens_target_menu);
    RUN_TEST(test_target_actions_require_selected_network);
    RUN_TEST(test_karma_portal_removed_from_menu);
    RUN_TEST(test_karma_with_portal_declared);
    RUN_TEST(test_karma_with_portal_prompts);
    RUN_TEST(test_target_deauth_has_chaining_prompt);
    RUN_TEST(test_multi_target_build_declared);
    RUN_TEST(test_general_menu_has_hotspot_entry);
    RUN_TEST(test_hotspot_capability_hides_feature_when_absent);
    RUN_TEST(test_wifi_general_hotspot_declared);
    return UNITY_END();
}

