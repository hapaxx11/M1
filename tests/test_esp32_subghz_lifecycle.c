/* See COPYING.txt for license details. */

/*
 * test_esp32_subghz_lifecycle.c
 *
 * Source-level regression checks for the ESP32/Sub-GHz lifecycle boundary.
 * The behavior is hardware-coupled (SPI3/EXTI vs. SI4463/TIM1), so the host
 * test verifies the source invariants that keep ESP32 transport quiesced
 * before Sub-GHz Read / Read Raw capture paths run.
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

void setUp(void)
{
}

void tearDown(void)
{
}

void test_subghz_entry_deinitializes_esp32_before_radio_init(void)
{
    char *content = read_file("m1_csrc/m1_subghz_scene.c");

    assert_contains(content, "#include \"m1_esp32_hal.h\"");
    assert_ordered(content, "m1_esp32_deinit();", "menu_sub_ghz_init();");

    free(content);
}

void test_wifi_and_bt_delegates_deinitialize_esp32_before_scene_pop(void)
{
    char *wifi = read_file("m1_csrc/m1_wifi_scene.c");
    char *bt = read_file("m1_csrc/m1_bt_scene.c");

    assert_contains(wifi, "#include \"m1_esp32_hal.h\"");
    assert_contains(bt, "#include \"m1_esp32_hal.h\"");
    assert_ordered(wifi, "fn(); m1_esp32_deinit();", "m1_scene_pop(app);");
    assert_ordered(bt, "fn(); m1_esp32_deinit();", "m1_scene_pop(app);");

    free(wifi);
    free(bt);
}

void test_esp32_uart_deinit_releases_transport_semaphore(void)
{
    char *content = read_file("m1_csrc/m1_esp32_hal.c");

    assert_ordered(content, "HAL_NVIC_DisableIRQ(ESP32_UART_DMA_Tx_IRQn);", "vSemaphoreDelete(sem_esp32_trans);");
    assert_ordered(content, "HAL_DMA_Abort(&hgpdma1_channel5_tx);", "vSemaphoreDelete(sem_esp32_trans);");
    assert_ordered(content, "HAL_NVIC_ClearPendingIRQ(ESP32_UART_DMA_Tx_IRQn);", "vSemaphoreDelete(sem_esp32_trans);");
    assert_ordered(content, "vSemaphoreDelete(sem_esp32_trans);", "sem_esp32_trans = NULL;");

    free(content);
}

void test_esp32_uart_tx_dma_isr_guards_deleted_semaphore(void)
{
    char *content = read_file("m1_csrc/m1_int_hdl.c");

    assert_ordered(content, "if ( sem_esp32_trans )", "xSemaphoreGiveFromISR(sem_esp32_trans");

    free(content);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_subghz_entry_deinitializes_esp32_before_radio_init);
    RUN_TEST(test_wifi_and_bt_delegates_deinitialize_esp32_before_scene_pop);
    RUN_TEST(test_esp32_uart_deinit_releases_transport_semaphore);
    RUN_TEST(test_esp32_uart_tx_dma_isr_guards_deleted_semaphore);
    return UNITY_END();
}
