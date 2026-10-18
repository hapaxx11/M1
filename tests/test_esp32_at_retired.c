/* See COPYING.txt for license details. */

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
	buf = (char *)malloc((size_t)size + 1u);
	TEST_ASSERT_NOT_NULL(buf);
	TEST_ASSERT_EQUAL_size_t((size_t)size, fread(buf, 1u, (size_t)size, fp));
	buf[size] = '\0';
	fclose(fp);
	return buf;
}

void setUp(void) {}
void tearDown(void) {}

void test_firmware_build_excludes_at_transport_sources(void)
{
	char *cmake = read_file("cmake/m1_01/CMakeLists.txt");

	TEST_ASSERT_NULL(strstr(cmake, "Esp_spi_at/"));
	TEST_ASSERT_NOT_NULL(strstr(cmake, "../../m1_csrc/m1_esp32_m1link.c"));
	free(cmake);
}

void test_esp32_hal_has_no_at_task_or_transport_interrupts(void)
{
	char *hal = read_file("m1_csrc/m1_esp32_hal.c");
	char *interrupts = read_file("m1_csrc/m1_int_hdl.c");

	TEST_ASSERT_NULL(strstr(hal, "esp_app_main.h"));
	TEST_ASSERT_NULL(strstr(hal, "esp32_main_init"));
	TEST_ASSERT_NULL(strstr(hal, "esp32_main_deinit"));
	TEST_ASSERT_NULL(strstr(interrupts, "ESP32_GPIO_EXTI_Callback"));
	TEST_ASSERT_NULL(strstr(interrupts, "esp_spi_msg_queue"));
	free(interrupts);
	free(hal);
}

void test_wifi_has_no_at_command_dispatch(void)
{
	char *wifi = read_file("m1_csrc/m1_wifi.c");

	TEST_ASSERT_NULL(strstr(wifi, "spi_AT_send_recv"));
	TEST_ASSERT_NULL(strstr(wifi, "AT+M1"));
	TEST_ASSERT_NOT_NULL(strstr(wifi, "ESP32_TRANSPORT_RPC"));
	TEST_ASSERT_NOT_NULL(strstr(wifi, "ESP32_TRANSPORT_BINARY_SPI"));
	free(wifi);
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_firmware_build_excludes_at_transport_sources);
	RUN_TEST(test_esp32_hal_has_no_at_task_or_transport_interrupts);
	RUN_TEST(test_wifi_has_no_at_command_dispatch);
	return UNITY_END();
}
