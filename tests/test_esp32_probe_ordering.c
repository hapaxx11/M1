/* See COPYING.txt for license details. */

/*
 * test_esp32_probe_ordering.c
 *
 * Source-level regression checks for issue #719 Phase 1: on real hardware the
 * Settings > Dashboard probe diagnostics (added in the Phase 0 work) read
 * back "ESP32 Unknown (fallback) - Probe Fail rcNN n0 at1 - caps 00000000 -
 * ATtask b1 a1" — the host AT task (spi_trans_control_task) was already
 * running (at_task_before == 1) by the time the brain-CD3 M1_RPC PING ran,
 * confirming the C1 hypothesis from documentation/agent/esp32-719-
 * compatibility-plan.md: SPI3 bus contention between that task and the
 * mutex-free full-duplex M1 Link transfer.
 *
 * Neither m1_esp32_caps_init() (m1_csrc/m1_esp32_caps.c) nor m1link_hal_xfer()
 * (Esp_spi_at/.../esp_app_main.c) can be exercised on the host: both call
 * concrete HAL/FreeRTOS/hardware entry points with no injectable seam. As
 * with the existing esp32_main_deinit() heap-leak regression test, these
 * checks verify the source-code invariants of the fix directly instead:
 *
 *   1. The M1_RPC brain probe (SYS_PING) is issued BEFORE esp32_main_init()
 *      (the host AT task start) in m1_esp32_caps_init(), so a brain that only
 *      speaks M1_RPC never starts or touches the AT task at all.
 *   2. m1link_hal_xfer() takes the shared SPI3 mutex around its transfer
 *      whenever the AT task has been created, so a PING issued while the AT
 *      task is already running (e.g. started by another feature before the
 *      dashboard/caps probe ever ran, matching the field report above) can
 *      no longer race a half-duplex AT transaction on the same peripheral.
 */

#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M1_ROOT
#error "M1_ROOT must be defined by CMake"
#endif

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

static void assert_contains(const char *content, const char *needle)
{
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(content, needle), needle);
}

static void assert_ordered(const char *content, const char *first,
                           const char *second)
{
    const char *p_first = strstr(content, first);
    const char *p_second = strstr(content, second);

    TEST_ASSERT_NOT_NULL_MESSAGE(p_first, first);
    TEST_ASSERT_NOT_NULL_MESSAGE(p_second, second);
    TEST_ASSERT_TRUE_MESSAGE(p_first < p_second, first);
}

void setUp(void)    {}
void tearDown(void) {}

/* ----- m1_esp32_caps.c: M1_RPC probe runs before the AT task starts ------ */

void test_rpc_ping_issued_before_at_task_start(void)
{
    char *content = read_file("m1_csrc/m1_esp32_caps.c");

    /* The M1_RPC PING transport call must precede esp32_main_init() (the
     * host AT task start) so a brain-only device is probed without ever
     * touching the AT task. */
    assert_ordered(content, "s_diag.rpc_attempted = 1u;",
                   "esp32_main_init();");

    free(content);
}

void test_at_task_start_precedes_at_probes(void)
{
    char *content = read_file("m1_csrc/m1_esp32_caps.c");

    /* The AT task must still be (re)started before the AT presence / AT+CMD?
     * probes run, for firmware that does not answer M1_RPC. */
    assert_ordered(content, "esp32_main_init();",
                   "spi_AT_send_recv(\"AT\\r\\n\", at_presence,");
    assert_ordered(content, "esp32_main_init();",
                   "spi_AT_send_recv(\"AT+CMD?\\r\\n\", at_resp,");

    free(content);
}

void test_caps_init_still_falls_back_to_unknown(void)
{
    char *content = read_file("m1_csrc/m1_esp32_caps.c");

    assert_contains(content, "\"Unknown (fallback)\"");
    assert_ordered(content, "esp32_main_init();",
                   "strncpy(s_fw_name, \"Unknown (fallback)\",");

    free(content);
}

/* ----- esp_app_main.c: mutex held for the full abort + exchange sequence ---- */

void test_spi_m1link_send_recv_bin_locks_spi_mutex_for_full_sequence(void)
{
    char *content = read_file(
        "Esp_spi_at/examples/at_spi_master/spi/stm32/main/esp_app_main.c");

    /* Anchor all checks to the body of spi_m1link_send_recv_bin so that
     * the HAL_SPI_Abort call inside m1link_hal_xfer (earlier in the file)
     * and any mention of m1_esp32_m1link_send_recv_timed in comments do not
     * pollute the ordered checks.  Anchor to the C function definition (which
     * has a return type) rather than the comment references to the name. */
    const char *fn_start = strstr(content, "uint8_t spi_m1link_send_recv_bin(");
    TEST_ASSERT_NOT_NULL_MESSAGE(fn_start, "spi_m1link_send_recv_bin definition not found");

    /* The full-sequence lock must wrap the abort AND the timed exchange, so
     * spi_trans_control_task cannot interpose between request and reply polls. */
    const char *lock_call  = strstr(fn_start, "m1link_lock_spi_if_at_task_present();");
    const char *abort_call = strstr(fn_start, "HAL_SPI_Abort(&hspi_esp);");
    const char *timed_call = strstr(fn_start, "m1_esp32_m1link_send_recv_timed(");
    const char *unlock_call= strstr(fn_start, "m1link_unlock_spi_if_at_task_present();");

    TEST_ASSERT_NOT_NULL(lock_call);
    TEST_ASSERT_NOT_NULL(abort_call);
    TEST_ASSERT_NOT_NULL(timed_call);
    TEST_ASSERT_NOT_NULL(unlock_call);

    TEST_ASSERT_TRUE_MESSAGE(lock_call  < abort_call,  "lock must precede abort");
    TEST_ASSERT_TRUE_MESSAGE(abort_call < timed_call,  "abort must precede timed exchange");
    TEST_ASSERT_TRUE_MESSAGE(timed_call < unlock_call, "timed exchange must precede unlock");

    /* m1link_hal_xfer must NOT take the per-transfer lock: doing so would
     * serialise individual SPI transactions but still allow the AT task to
     * interpose between the request unlock and the next IDLE poll.  Search
     * only within m1link_hal_xfer's text range (between its definition and
     * the start of spi_m1link_send_recv_bin) to avoid false positives. */
    const char *xfer_fn = strstr(content, "static int m1link_hal_xfer(");
    TEST_ASSERT_NOT_NULL(xfer_fn);
    TEST_ASSERT_TRUE_MESSAGE(fn_start > xfer_fn,
        "spi_m1link_send_recv_bin must come after m1link_hal_xfer");
    const char *xfer_lock = strstr(xfer_fn, "m1link_lock_spi_if_at_task_present();");
    /* If the call is found at all, it must be in spi_m1link_send_recv_bin
     * (at or after fn_start), not inside m1link_hal_xfer (before fn_start). */
    TEST_ASSERT_TRUE_MESSAGE(xfer_lock == NULL || xfer_lock >= fn_start,
        "m1link_hal_xfer must not take per-transfer SPI mutex");

    free(content);
}

/* ----- m1_wifi.c: firmware classification before AT task start ------------ */

void test_wifi_do_scan_classifies_before_starting_at_task(void)
{
    char *content = read_file("m1_csrc/m1_wifi.c");

    /* m1_esp32_caps_init() (classification) must run before esp32_main_init()
     * (AT task start) inside wifi_do_scan(), so a brain-CD3 device is probed
     * over M1_RPC without the AT task on SPI3. */
    assert_ordered(content, "m1_esp32_caps_init();",
                   "esp32_main_init();");

    free(content);
}

void test_wifi_do_scan_skips_at_task_for_rpc_transport(void)
{
    char *content = read_file("m1_csrc/m1_wifi.c");

    /* esp32_main_init() must be guarded by an ESP32_TRANSPORT_RPC check so
     * that the AT task is not started for brain-CD3 firmware. */
    assert_contains(content, "ESP32_TRANSPORT_RPC");
    assert_ordered(content, "ESP32_TRANSPORT_RPC",
                   "esp32_main_init();");

    free(content);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_rpc_ping_issued_before_at_task_start);
    RUN_TEST(test_at_task_start_precedes_at_probes);
    RUN_TEST(test_caps_init_still_falls_back_to_unknown);
    RUN_TEST(test_spi_m1link_send_recv_bin_locks_spi_mutex_for_full_sequence);
    RUN_TEST(test_wifi_do_scan_classifies_before_starting_at_task);
    RUN_TEST(test_wifi_do_scan_skips_at_task_for_rpc_transport);
    return UNITY_END();
}
