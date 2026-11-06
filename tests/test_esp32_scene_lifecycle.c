#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M1_ROOT
#error "M1_ROOT must be defined by CMake"
#endif

void setUp(void) { }
void tearDown(void) { }

static char *read_file(const char *relpath)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", M1_ROOT, relpath);
    FILE *fp = fopen(path, "rb");
    TEST_ASSERT_NOT_NULL_MESSAGE(fp, relpath);
    TEST_ASSERT_EQUAL_INT(0, fseek(fp, 0, SEEK_END));
    long size = ftell(fp);
    TEST_ASSERT_GREATER_THAN_INT(0, size);
    TEST_ASSERT_EQUAL_INT(0, fseek(fp, 0, SEEK_SET));
    char *buf = malloc((size_t)size + 1u);
    TEST_ASSERT_NOT_NULL(buf);
    TEST_ASSERT_EQUAL_size_t((size_t)size, fread(buf, 1u, (size_t)size, fp));
    buf[size] = '\0';
    fclose(fp);
    return buf;
}

static void assert_sequence(const char *start, const char *first,
                            const char *second, const char *third)
{
    const char *a = strstr(start, first);
    const char *b = strstr(start, second);
    const char *c = strstr(start, third);
    TEST_ASSERT_NOT_NULL_MESSAGE(a, first);
    TEST_ASSERT_NOT_NULL_MESSAGE(b, second);
    TEST_ASSERT_NOT_NULL_MESSAGE(c, third);
    TEST_ASSERT_TRUE_MESSAGE(a < b && b < c, "lifecycle calls are out of order");
}

static void assert_pair(const char *start, const char *first, const char *second)
{
    const char *a = strstr(start, first);
    const char *b = strstr(start, second);
    TEST_ASSERT_NOT_NULL_MESSAGE(a, first);
    TEST_ASSERT_NOT_NULL_MESSAGE(b, second);
    TEST_ASSERT_TRUE_MESSAGE(a < b, "lifecycle calls are out of order");
}

static void assert_delegate_lifecycle(const char *content, const char *macro)
{
    const char *start = strstr(content, macro);
    TEST_ASSERT_NOT_NULL_MESSAGE(start, macro);
    assert_sequence(start,
                    "m1_esp32_rpc_foreground_acquire();",
                    "m1_esp32_deinit();",
                    "m1_esp32_rpc_foreground_release();");
}

static void test_all_wifi_and_bt_delegate_macros_hold_foreground_lease(void)
{
    static const char *const files[] = {
        "m1_csrc/m1_wifi_scene_attack.c",
        "m1_csrc/m1_wifi_scene_general.c",
        "m1_csrc/m1_wifi_scene_menu.c",
        "m1_csrc/m1_wifi_scene_net.c",
        "m1_csrc/m1_wifi_scene_sniff.c",
        "m1_csrc/m1_bt_scene_menu.c",
        "m1_csrc/m1_bt_scene_sniff.c",
        "m1_csrc/m1_bt_scene_spam.c",
        "m1_csrc/m1_bt_scene_badbt.c",
    };
    for (uint32_t i = 0; i < sizeof(files) / sizeof(files[0]); i++) {
        char *content = read_file(files[i]);
        if (strstr(content, "#define DELEGATE(name, fn)"))
            assert_delegate_lifecycle(content, "#define DELEGATE(name, fn)");
        if (strstr(content, "#define DELEGATE_FEATURE(name, fn, fid)"))
            assert_delegate_lifecycle(content, "#define DELEGATE_FEATURE(name, fn, fid)");
        if (strstr(content, "#define DELEGATE_RECON(name, fn, fid)"))
            assert_delegate_lifecycle(content, "#define DELEGATE_RECON(name, fn, fid)");
        TEST_ASSERT_NULL_MESSAGE(strstr(content, "esp32_main_init();"), files[i]);
        free(content);
    }
}

static void test_custom_wifi_delegates_release_after_deinit(void)
{
    static const struct {
        const char *file;
        const char *function;
    } cases[] = {
        { "m1_csrc/m1_wifi_scene_menu.c", "static void scan_connect_on_enter(" },
        { "m1_csrc/m1_wifi_scene_general.c", "static void gen_join_on_enter(" },
        { "m1_csrc/m1_wifi_scene_connect.c", "static void saved_on_enter(" },
        { "m1_csrc/m1_wifi_scene_connect.c", "static void disconnect_on_enter(" },
        { "m1_csrc/m1_wifi_scene_target.c", "static void target_connect_on_enter(" },
        { "m1_csrc/m1_wifi_scene_target.c", "#define TARGET_DELEGATE(name, fn)" },
    };
    for (uint32_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        char *content = read_file(cases[i].file);
        assert_sequence(strstr(content, cases[i].function),
                        "m1_esp32_rpc_foreground_acquire();",
                        "m1_esp32_deinit();",
                        "m1_esp32_rpc_foreground_release();");
        free(content);
    }

    char *target = read_file("m1_csrc/m1_wifi_scene_target.c");
    assert_pair(strstr(target, "#define TARGET_DELEGATE(name, fn)"),
                "m1_esp32_rpc_foreground_acquire();",
                "m1_esp32_ensure_init();");
    assert_pair(strstr(target, "static void target_connect_on_enter("),
                "m1_esp32_rpc_foreground_acquire();",
                "m1_esp32_ensure_init();");
    assert_sequence(strstr(target, "static void target_menu_exit("),
                    "m1_esp32_rpc_foreground_acquire();",
                    "m1_esp32_deinit();",
                    "m1_esp32_rpc_foreground_release();");
    TEST_ASSERT_NOT_NULL(strstr(target, ".on_exit  = target_menu_exit"));
    free(target);
}

static void test_active_bt_scenes_hold_lease_until_exit(void)
{
    static const struct {
        const char *file;
        const char *enter;
        const char *exit;
    } cases[] = {
        { "m1_csrc/m1_bt_scene_menu.c", "static void signal_finder_on_enter(",
          "static void signal_finder_on_exit(" },
        { "m1_csrc/m1_bt_scene_sniff.c", "static void monitor_airtag_on_enter(",
          "static void monitor_airtag_on_exit(" },
    };
    for (uint32_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        char *content = read_file(cases[i].file);
        TEST_ASSERT_NOT_NULL(strstr(content, cases[i].enter));
        TEST_ASSERT_NOT_NULL(strstr(strstr(content, cases[i].enter),
                                    "m1_esp32_rpc_foreground_acquire();"));
        assert_pair(strstr(content, cases[i].exit),
                    "m1_esp32_deinit();",
                    "m1_esp32_rpc_foreground_release();");
        free(content);
    }
}

static void test_native_recon_and_espnow_lifecycles(void)
{
    char *radio = read_file("m1_csrc/m1_wifi_scene_radio.c");
    const char *init = strstr(radio, "static bool radio_init_feature(");
    TEST_ASSERT_NOT_NULL(init);
    const char *guard = strstr(init, "m1_esp32_lifecycle_at_task_required(transport)");
    const char *at_init = strstr(init, "esp32_main_init();");
    TEST_ASSERT_NOT_NULL(guard);
    TEST_ASSERT_NOT_NULL(at_init);
    TEST_ASSERT_TRUE(guard < at_init);
    assert_sequence(init,
                    "m1_esp32_rpc_foreground_acquire();",
                    "m1_esp32_ensure_init();",
                    "m1_esp32_lifecycle_at_task_required(transport)");
    assert_pair(strstr(radio, "static void radio_release_feature("),
                "m1_esp32_deinit();",
                "m1_esp32_rpc_foreground_release();");
    free(radio);

    char *espnow = read_file("m1_csrc/m1_espnow_scene.c");
    TEST_ASSERT_NOT_NULL(strstr(strstr(espnow, "static void espnow_hw_init("),
                                "m1_esp32_rpc_foreground_acquire();"));
    assert_sequence(strstr(espnow, "static void espnow_hw_deinit("),
                    "m1_espnow_stop();",
                    "m1_esp32_deinit();",
                    "m1_esp32_rpc_foreground_release();");
    free(espnow);
}

static void test_native_operations_gate_legacy_task_by_transport(void)
{
    static const struct {
        const char *file;
        const char *function;
    } cases[] = {
        { "m1_csrc/m1_wifi.c", "static uint16_t wifi_do_scan(" },
        { "m1_csrc/m1_wifi.c", "void wifi_scan_ap(" },
        { "m1_csrc/m1_wifi.c", "static bool wifi_connect_from_saved(" },
        { "m1_csrc/m1_wifi.c", "void wifi_disconnect(" },
        { "m1_csrc/m1_espnow_hal.c", "bool m1_espnow_start(" },
    };
    for (uint32_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        char *content = read_file(cases[i].file);
        const char *start = strstr(content, cases[i].function);
        TEST_ASSERT_NOT_NULL_MESSAGE(start, cases[i].function);
        const char *guard = strstr(start, "m1_esp32_lifecycle_at_task_required(");
        const char *at_init = strstr(start, "esp32_main_init();");
        TEST_ASSERT_NOT_NULL(guard);
        TEST_ASSERT_NOT_NULL(at_init);
        TEST_ASSERT_TRUE(guard < at_init);
        free(content);
    }

    char *attack = read_file("m1_csrc/m1_wifi_scene_attack.c");
    assert_pair(strstr(attack, "#define DELEGATE(name, fn)"),
                "m1_esp32_rpc_foreground_acquire();",
                "m1_esp32_ensure_init();");
    TEST_ASSERT_NOT_NULL(strstr(attack, "DELEGATE(attack_pmkid_at,     attack_pmkid_capture)"));
    TEST_ASSERT_NOT_NULL(strstr(attack, "wifi_scan_ap_target_selected()"));
    TEST_ASSERT_NOT_NULL(strstr(attack, "if (wifi_target_valid())"));
    free(attack);
}

static void test_gatt_and_raw_scan_use_cancellable_pending_states(void)
{
    char *bt = read_file("m1_csrc/m1_bt.c");
    TEST_ASSERT_NOT_NULL(strstr(bt, "static bool ble_gatt_poll_rpc(void)"));
    TEST_ASSERT_NOT_NULL(strstr(bt, "s_gatt_rpc_pending ? pdMS_TO_TICKS(100) : portMAX_DELAY"));
    TEST_ASSERT_NOT_NULL(strstr(bt, "showing_gatt || s_gatt_rpc_pending"));
    TEST_ASSERT_NOT_NULL(strstr(bt, "req[6] = dev->addr_type;"));
    TEST_ASSERT_NOT_NULL(strstr(bt, "enable ? mode : 0"));
    TEST_ASSERT_NOT_NULL(strstr(bt, "rpc_req, sizeof(rpc_req)"));
    TEST_ASSERT_NOT_NULL(strstr(bt, "scan_status == M1_ESP32_RPC_PENDING"));
    TEST_ASSERT_NOT_NULL(strstr(bt, "HAL_GetTick() - scan_started > 7000u"));
    TEST_ASSERT_NULL(strstr(bt, "while (HAL_GetTick() - started < 30000u"));
    free(bt);
}

static void test_csi_preserves_station_connection(void)
{
    char *menu = read_file("m1_csrc/m1_wifi_scene_menu.c");
    const char *enter = strstr(menu, "static void recon_menu_enter(");
    TEST_ASSERT_NOT_NULL(enter);
    const char *event = strstr(enter, "static bool recon_menu_event");
    const char *prompt = strstr(enter, "wifi_prompt_disconnect");
    TEST_ASSERT_NOT_NULL(event);
    TEST_ASSERT_TRUE(prompt == NULL || prompt > event);
    TEST_ASSERT_NOT_NULL(strstr(menu, "wifi_recon_prepare(fid)"));
    free(menu);

    char *wifi = read_file("m1_csrc/m1_wifi.c");
    TEST_ASSERT_NOT_NULL(strstr(wifi, "fid != ESP32_FEATURE_CSI"));
    free(wifi);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_all_wifi_and_bt_delegate_macros_hold_foreground_lease);
    RUN_TEST(test_custom_wifi_delegates_release_after_deinit);
    RUN_TEST(test_active_bt_scenes_hold_lease_until_exit);
    RUN_TEST(test_native_recon_and_espnow_lifecycles);
    RUN_TEST(test_native_operations_gate_legacy_task_by_transport);
    RUN_TEST(test_gatt_and_raw_scan_use_cancellable_pending_states);
    RUN_TEST(test_csi_preserves_station_connection);
    return UNITY_END();
}
