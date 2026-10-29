#include "unity.h"
#include <stdio.h>
#include <string.h>

static char source[65536];
static char tasks_source[65536];

#ifndef M1_ROOT
#error "M1_ROOT must be defined by CMake"
#endif

void setUp(void)
{
    char path[512];
    FILE *fp;
    size_t len;

    snprintf(path, sizeof(path), "%s/m1_csrc/m1_log_debug.c", M1_ROOT);
    fp = fopen(path, "rb");
    TEST_ASSERT_NOT_NULL_MESSAGE(fp, path);
    len = fread(source, 1, sizeof(source) - 1u, fp);
    fclose(fp);
    TEST_ASSERT_LESS_THAN(sizeof(source) - 1u, len);
    source[len] = '\0';

    snprintf(path, sizeof(path), "%s/m1_csrc/m1_tasks.c", M1_ROOT);
    fp = fopen(path, "rb");
    TEST_ASSERT_NOT_NULL_MESSAGE(fp, path);
    len = fread(tasks_source, 1, sizeof(tasks_source) - 1u, fp);
    fclose(fp);
    TEST_ASSERT_LESS_THAN(sizeof(tasks_source) - 1u, len);
    tasks_source[len] = '\0';
}

void tearDown(void) {}

void test_gpdma_clock_is_enabled_before_any_dma_initialization(void)
{
    char *clock = strstr(source, "__HAL_RCC_GPDMA1_CLK_ENABLE();");
    char *rx_init = strstr(source, "HAL_DMAEx_List_Init(&hdma_rxlogdb)");
    char *tx_init = strstr(source, "HAL_DMA_Init(&hdma_logdb)");

    TEST_ASSERT_NOT_NULL(clock);
    TEST_ASSERT_NOT_NULL(rx_init);
    TEST_ASSERT_NOT_NULL(tx_init);
    TEST_ASSERT_LESS_THAN(rx_init, clock);
    TEST_ASSERT_LESS_THAN(tx_init, clock);
}

void test_log_queue_and_ring_buffer_exist_before_dma_irq(void)
{
    char *queue = strstr(source, "log_q_hdl = xQueueCreate");
    char *ring = strstr(source, "m1_ringbuffer_init(&logdb_tx_rb");
    char *irq = strstr(source, "HAL_NVIC_EnableIRQ(GPDMA1_Channel1_IRQn)");

    TEST_ASSERT_NOT_NULL(queue);
    TEST_ASSERT_NOT_NULL(ring);
    TEST_ASSERT_NOT_NULL(irq);
    TEST_ASSERT_LESS_THAN(irq, queue);
    TEST_ASSERT_LESS_THAN(irq, ring);
}

void test_uart_receive_is_armed_before_uart_irq(void)
{
    char *receive = strstr(source, "HAL_UART_Receive_IT(&huart_logdb");
    char *irq = strstr(source, "HAL_NVIC_EnableIRQ(LOG_DEBUG_UART_IRQn)");

    TEST_ASSERT_NOT_NULL(receive);
    TEST_ASSERT_NOT_NULL(irq);
    TEST_ASSERT_LESS_THAN(irq, receive);
}

void test_logger_irq_activation_is_deferred_until_task_exists(void)
{
    char *start = strstr(source, "void m1_logdb_irq_start(void)");
    char *guard = start ? strstr(start, "log_db_task_hdl == NULL") : NULL;
    char *task_create = strstr(tasks_source, "xTaskCreate(log_db_handler_task");
    char *task_start = strstr(tasks_source, "m1_logdb_irq_start();");

    TEST_ASSERT_NOT_NULL(start);
    TEST_ASSERT_NOT_NULL(guard);
    TEST_ASSERT_NOT_NULL(task_create);
    TEST_ASSERT_NOT_NULL(task_start);
    TEST_ASSERT_LESS_THAN(task_start, task_create);
}

void test_log_queue_uses_static_allocation(void)
{
    char *queue = strstr(source, "log_q_hdl = xQueueCreateStatic(");
    char *storage = strstr(source, "log_q_storage[1]");
    char *control = strstr(source, "log_q_control");

    TEST_ASSERT_NOT_NULL(queue);
    TEST_ASSERT_NOT_NULL(storage);
    TEST_ASSERT_NOT_NULL(control);
}

void test_rx_dma_irq_is_also_deferred_until_task_exists(void)
{
    char *start = strstr(source, "void m1_logdb_irq_start(void)");
    char *channel2 = strstr(source, "HAL_NVIC_EnableIRQ(GPDMA1_Channel2_IRQn)");

    TEST_ASSERT_NOT_NULL(start);
    TEST_ASSERT_NOT_NULL(channel2);
    TEST_ASSERT_TRUE(channel2 > start);
}

void test_log_formatting_waits_for_logger_task(void)
{
    char *fn = strstr(source, "if (level > m1_logdb.log_level)");
    char *guard = fn ? strstr(fn, "log_db_task_hdl == NULL") : NULL;
    char *allocation = fn ? strstr(fn, "malloc(M1_LOGDB_MESSAGE_SIZE") : NULL;

    TEST_ASSERT_NOT_NULL(guard);
    TEST_ASSERT_NOT_NULL(allocation);
    TEST_ASSERT_LESS_THAN(allocation, guard);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_gpdma_clock_is_enabled_before_any_dma_initialization);
    RUN_TEST(test_log_queue_and_ring_buffer_exist_before_dma_irq);
    RUN_TEST(test_uart_receive_is_armed_before_uart_irq);
    RUN_TEST(test_logger_irq_activation_is_deferred_until_task_exists);
    RUN_TEST(test_log_queue_uses_static_allocation);
    RUN_TEST(test_rx_dma_irq_is_also_deferred_until_task_exists);
    RUN_TEST(test_log_formatting_waits_for_logger_task);
    return UNITY_END();
}
