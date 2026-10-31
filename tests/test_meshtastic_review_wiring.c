#ifndef M1_ROOT
#error "M1_ROOT must be defined by CMake"
#endif

#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char* read_repo_file(const char* relpath) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", M1_ROOT, relpath);
    FILE* file = fopen(path, "rb");
    if(!file) return NULL;
    if(fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }
    long length = ftell(file);
    if(length < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }
    char* content = malloc((size_t)length + 1);
    if(!content) {
        fclose(file);
        return NULL;
    }
    size_t read = fread(content, 1, (size_t)length, file);
    fclose(file);
    if(read != (size_t)length) {
        free(content);
        return NULL;
    }
    content[read] = '\0';
    return content;
}

void setUp(void) {}
void tearDown(void) {}

void test_app_manager_releases_uart_before_unloading_elf(void) {
    char* content = read_repo_file("m1_csrc/m1_app_manager.c");
    TEST_ASSERT_NOT_NULL(content);
    const char* cleanup = strstr(content, "/* Cleanup */");
    const char* service_clear =
        cleanup ? strstr(cleanup, "m1app_set_keyboard_service_callback(NULL);") : NULL;
    const char* uart_release = cleanup ? strstr(cleanup, "m1_uart_app_deinit();") : NULL;
    const char* unload = cleanup ? strstr(cleanup, "elf_unload_app(&s_run_ctx.elf);") : NULL;
    TEST_ASSERT_NOT_NULL(service_clear);
    TEST_ASSERT_NOT_NULL(uart_release);
    TEST_ASSERT_NOT_NULL(unload);
    TEST_ASSERT_TRUE(service_clear < uart_release);
    TEST_ASSERT_TRUE(uart_release < unload);
    free(content);
}

void test_uart_bridge_acknowledges_pause_before_app_claims_stream(void) {
    char* app_uart = read_repo_file("m1_csrc/m1_uart_app.c");
    char* bridge = read_repo_file("m1_csrc/m1_usb_cdc_msc.c");
    TEST_ASSERT_NOT_NULL(app_uart);
    TEST_ASSERT_NOT_NULL(bridge);

    const char* init = strstr(app_uart, "void m1_uart_app_init(uint32_t baud)");
    const char* request = init ? strstr(init, "m1_uart_app_pause_requested = true;") : NULL;
    const char* wait_rx = init ? strstr(init, "!m1_uart_app_bridge_paused") : NULL;
    const char* wait_tx = init ? strstr(init, "!m1_uart_app_tx_bridge_paused") : NULL;
    const char* claim = init ? strstr(init, "m1_uart_app_owned = true;") : NULL;
    const char* pause_lock = init ? strstr(init, "taskENTER_CRITICAL();\n    m1_uart_app_pause_requested = true;") : NULL;
    const char* pause_unlock = pause_lock ? strstr(pause_lock, "taskEXIT_CRITICAL();") : NULL;
    TEST_ASSERT_NOT_NULL(request);
    TEST_ASSERT_NOT_NULL(wait_rx);
    TEST_ASSERT_NOT_NULL(wait_tx);
    TEST_ASSERT_NOT_NULL(claim);
    TEST_ASSERT_NOT_NULL(pause_lock);
    TEST_ASSERT_NOT_NULL(pause_unlock);
    TEST_ASSERT_TRUE(request < wait_rx);
    TEST_ASSERT_TRUE(wait_rx < claim);
    TEST_ASSERT_TRUE(pause_lock < pause_unlock);
    TEST_ASSERT_NOT_NULL(strstr(bridge, "m1_uart_app_bridge_paused = true;"));
    TEST_ASSERT_NOT_NULL(strstr(bridge, "m1_uart_app_tx_bridge_paused = true;"));
    TEST_ASSERT_NOT_NULL(strstr(bridge, "pdMS_TO_TICKS(20));"));

    free(app_uart);
    free(bridge);
}

void test_uart_task_handles_match_bridge_directions(void) {
    char* tasks = read_repo_file("m1_csrc/m1_tasks.c");
    char* bridge = read_repo_file("m1_csrc/m1_usb_cdc_msc.c");
    TEST_ASSERT_NOT_NULL(tasks);
    TEST_ASSERT_NOT_NULL(bridge);
    TEST_ASSERT_NOT_NULL(strstr(tasks, "&ser2usb_task_hdl);"));
    TEST_ASSERT_NOT_NULL(strstr(tasks, "assert(ser2usb_task_hdl!=NULL);"));
    TEST_ASSERT_NULL(strstr(tasks, "&usb2ser_task_hdl"));
    TEST_ASSERT_NOT_NULL(strstr(bridge, "usb2ser_task_hdl = osThreadNew(vUsb2SerTask"));
    free(tasks);
    free(bridge);
}

void test_usb_to_uart_dma_stops_and_acknowledges_before_reconfiguration(void) {
    char* app_uart = read_repo_file("m1_csrc/m1_uart_app.c");
    char* bridge = read_repo_file("m1_csrc/m1_usb_cdc_msc.c");
    TEST_ASSERT_NOT_NULL(app_uart);
    TEST_ASSERT_NOT_NULL(bridge);

    const char* init = strstr(app_uart, "void m1_uart_app_init(uint32_t baud)");
    const char* wait = init ? strstr(init, "!m1_uart_app_tx_bridge_paused") : NULL;
    const char* config = wait ? strstr(wait, "m1_usb_cdc_app_comconfig(baud);") : NULL;
    TEST_ASSERT_NOT_NULL(wait);
    TEST_ASSERT_NOT_NULL(config);
    TEST_ASSERT_TRUE(wait < config);
    TEST_ASSERT_NOT_NULL(strstr(init, "xTaskNotifyGive(usb2ser_task_hdl);"));

    const char* task = strstr(bridge, "void vUsb2SerTask(void *pvParameters)");
    TEST_ASSERT_NOT_NULL(task);
    TEST_ASSERT_NOT_NULL(strstr(task, "m1_uart_app_tx_bridge_paused = true;"));
    TEST_ASSERT_NOT_NULL(strstr(task, "m1_uart_app_pause_requested || dma_wait_ticks"));
    TEST_ASSERT_NOT_NULL(strstr(task, "HAL_UART_DMAStop(&huart_logdb);"));
    TEST_ASSERT_NOT_NULL(strstr(task, "if (m1_uart_app_pause_requested) break;"));
    const char* dma_gate = strstr(task, "pause_before_dma = m1_uart_app_pause_requested || m1_uart_app_owned;");
    const char* dma_start = dma_gate ? strstr(dma_gate, "HAL_UART_Transmit_DMA(&huart_logdb") : NULL;
    const char* dma_gate_end = dma_gate ? strstr(dma_gate, "taskEXIT_CRITICAL();") : NULL;
    TEST_ASSERT_NOT_NULL(dma_gate);
    TEST_ASSERT_NOT_NULL(dma_start);
    TEST_ASSERT_NOT_NULL(dma_gate_end);
    TEST_ASSERT_TRUE(dma_gate < dma_start);
    TEST_ASSERT_TRUE(dma_start < dma_gate_end);

    free(app_uart);
    free(bridge);
}

void test_cdc_defers_host_reconfiguration_during_uart_ownership(void) {
    char* content = read_repo_file("USB/Class/CDC/Src/usbd_cdc_if.c");
    TEST_ASSERT_NOT_NULL(content);
    TEST_ASSERT_NOT_NULL(strstr(content, "!m1_uart_app_owned"));
    TEST_ASSERT_NOT_NULL(strstr(content, "!m1_uart_app_pause_requested"));
    free(content);
}

void test_baud_change_keeps_uart_owned_without_bridge_handoff(void) {
    char* transport = read_repo_file("apps/meshtastic/src/transport.c");
    char* uart_app = read_repo_file("m1_csrc/m1_uart_app.c");
    TEST_ASSERT_NOT_NULL(transport);
    TEST_ASSERT_NOT_NULL(uart_app);
    const char* set_baud = strstr(transport, "void transport_set_baud(");
    const char* set_baud_end = set_baud ? strstr(set_baud, "\n}") : NULL;
    TEST_ASSERT_NOT_NULL(set_baud);
    TEST_ASSERT_NOT_NULL(set_baud_end);
    TEST_ASSERT_NULL(strstr(set_baud, "m1_uart_app_deinit();"));
    TEST_ASSERT_NOT_NULL(strstr(set_baud, "m1_uart_app_init(baud);"));
    const char* init = strstr(uart_app, "void m1_uart_app_init(uint32_t baud)");
    const char* already_owned = init ? strstr(init, "if(m1_uart_app_owned)") : NULL;
    const char* app_config = already_owned ? strstr(already_owned, "m1_usb_cdc_app_comconfig(baud);") : NULL;
    const char* fresh_pause = init ? strstr(init, "m1_uart_app_pause_requested = true;") : NULL;
    TEST_ASSERT_NOT_NULL(already_owned);
    TEST_ASSERT_NOT_NULL(app_config);
    TEST_ASSERT_NOT_NULL(fresh_pause);
    TEST_ASSERT_TRUE(already_owned < fresh_pause);
    free(transport);
    free(uart_app);
}

void test_keyboard_uses_text_input_and_services_protocol(void) {
    char* app = read_repo_file("apps/meshtastic/src/main.c");
    char* keyboard = read_repo_file("m1_csrc/m1_virtual_kb.c");
    TEST_ASSERT_NOT_NULL(app);
    TEST_ASSERT_NOT_NULL(keyboard);
    TEST_ASSERT_NOT_NULL(strstr(app, "m1_vkb_get_text(\"Message:\", \"\", buf, sizeof(buf))"));
    const char* service = strstr(app, "static void keyboard_service(void)");
    const char* poll = service ? strstr(service, "protocol_poll(app);") : NULL;
    const char* heartbeat = service ? strstr(service, "service_heartbeat(app);") : NULL;
    TEST_ASSERT_NOT_NULL(service);
    TEST_ASSERT_NOT_NULL(poll);
    TEST_ASSERT_NOT_NULL(heartbeat);
    TEST_ASSERT_TRUE(poll < heartbeat);
    TEST_ASSERT_NOT_NULL(strstr(app, "m1app_set_keyboard_service_callback(keyboard_service);"));
    TEST_ASSERT_NOT_NULL(strstr(keyboard, "xQueueReceive(main_q_hdl, &q_item, pdMS_TO_TICKS(20))"));
    TEST_ASSERT_NOT_NULL(strstr(keyboard, "s_keyboard_service_callback();"));
    free(app);
    free(keyboard);
}

void test_settings_ensure_parent_before_child_directory(void) {
    char* content = read_repo_file("apps/meshtastic/src/settings.c");
    TEST_ASSERT_NOT_NULL(content);
    const char* parent = strstr(content, "fs_directory_ensure(\"0:/apps_data\")");
    const char* child = strstr(content, "fs_directory_ensure(SETTINGS_DIR)");
    TEST_ASSERT_NOT_NULL(parent);
    TEST_ASSERT_NOT_NULL(child);
    TEST_ASSERT_TRUE(parent < child);
    free(content);
}

void test_snr_is_preserved_as_float_db(void) {
    char* state = read_repo_file("apps/meshtastic/src/mesh_app.h");
    char* roster_header = read_repo_file("apps/meshtastic/src/roster.h");
    char* roster = read_repo_file("apps/meshtastic/src/roster.c");
    char* protocol = read_repo_file("apps/meshtastic/src/protocol.c");
    char* gui = read_repo_file("apps/meshtastic/src/gui.c");
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(roster_header);
    TEST_ASSERT_NOT_NULL(roster);
    TEST_ASSERT_NOT_NULL(protocol);
    TEST_ASSERT_NOT_NULL(gui);
    TEST_ASSERT_NOT_NULL(strstr(state, "float last_rx_snr;"));
    TEST_ASSERT_NOT_NULL(strstr(state, "float last_snr;"));
    TEST_ASSERT_NOT_NULL(strstr(roster_header, "uint32_t node_id, float snr, int16_t rssi"));
    TEST_ASSERT_NOT_NULL(strstr(roster, "void roster_add_node(ZeroMeshApp* app, uint32_t node_id, float snr"));
    TEST_ASSERT_NOT_NULL(strstr(roster, "selected->last_snr * 10.0f"));
    TEST_ASSERT_NOT_NULL(strstr(roster, "Signal: SNR %c%d.%d / RSSI %d"));
    TEST_ASSERT_NOT_NULL(strstr(protocol, "app->last_rx_snr = p->rx_snr;"));
    TEST_ASSERT_NOT_NULL(strstr(protocol, "roster_add_node(app, sender_id, p->rx_snr, p->rx_rssi);"));
    TEST_ASSERT_NOT_NULL(strstr(protocol, "roster_add_node(app, n.num, n.snr, 0);"));
    TEST_ASSERT_NOT_NULL(strstr(gui, "app->last_rx_snr * 10.0f"));
    TEST_ASSERT_NULL(strstr(gui, "last_rx_snr * 10) / 4"));
    free(roster);
    free(roster_header);
    free(state);
    free(protocol);
    free(gui);
}

void test_nanopb_license_is_present_beside_vendored_sources(void) {
    char* content = read_repo_file("apps/meshtastic/lib/nanopb/LICENSE.txt");
    TEST_ASSERT_NOT_NULL(content);
    TEST_ASSERT_NOT_NULL(
        strstr(content, "3. This notice may not be removed or altered from any source"));
    free(content);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_app_manager_releases_uart_before_unloading_elf);
    RUN_TEST(test_uart_bridge_acknowledges_pause_before_app_claims_stream);
    RUN_TEST(test_uart_task_handles_match_bridge_directions);
    RUN_TEST(test_usb_to_uart_dma_stops_and_acknowledges_before_reconfiguration);
    RUN_TEST(test_cdc_defers_host_reconfiguration_during_uart_ownership);
    RUN_TEST(test_baud_change_keeps_uart_owned_without_bridge_handoff);
    RUN_TEST(test_keyboard_uses_text_input_and_services_protocol);
    RUN_TEST(test_settings_ensure_parent_before_child_directory);
    RUN_TEST(test_snr_is_preserved_as_float_db);
    RUN_TEST(test_nanopb_license_is_present_beside_vendored_sources);
    return UNITY_END();
}
