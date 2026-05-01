/* See COPYING.txt for license details. */

/*
 * m1_esp32_cmd.c
 *
 * Binary SPI command protocol for STM32 <-> ESP32-C6 communication.
 *
 * M1 Project
 */

/*************************** I N C L U D E S **********************************/

#include <stdint.h>
#include <string.h>
#include "stm32h5xx_hal.h"
#include "main.h"
#include "m1_esp32_cmd.h"
#include "m1_esp32_hal.h"

/*************************** D E F I N E S ************************************/

#define SPI_TIMEOUT_MS  100

/*************************** V A R I A B L E S ********************************/

extern SPI_HandleTypeDef hspi_esp;

/*************** F U N C T I O N   I M P L E M E N T A T I O N ****************/


/******************************************************************************/
/**
  * @brief  Send 64 bytes to ESP32 over SPI3 (master TX)
  * @param  data  Pointer to 64 bytes to send
  * @return HAL status
  */
/******************************************************************************/
static uint8_t spi_dummy[64];

static void cs_delay(void)
{
	/* Short delay (~1us) for ESP32 SPI slave to recognize CS edge */
	for (volatile int i = 0; i < 50; i++) {}
}

static HAL_StatusTypeDef spi_tx_64(const uint8_t *data)
{
	HAL_StatusTypeDef ret;

	/* Assert CS */
	HAL_GPIO_WritePin(ESP32_SPI3_NSS_GPIO_Port, ESP32_SPI3_NSS_Pin, GPIO_PIN_RESET);
	cs_delay();

	ret = HAL_SPI_TransmitReceive(&hspi_esp, (uint8_t *)data, spi_dummy, 64, SPI_TIMEOUT_MS);

	cs_delay();
	/* Deassert CS */
	HAL_GPIO_WritePin(ESP32_SPI3_NSS_GPIO_Port, ESP32_SPI3_NSS_Pin, GPIO_PIN_SET);

	return ret;
}


/******************************************************************************/
/**
  * @brief  Receive 64 bytes from ESP32 over SPI3 (master RX)
  * @param  data  Pointer to 64-byte receive buffer
  * @return HAL status
  */
/******************************************************************************/
static HAL_StatusTypeDef spi_rx_64(uint8_t *data)
{
	HAL_StatusTypeDef ret;

	memset(spi_dummy, 0, 64);

	/* Assert CS */
	HAL_GPIO_WritePin(ESP32_SPI3_NSS_GPIO_Port, ESP32_SPI3_NSS_Pin, GPIO_PIN_RESET);
	cs_delay();

	ret = HAL_SPI_TransmitReceive(&hspi_esp, spi_dummy, data, 64, SPI_TIMEOUT_MS);

	cs_delay();
	/* Deassert CS */
	HAL_GPIO_WritePin(ESP32_SPI3_NSS_GPIO_Port, ESP32_SPI3_NSS_Pin, GPIO_PIN_SET);

	return ret;
}


/******************************************************************************/
/**
  * @brief  Wait for ESP32 HANDSHAKE pin to go HIGH (response ready)
  * @param  timeout_ms  Maximum wait time in milliseconds
  * @return 0 on success, -1 on timeout
  */
/******************************************************************************/
static int wait_handshake(uint32_t timeout_ms)
{
	uint32_t start = HAL_GetTick();

	while (HAL_GPIO_ReadPin(ESP32_HANDSHAKE_GPIO_Port, ESP32_HANDSHAKE_Pin) == GPIO_PIN_RESET)
	{
		if ((HAL_GetTick() - start) > timeout_ms)
			return -1;
		HAL_Delay(1);
	}

	return 0;
}


/******************************************************************************/
/**
  * @brief  Send a command to the ESP32 and receive the response.
  * @param  cmd  Pointer to the command to send (64 bytes)
  * @param  resp Pointer to buffer for the response (64 bytes)
  * @param  timeout_ms Maximum time to wait for response
  * @return 0 on success, -1 SPI error, -2 timeout, -3 bad magic
  */
/******************************************************************************/
int m1_esp32_send_cmd(const m1_cmd_t *cmd, m1_resp_t *resp, uint32_t timeout_ms)
{
	HAL_StatusTypeDef hal_ret;

	/* Brief pause so ESP32 SPI slave can re-queue its next receive
	 * transaction between back-to-back commands (e.g. SCAN_NEXT). */
	HAL_Delay(2);

	/* Send the command */
	hal_ret = spi_tx_64((const uint8_t *)cmd);
	if (hal_ret != HAL_OK)
		return -10 - (int)hal_ret;  /* -11=ERROR, -12=BUSY, -13=TIMEOUT */

	/* Wait for ESP32 to signal response ready */
	if (wait_handshake(timeout_ms) != 0)
		return -2;

	/* Read the response */
	hal_ret = spi_rx_64((uint8_t *)resp);
	if (hal_ret != HAL_OK)
		return -20 - (int)hal_ret;  /* -21=ERROR, -22=BUSY, -23=TIMEOUT */

	/* Validate response magic */
	if (resp->magic != M1_RESP_MAGIC)
		return -3;

	return 0;
}


/******************************************************************************/
/**
  * @brief  Send a simple command with no payload and receive response.
  * @param  cmd_id  The command ID
  * @param  resp    Pointer to buffer for the response
  * @param  timeout_ms Maximum time to wait for response
  * @return 0 on success, negative on error
  */
/******************************************************************************/
int m1_esp32_simple_cmd(uint8_t cmd_id, m1_resp_t *resp, uint32_t timeout_ms)
{
	m1_cmd_t cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.magic = M1_CMD_MAGIC;
	cmd.cmd_id = cmd_id;
	cmd.payload_len = 0;

	return m1_esp32_send_cmd(&cmd, resp, timeout_ms);
}
