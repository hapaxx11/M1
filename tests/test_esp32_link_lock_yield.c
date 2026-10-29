/* See COPYING.txt for license details. */

/*
 * test_esp32_link_lock_yield.c
 *
 * Source-level regression checks for ESP32 transport regressions:
 *
 *   1. CD3 brain (M1_RPC) fully broken — "AP scan failed" on every WiFi
 *      scan: PR #809 introduced esp32_m1link_spi_mutex_init() but never
 *      called it, so s_m1link_spi_mutex stayed NULL, esp32_m1link_spi_lock()
 *      always failed, and every M1 Link transfer returned
 *      CTRL_ERR_TRANSPORT_SEND before clocking a byte.  The lock must create
 *      the mutex lazily on first use.
 *
 *   2. Bluetooth Direct boot freeze / watchdog boot loop: the ESP32 probe
 *      chain (reachable from the high-priority RPC task when Bluetooth
 *      Direct is enabled) busy-spun in HAL_Delay — up to ~10 s contiguous
 *      in init_master_hd()'s 20 x 500 ms retry loop — starving the menu
 *      task and the watchdog feeder.  Those waits must yield (vTaskDelay)
 *      once the scheduler is running.
 *
 *   3. Bluetooth Direct probe starving the supervised RPC task: even with
 *      yielding waits, a multi-second probe iteration stops m1_rpc_task's
 *      watchdog heartbeat.  rpc_brain_relay_poll() must suspend
 *      M1_REPORT_ID_RPC_TASK supervision around the probe (mirroring the
 *      ESP32 flash-op pattern) and the task must reset its heartbeat
 *      baseline after a probe iteration.
 *
 *   4. Scan-job use-after-free guard: the bounded UI wait in
 *      wifi_do_scan() must orphan (not free) a job whose worker outlives
 *      the cancel+drain window.  Decision logic is pure-tested in
 *      test_wifi_scan_wait.c.
 *
 * The fault sites call concrete HAL/FreeRTOS/hardware entry points with no
 * injectable seam, so (as in test_esp32_probe_ordering.c) the checks verify
 * the source-code invariants of the fixes directly.
 */

#include "unity.h"

#include <stdbool.h>
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

    /* Normalise CRLF -> LF in place: on Windows checkouts with
     * core.autocrlf=true the working tree is CRLF, and needles that span a
     * newline would never match even though the committed (LF) source and CI
     * are correct. */
    {
        char *rd = buf;
        char *wr = buf;
        while (*rd != '\0') {
            if (*rd != '\r')
                *wr++ = *rd;
            rd++;
        }
        *wr = '\0';
    }
    return buf;
}

/* Return a malloc'd copy of the text between the first occurrences of
 * @p begin and @p end (end exclusive). */
static char *slice_between(const char *content, const char *begin,
                           const char *end)
{
    const char *p_begin = strstr(content, begin);
    const char *p_end;
    size_t len;
    char *out;

    TEST_ASSERT_NOT_NULL_MESSAGE(p_begin, begin);
    p_end = strstr(p_begin + strlen(begin), end);
    TEST_ASSERT_NOT_NULL_MESSAGE(p_end, end);
    TEST_ASSERT_TRUE_MESSAGE(p_end > p_begin, "slice end before begin");

    len = (size_t)(p_end - p_begin);
    out = (char *)malloc(len + 1U);
    TEST_ASSERT_NOT_NULL(out);
    memcpy(out, p_begin, len);
    out[len] = '\0';
    return out;
}

static size_t count_occurrences(const char *haystack, const char *needle)
{
    size_t count = 0;
    const char *p = haystack;

    while ((p = strstr(p, needle)) != NULL) {
        count++;
        p += strlen(needle);
    }
    return count;
}

void setUp(void)    {}
void tearDown(void) {}

/* ----- Fix 1: the M1 Link SPI mutex is created before it is taken ------- */

void test_m1link_spi_lock_creates_mutex_lazily(void)
{
    char *content = read_file(
        "Esp_spi_at/examples/at_spi_master/spi/stm32/main/esp_app_main.c");

    char *lock_body = slice_between(content,
        "bool esp32_m1link_spi_lock_timed(uint32_t timeout_ms)",
        "void esp32_m1link_spi_unlock(void)");

    /* The lock must initialise the mutex before taking it; without this the
     * mutex stays NULL and every brain-CD3 M1_RPC transfer fails. */
    const char *init_call = strstr(lock_body, "esp32_m1link_spi_mutex_init()");
    const char *take_call = strstr(lock_body, "esp32_m1link_lifecycle_lock_timed(timeout_ms)");
    TEST_ASSERT_NOT_NULL_MESSAGE(init_call,
        "esp32_m1link_spi_lock() must call esp32_m1link_spi_mutex_init()");
    TEST_ASSERT_NOT_NULL(take_call);
    TEST_ASSERT_TRUE_MESSAGE(init_call < take_call,
        "mutex init must precede the take");

    free(lock_body);

    /* The initialiser must have at least one call site beyond its own
     * definition and the header declaration — the original bug was a
     * defined-but-never-called initialiser. */
    TEST_ASSERT_GREATER_THAN_size_t_MESSAGE(2u,
        count_occurrences(content, "esp32_m1link_spi_mutex_init"),
        "esp32_m1link_spi_mutex_init() needs a real call site");

    free(content);
}

/* ----- Fix 2: probe-path waits yield instead of busy-spinning ----------- */

void test_init_master_hd_retry_loop_yields(void)
{
    char *content = read_file(
        "Esp_spi_at/examples/at_spi_master/spi/stm32/main/esp_app_main.c");

    char *body = slice_between(content,
        "static void init_master_hd(",
        "} // static void init_master_hd");

    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(body, "esp32_yield_delay_ms(500)"),
        "init_master_hd retry loop must yield between retries");
    TEST_ASSERT_NULL_MESSAGE(strstr(body, "HAL_Delay("),
        "init_master_hd must not busy-spin in HAL_Delay "
        "(up to ~10 s starves the watchdog feeder)");

    free(body);
    free(content);
}

void test_reset_slave_delay_yields(void)
{
    char *content = read_file(
        "Esp_spi_at/examples/at_spi_master/spi/stm32/main/esp_app_main.c");

    char *body = slice_between(content,
        "static void reset_slave(void)",
        "void esp32_main_init(void)");

    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(body, "esp32_yield_delay_ms("),
        "reset_slave must use the yield-aware delay");
    TEST_ASSERT_NULL_MESSAGE(strstr(body, "HAL_Delay("),
        "reset_slave must not busy-spin in HAL_Delay");

    free(body);
    free(content);
}

void test_esp32_cmd_module_has_no_bare_hal_delay_waits(void)
{
    char *content = read_file("m1_csrc/m1_esp32_cmd.c");

    /* After the fix the only HAL_Delay( left in this module is the
     * pre-scheduler fallback inside esp32_cmd_delay_ms() itself. */
    TEST_ASSERT_EQUAL_size_t_MESSAGE(1u,
        count_occurrences(content, "HAL_Delay("),
        "m1_esp32_cmd.c must route all waits through esp32_cmd_delay_ms()");

    const char *helper = strstr(content, "static void esp32_cmd_delay_ms(");
    const char *hal_delay = strstr(content, "HAL_Delay(");
    TEST_ASSERT_NOT_NULL(helper);
    TEST_ASSERT_NOT_NULL(hal_delay);
    TEST_ASSERT_TRUE_MESSAGE(hal_delay > helper,
        "the remaining HAL_Delay must be the helper's pre-scheduler fallback");

    free(content);
}

/* ----- Hardening: bounded transaction lock in (de)init --------------------- */

void test_esp32_init_deinit_use_bounded_transaction_lock(void)
{
    char *content = read_file("m1_csrc/m1_esp32_hal.c");

    /* m1_esp32_init()/m1_esp32_deinit() must not wait on the RPC transaction
     * mutex with portMAX_DELAY: a stuck transaction holder (e.g. a wedged
     * WiFi scan worker) would wedge (de)init — and its caller — forever. */
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u,
        count_occurrences(content, "m1_esp32_rpc_transaction_lock()"),
        "(de)init must not use the unbounded transaction lock");
    TEST_ASSERT_GREATER_THAN_size_t_MESSAGE(1u,
        count_occurrences(content, "m1_esp32_rpc_transaction_lock_timed("),
        "(de)init must use the bounded transaction lock variant");

    free(content);
}

/* ----- Hardening: WiFi scan worker stack and bounded UI wait --------------- */

void test_wifi_scan_worker_stack_has_margin(void)
{
    char *content = read_file("m1_csrc/m1_esp32_rpc_features.c");

    /* The worker's worst-case chain (RPC call staging + 2 x 512 B M1 Link
     * frame buffers + HAL) is ~1.6-1.8 KiB; a 512-word (2 KiB) stack left
     * almost no margin. */
    const char *create = strstr(content, "xTaskCreate(wifi_scan_worker");
    TEST_ASSERT_NOT_NULL_MESSAGE(create, "wifi_scan_worker xTaskCreate not found");
    /* Scan only the argument list (next ~120 chars) for the old 512-word
     * stack size. */
    int found_old_stack = 0;
    for (int i = 0; i < 120 && create[i] != '\0' && create[i] != ';'; i++) {
        if (strncmp(&create[i], "512u", 4) == 0) {
            found_old_stack = 1;
            break;
        }
    }
    TEST_ASSERT_FALSE_MESSAGE(found_old_stack,
        "wifi_scan_worker stack must exceed 512 words");

    free(content);
}

void test_wifi_do_scan_poll_loop_is_bounded(void)
{
    char *content = read_file("m1_csrc/m1_wifi.c");

    /* An unbounded async_poll loop hangs the UI forever if the scan worker
     * dies without signalling; the loop must be able to cancel and bail.
     * The async context now lives in the heap-allocated scan job. */
    const char *poll   = strstr(content,
                                "m1_esp32_rpc_wifi_scan_async_poll(&job->ctx)");
    const char *cancel = poll ? strstr(poll,
                                "m1_esp32_rpc_wifi_scan_async_cancel(&job->ctx)")
                              : NULL;
    TEST_ASSERT_NOT_NULL(poll);
    TEST_ASSERT_NOT_NULL_MESSAGE(cancel,
        "wifi_do_scan() RPC poll loop must have a bounded cancel path");

    free(content);
}

/* ----- Watchdog: RPC-task supervision suspended around the boot probe ------ */

void test_relay_poll_suspends_supervision_around_probe(void)
{
    char *content = read_file("m1_csrc/m1_rpc.c");

    /* rpc_brain_relay_poll() runs on the watchdog-supervised m1_rpc_task.
     * Its ESP32 bring-up / capability probe can block for many seconds
     * (AT-fallback retry loop), so supervision must be suspended BEFORE the
     * probe and resumed on every path out of the probe section — mirroring
     * the deferred ESP32 flash-op pattern. */
    char *body = slice_between(content,
        "static bool rpc_brain_relay_poll(TickType_t *supervised_start)",
        "void m1_rpc_task(void *param)");

    const char *suspend = strstr(body,
        "m1_wdt_suspend_task(M1_REPORT_ID_RPC_TASK)");
    const char *probe   = strstr(body, "m1_esp32_init();");
    TEST_ASSERT_NOT_NULL_MESSAGE(suspend,
        "relay poll must suspend RPC-task supervision around the probe");
    TEST_ASSERT_NOT_NULL(probe);
    TEST_ASSERT_TRUE_MESSAGE(suspend < probe,
        "supervision suspend must precede the ESP32 probe");

    /* Every exit from the probe section resumes supervision. */
    TEST_ASSERT_GREATER_THAN_size_t_MESSAGE(2u,
        count_occurrences(body, "m1_wdt_resume_task(M1_REPORT_ID_RPC_TASK)"),
        "every probe exit path must resume supervision");

    free(body);
    free(content);
}

void test_rpc_task_resets_heartbeat_baseline_after_probe(void)
{
    char *content = read_file("m1_csrc/m1_rpc.c");

    /* After a probe iteration the task must reset rpc_wdt_last, otherwise
     * the probe seconds are reported as loop run-time and trip the
     * maximum-report-percent check on the next heartbeat. */
    const char *call = strstr(content,
        "if (rpc_brain_relay_poll(&rpc_wdt_probe_resume_tick))");
    TEST_ASSERT_NOT_NULL_MESSAGE(call,
        "m1_rpc_task must act on rpc_brain_relay_poll()'s probe flag");
    const char *reset = strstr(call,
        "rpc_wdt_last = rpc_wdt_probe_resume_tick;");
    TEST_ASSERT_NOT_NULL_MESSAGE(reset,
        "heartbeat baseline must use the exact probe-resume tick");
    TEST_ASSERT_TRUE_MESSAGE(reset - call < 400,
        "baseline reset must sit inside the probe-flag block");

    const char *resume = strstr(content,
        "m1_wdt_resume_task(M1_REPORT_ID_RPC_TASK);");
    const char *resume_tick = resume
        ? strstr(resume, "*supervised_start = xTaskGetTickCount();")
        : NULL;
    TEST_ASSERT_NOT_NULL_MESSAGE(resume_tick,
        "supervised timing must restart immediately after probe resume");

    free(content);
}

void test_m1link_lock_chain_honors_response_timeout(void)
{
    char *content = read_file(
        "Esp_spi_at/examples/at_spi_master/spi/stm32/main/esp_app_main.c");

    const char *lifecycle = strstr(content,
        "esp32_m1link_lifecycle_lock_timed(uint32_t timeout_ms)");
    const char *lifecycle_take = lifecycle
        ? strstr(lifecycle,
            "pdMS_TO_TICKS(timeout_ms)) == pdPASS")
        : NULL;
    const char *lock = strstr(content,
        "esp32_m1link_spi_lock_timed(uint32_t timeout_ms)");
    const char *at_take = lock
        ? strstr(lock,
            "xSemaphoreTake(pxMutex, pdMS_TO_TICKS(remaining_timeout_ms))")
        : NULL;
    const char *cleanup = at_take
        ? strstr(at_take, "esp32_m1link_lifecycle_unlock();")
        : NULL;
    const char *caller = strstr(content, "esp32_m1link_spi_lock_timed(timeout_ms)");

    bool complete = lifecycle_take != NULL && at_take != NULL &&
                    cleanup != NULL && caller != NULL;

    free(content);

    TEST_ASSERT_TRUE(complete);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_m1link_lock_chain_honors_response_timeout);
    RUN_TEST(test_m1link_spi_lock_creates_mutex_lazily);
    RUN_TEST(test_init_master_hd_retry_loop_yields);
    RUN_TEST(test_reset_slave_delay_yields);
    RUN_TEST(test_esp32_cmd_module_has_no_bare_hal_delay_waits);
    RUN_TEST(test_esp32_init_deinit_use_bounded_transaction_lock);
    RUN_TEST(test_wifi_scan_worker_stack_has_margin);
    RUN_TEST(test_wifi_do_scan_poll_loop_is_bounded);
    RUN_TEST(test_relay_poll_suspends_supervision_around_probe);
    RUN_TEST(test_rpc_task_resets_heartbeat_baseline_after_probe);
    return UNITY_END();
}
