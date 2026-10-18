/* See COPYING.txt for license details. */

#include <string.h>
#include "stm32h5xx_hal.h"
#include "main.h"
#include "m1_esp32_hal.h"
#include "m1_esp32_m1link.h"
#include "m1_esp32_rpc.h"

#define M1LINK_SPI_TIMEOUT_MS 100u
#define M1LINK_HS_TIMEOUT_MS  100u
#define M1LINK_BUSY_RETRY_MS  150u

static uint32_t s_m1link_last_elapsed_ms;

static void m1link_delay_1ms(void)
{
	if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
		vTaskDelay(pdMS_TO_TICKS(1));
	else
		HAL_Delay(1);
}

static bool m1link_wait_handshake(uint32_t timeout_ms)
{
	uint32_t start = HAL_GetTick();
	while (HAL_GPIO_ReadPin(ESP32_HANDSHAKE_GPIO_Port, ESP32_HANDSHAKE_Pin)
	       != GPIO_PIN_SET)
	{
		if ((HAL_GetTick() - start) >= timeout_ms)
			return false;
		m1link_delay_1ms();
	}
	return true;
}

static int m1link_hal_xfer(const uint8_t *tx, uint8_t *rx, uint16_t mtu,
                           void *ctx)
{
	HAL_StatusTypeDef ret;
	uint32_t start;
	(void)ctx;

	(void)m1link_wait_handshake(M1LINK_HS_TIMEOUT_MS);

	start = HAL_GetTick();
	do
	{
		HAL_GPIO_WritePin(ESP32_SPI3_NSS_GPIO_Port, ESP32_SPI3_NSS_Pin,
		                  GPIO_PIN_RESET);
		for (volatile int i = 0; i < 50; i++) {}
		ret = HAL_SPI_TransmitReceive(&hspi_esp, (uint8_t *)tx, rx, mtu,
		                              M1LINK_SPI_TIMEOUT_MS);
		for (volatile int i = 0; i < 50; i++) {}
		HAL_GPIO_WritePin(ESP32_SPI3_NSS_GPIO_Port, ESP32_SPI3_NSS_Pin,
		                  GPIO_PIN_SET);

		if (ret != HAL_BUSY)
			break;
		m1link_delay_1ms();
	} while ((HAL_GetTick() - start) < M1LINK_BUSY_RETRY_MS);

	if (ret != HAL_OK)
	{
		HAL_SPI_Abort(&hspi_esp);
		return -1;
	}
	return 0;
}

uint32_t m1_esp32_m1link_last_elapsed_ms(void)
{
	return s_m1link_last_elapsed_ms;
}

static void m1link_pace_1ms(void *ctx)
{
	(void)ctx;
	m1link_delay_1ms();
}

uint8_t spi_m1link_send_recv_bin(const uint8_t *tx_buf, int tx_len,
                                 uint8_t *rx_buf, int rx_buf_size,
                                 int *out_len, int timeout_sec)
{
	uint8_t tx[M1_ESP32_M1LINK_MTU];
	uint8_t rx[M1_ESP32_M1LINK_MTU];
	uint32_t start = HAL_GetTick();
	uint32_t timeout_ms;
	uint8_t rc;

	if (out_len)
		*out_len = 0;
	if (!tx_buf || tx_len <= 0 || !rx_buf || rx_buf_size < 1)
		return 1u;
	if (!m1_esp32_get_init_status())
		return 2u;

	if (timeout_sec <= 0)
		timeout_sec = 2;
	timeout_ms = (uint32_t)timeout_sec * 1000u;

	/* Callers serialize probes and RPC requests with the shared RPC transaction
	 * mutex, which also excludes ESP32 HAL teardown. */
	HAL_SPI_Abort(&hspi_esp);
	rc = m1_esp32_m1link_send_recv_timed(m1link_hal_xfer, NULL,
	                                     tx, rx, M1_ESP32_M1LINK_MTU,
	                                     HAL_GetTick, timeout_ms, 20000u,
	                                     m1link_pace_1ms, NULL,
	                                     tx_buf, tx_len, rx_buf, rx_buf_size,
	                                     out_len);
	s_m1link_last_elapsed_ms = HAL_GetTick() - start;

	if (rc == 0u)
		return 0u;
	if (rc == 1u)
		return 1u;
	if (rc == 2u)
		return 2u;
	return 3u;
}
