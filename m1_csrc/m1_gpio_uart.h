/* See COPYING.txt for license details. */

/**
 * @file   m1_gpio_uart.h
 * @brief  USB-UART Bridge — Momentum-style.
 *
 * Provides a USB-to-UART adapter using USART1 (PA9 TX / PA10 RX) on J7
 * and the USB CDC virtual COM port.  The on-screen terminal shows received
 * data while the bidirectional bridge runs in the background via the
 * existing VCP task infrastructure.
 *
 * Tightly coupled to the M1 USART1 / DMA / USB CDC VCP plumbing:
 * reads the logdb DMA circular RX buffer directly and uses
 * __HAL_DMA_GET_COUNTER() on huart_logdb.hdmarx.  Not portable
 * without adapting these hardware-specific paths.
 */

#ifndef M1_GPIO_UART_H_
#define M1_GPIO_UART_H_

/**
 * @brief  Run the USB-UART bridge (blocking delegate).
 *
 * Switches USART1 into VCP mode at the user-selected baud rate,
 * displays a scrolling terminal with received data, and bridges
 * USB CDC ↔ USART1 bidirectionally.
 *
 * Controls:
 *   LEFT / RIGHT  — cycle baud rate (9600 … 921600)
 *   OK            — toggle ASCII / HEX display mode
 *   UP            — clear terminal
 *   BACK          — exit bridge and restore original UART state
 */
void gpio_uart_bridge_run(void);

#endif /* M1_GPIO_UART_H_ */
