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
    return UNITY_END();
}
