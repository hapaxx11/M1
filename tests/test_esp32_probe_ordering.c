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

/* ----- esp_app_main.c: m1link_hal_xfer() takes the shared SPI3 mutex ----- */

void test_m1link_hal_xfer_locks_shared_spi_mutex(void)
{
    char *content = read_file(
        "Esp_spi_at/examples/at_spi_master/spi/stm32/main/esp_app_main.c");

    assert_contains(content, "m1link_lock_spi_if_at_task_present");
    assert_contains(content, "m1link_unlock_spi_if_at_task_present");

    /* Guarded so it is safe to call before the AT task (and its mutex) has
     * ever been created. */
    assert_contains(content,
        "static void m1link_lock_spi_if_at_task_present(void)\n"
        "{\n"
        "\tif (pxMutex)\n"
        "\t\tspi_mutex_lock();\n"
        "}");

    /* The lock must wrap the actual SPI transaction inside m1link_hal_xfer(). */
    assert_ordered(content, "m1link_lock_spi_if_at_task_present();",
                   "HAL_SPI_TransmitReceive(&hspi_esp,");
    assert_ordered(content, "HAL_SPI_TransmitReceive(&hspi_esp,",
                   "m1link_unlock_spi_if_at_task_present();");

    free(content);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_rpc_ping_issued_before_at_task_start);
    RUN_TEST(test_at_task_start_precedes_at_probes);
    RUN_TEST(test_caps_init_still_falls_back_to_unknown);
    RUN_TEST(test_m1link_hal_xfer_locks_shared_spi_mutex);
    return UNITY_END();
}
