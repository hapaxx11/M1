/* See COPYING.txt for license details. */

/*
 * test_esp32_main_deinit_releases_legacy_task.c
 *
 * Source-level regression checks for esp32_main_deinit().
 *
 * Bug context: After switching to the SiN360 binary-SPI ESP32 firmware in
 * 0.9.1, Sub-GHz Read Raw failed with "Record failed / Check SD card or free
 * memory" whenever an ESP32-dependent feature (Bad-BT, Zigbee scan, ESP32 FW
 * update) had been used first in the same session.  The root cause was that
 * esp32_main_init() creates ~17 KB of FreeRTOS objects (8 KB task stack,
 * 4 KB trans_data, 4 KB stream buffer, misc semaphores/queues) that were
 * never released when m1_esp32_deinit() ran.  This left too little heap for
 * the 128 KB front-buffer + 28 KB SD-writer reserve needed by Read Raw.
 *
 * The fix adds esp32_main_deinit(), which signals spi_trans_control_task to
 * exit via a sentinel, waits for the task to free its private trans_data
 * allocation and notify the caller, then tears down all remaining RTOS objects.
 * m1_esp32_deinit() calls esp32_main_deinit() unconditionally (guarded by
 * get_esp32_main_init_status()), so every caller of m1_esp32_deinit()
 * automatically reclaims the heap.
 *
 * These tests verify the source-code invariants without running firmware.
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

static void assert_ordered(const char *content, const char *first, const char *second)
{
    const char *p_first = strstr(content, first);
    const char *p_second = strstr(content, second);

    TEST_ASSERT_NOT_NULL_MESSAGE(p_first, first);
    TEST_ASSERT_NOT_NULL_MESSAGE(p_second, second);
    TEST_ASSERT_TRUE_MESSAGE(p_first < p_second, first);
}

void setUp(void) {}
void tearDown(void) {}

/* ----- esp_app_main.h: public declaration --------------------------------- */

void test_esp32_main_deinit_declared_in_header(void)
{
    char *content = read_file(
        "Esp_spi_at/examples/at_spi_master/spi/stm32/main/esp_app_main.h");

    assert_contains(content, "void esp32_main_deinit(void);");

    free(content);
}

/* ----- esp_app_main.c: stop-flag and sentinel ----------------------------- */

void test_esp32_main_deinit_sets_stop_flag_and_sends_sentinel(void)
{
    char *content = read_file(
        "Esp_spi_at/examples/at_spi_master/spi/stm32/main/esp_app_main.c");

    /* The stop flag must be declared as volatile to prevent the compiler
     * from caching the value across the task boundary. */
    assert_contains(content, "volatile bool s_esp32_stop_task");

    /* The deinit function must set the flag before waking the task */
    assert_ordered(content, "s_esp32_stop_task = true;",
                   "xQueueSendToFront(esp_spi_msg_queue");

    free(content);
}

void test_spi_task_checks_stop_flag_before_acquiring_mutex(void)
{
    char *content = read_file(
        "Esp_spi_at/examples/at_spi_master/spi/stm32/main/esp_app_main.c");

    /* The stop flag check comment and break are unique to the task loop.
     * Verify the sentinel block appears before any SPI mutex acquisition
     * WITHIN the same task context by checking a unique surrounding string
     * from the task loop body against the stop-flag sentinel comment. */
    assert_ordered(content,
                   "/* Graceful-exit sentinel: deinit caller set the stop flag",
                   "spi_recv_opt_t recv_opt = query_slave_data_trans_info()");

    free(content);
}

/* ----- esp_app_main.c: task notifies caller and frees trans_data ---------- */

void test_spi_task_notifies_deinit_caller_before_self_delete(void)
{
    char *content = read_file(
        "Esp_spi_at/examples/at_spi_master/spi/stm32/main/esp_app_main.c");

    /* Task gives the dedicated join semaphore BEFORE vTaskDelete so the
     * deinit caller can safely clean up RTOS objects once trans_data is
     * freed.  xSemaphoreGive is used instead of xTaskNotify so that an
     * unrelated pending notification on the caller's task cannot be
     * mistaken for our exit signal. */
    assert_ordered(content, "xSemaphoreGive(s_deinit_sync_sem)", "vTaskDelete(NULL)");

    /* trans_data must be freed before the notification */
    assert_ordered(content, "free(trans_data);", "xSemaphoreGive(s_deinit_sync_sem)");

    /* The give must be guarded against a NULL semaphore handle (creation can
     * fail under low-memory conditions).  Without the guard, giving a NULL
     * handle crashes the scheduler. */
    assert_contains(content, "if (s_deinit_sync_sem)");

    free(content);
}

void test_esp32_main_deinit_waits_for_task_notification(void)
{
    char *content = read_file(
        "Esp_spi_at/examples/at_spi_master/spi/stm32/main/esp_app_main.c");

    /* Deinit must use a dedicated binary semaphore for the task join.
     * xSemaphoreTake blocks until the task gives s_deinit_sync_sem, which is
     * a distinct primitive from xTaskNotify so unrelated task notifications
     * cannot cause a spurious early return. */
    assert_contains(content, "xSemaphoreTake(s_deinit_sync_sem,");

    /* The return value must be checked.  If the take times out the task has
     * not acknowledged shutdown; shared RTOS objects must NOT be deleted. */
    assert_contains(content, "joined != pdTRUE");

    /* The stop flag must be cleared AFTER all shared RTOS objects have been
     * freed, not before.  This prevents a late-waking task from re-entering
     * normal SPI operation after its queue/semaphores have already been freed.
     * Anchor the first arg on the queue-local-delete, and the second on the
     * unique block-opening comment (including the C comment start) so the
     * variable-initialiser `= false` at the top of the file is not matched. */
    assert_ordered(content, "vQueueDelete(q)",
                   "/* Clear the stop flag only after all shared objects");

    free(content);
}

/* ----- esp_app_main.c: all RTOS objects freed ----------------------------- */

void test_esp32_main_deinit_frees_all_rtos_objects(void)
{
    char *content = read_file(
        "Esp_spi_at/examples/at_spi_master/spi/stm32/main/esp_app_main.c");

    /* FreeRTOS queue: NULL the global BEFORE delete (via local copy 'q') so
     * the ISR NULL-guard in ESP32_GPIO_EXTI_Callback sees NULL before the
     * handle is freed.  Check both the NULL assignment and the local delete. */
    assert_contains(content, "esp_spi_msg_queue = NULL;");
    assert_contains(content, "vQueueDelete(q)");
    /* Stream buffer for AT command transmit */
    assert_contains(content, "vStreamBufferDelete(spi_master_tx_ring_buf)");
    /* Mutex protecting SPI transactions */
    assert_contains(content, "vSemaphoreDelete(pxMutex)");
    /* Request and response semaphores */
    assert_contains(content, "vSemaphoreDelete(esp_ctrl_req_sem)");
    assert_contains(content, "vSemaphoreDelete(esp_resp_read_sem)");
    /* Join semaphore freed once the task has confirmed exit */
    assert_contains(content, "vSemaphoreDelete(s_deinit_sync_sem)");
    /* SPI device handle and host struct (pvPortMalloc in init_master_hd) */
    assert_contains(content, "vPortFree(spi_dev_handle->host)");
    assert_contains(content, "vPortFree(spi_dev_handle)");
    /* Linked-list ctrl_msg_Q */
    assert_contains(content, "esp_queue_destroy(&ctrl_msg_Q)");

    free(content);
}

void test_esp32_main_deinit_resets_init_flag(void)
{
    char *content = read_file(
        "Esp_spi_at/examples/at_spi_master/spi/stm32/main/esp_app_main.c");

    /* The guard flag must be cleared so a subsequent esp32_main_init() call
     * is not skipped after a deinit/re-init cycle. */
    assert_contains(content, "esp32_main_init_done = false;");

    free(content);
}

/* ----- m1_esp32_hal.c: hooks deinit into the HAL teardown path ----------- */

void test_m1_esp32_deinit_calls_esp32_main_deinit(void)
{
    char *content = read_file("m1_csrc/m1_esp32_hal.c");

    /* The HAL deinit must include the esp_app_main header */
    assert_contains(content, "#include \"esp_app_main.h\"");

    /* It must guard the call so it is skipped when the legacy task was
     * never started (e.g. pure binary-SPI / SiN360 sessions). */
    assert_contains(content, "get_esp32_main_init_status()");
    assert_contains(content, "esp32_main_deinit();");

    /* The call must come BEFORE esp32_init_done is cleared, so
     * get_esp32_main_init_status() can still return the correct value.
     * Use http_ssl_reset() as anchor — it is a unique call at the very
     * end of m1_esp32_deinit(), after the init flags are cleared. */
    assert_ordered(content, "esp32_main_deinit();", "http_ssl_reset();");

    free(content);
}

void test_m1_esp32_deinit_calls_main_deinit_after_uart_deinit(void)
{
    char *content = read_file("m1_csrc/m1_esp32_hal.c");

    /* The legacy AT UART transport must be torn down BEFORE we signal
     * the SPI task to exit, so no stale UART-driven callbacks can fire
     * after esp32_main_deinit() has freed the semaphores/queues. */
    assert_ordered(content, "esp32_UART_deinit();", "esp32_main_deinit();");

    free(content);
}

void test_m1_esp32_deinit_stops_task_before_spi_hardware_teardown(void)
{
    char *content = read_file("m1_csrc/m1_esp32_hal.c");

    /* The AT-over-SPI task uses spi_device_polling_transmit() on hspi_esp.
     * The task MUST be stopped (esp32_main_deinit) before the SPI3 peripheral
     * is deinitialized, otherwise a mid-transaction could race against
     * HAL_SPI_DeInit() / __HAL_RCC_SPI3_CLK_DISABLE(). */
    assert_ordered(content, "esp32_main_deinit();", "HAL_SPI_DeInit(&hspi_esp)");
    assert_ordered(content, "esp32_main_deinit();", "__HAL_RCC_SPI3_CLK_DISABLE()");

    free(content);
}

/* ----- esp_app_main.c: ISR-safe queue teardown ---------------------------- */

void test_esp32_main_deinit_nulls_queue_before_delete(void)
{
    char *content = read_file(
        "Esp_spi_at/examples/at_spi_master/spi/stm32/main/esp_app_main.c");

    /* The global queue handle must be NULLed BEFORE vQueueDelete() so that
     * a concurrent ISR (ESP32_GPIO_EXTI_Callback) that reads the global and
     * then calls xQueueSendFromISR() will hit the NULL guard and skip the
     * send rather than touching a freed handle.  The actual delete uses a
     * local copy of the handle. */
    /* Local copy must be taken BEFORE the global is NULLed, so the handle
     * remains valid for the vQueueDelete(q) call. */
    assert_ordered(content, "QueueHandle_t q = esp_spi_msg_queue;",
                   "esp_spi_msg_queue = NULL;");
    assert_ordered(content, "esp_spi_msg_queue = NULL;", "vQueueDelete(q)");

    free(content);
}

/* ----- m1_esp32_hal.c: EXTI disabled before queue teardown ---------------- */

void test_m1_esp32_deinit_disables_exti_before_task_deinit(void)
{
    char *content = read_file("m1_csrc/m1_esp32_hal.c");

    /* The ESP32 EXTI IRQ lines feed esp_spi_msg_queue from the ISR.
     * Both DATAREADY and HANDSHAKE lines MUST be disabled before
     * esp32_main_deinit() deletes the queue, otherwise a concurrent ISR
     * can enqueue into a freed handle. */
    assert_ordered(content,
                   "HAL_NVIC_DisableIRQ((IRQn_Type)(ESP32_DATAREADY_EXTI_IRQn))",
                   "esp32_main_deinit();");
    assert_ordered(content,
                   "HAL_NVIC_DisableIRQ((IRQn_Type)(ESP32_HANDSHAKE_EXTI_IRQn))",
                   "esp32_main_deinit();");

    free(content);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_esp32_main_deinit_declared_in_header);
    RUN_TEST(test_esp32_main_deinit_sets_stop_flag_and_sends_sentinel);
    RUN_TEST(test_spi_task_checks_stop_flag_before_acquiring_mutex);
    RUN_TEST(test_spi_task_notifies_deinit_caller_before_self_delete);
    RUN_TEST(test_esp32_main_deinit_waits_for_task_notification);
    RUN_TEST(test_esp32_main_deinit_frees_all_rtos_objects);
    RUN_TEST(test_esp32_main_deinit_resets_init_flag);
    RUN_TEST(test_m1_esp32_deinit_calls_esp32_main_deinit);
    RUN_TEST(test_m1_esp32_deinit_calls_main_deinit_after_uart_deinit);
    RUN_TEST(test_m1_esp32_deinit_stops_task_before_spi_hardware_teardown);
    RUN_TEST(test_esp32_main_deinit_nulls_queue_before_delete);
    RUN_TEST(test_m1_esp32_deinit_disables_exti_before_task_deinit);
    return UNITY_END();
}
