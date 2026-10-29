/*
 * m1_uart_app.h — app-facing header UART primitive + extended button poll.
 *
 * Exported through m1_app_api.c for external .m1app applications.
 *
 * The header UART is USART1 on the expansion header — pin 12 = PA9 (TX),
 * pin 13 = PA10 (RX) — already brought up by the firmware for the USB<->UART
 * bridge. These calls let an external .m1app own that UART: bytes the radio
 * sends land in the existing RX stream buffer (h_uart_rx_streambuf) and the app
 * reads them, while the USB bridge is held off for the duration.
 */
#ifndef M1_UART_APP_H_
#define M1_UART_APP_H_

#include <stdint.h>
#include <stdbool.h>

/* True while an app owns the header UART; read by the USB-bridge tasks in
 * m1_usb_cdc_msc.c so they stop draining/feeding USART1. */
extern volatile bool m1_uart_app_owned;

/* Claim the header UART at `baud`, flush any stale RX, and hold off the USB
 * bridge. Requires the CDC to be in VCP mode (the default), which is what feeds
 * h_uart_rx_streambuf from the USART1 IDLE ISR. */
void m1_uart_app_init(uint32_t baud);

/* Release the UART back to the USB bridge. */
void m1_uart_app_deinit(void);

/* Read up to max_len bytes already received; returns the count (0 if none).
 * Non-blocking. */
int m1_uart_app_read(uint8_t *buf, int max_len);

/* Transmit len bytes to the radio over USART1 (blocking, bounded timeout). */
void m1_uart_app_write(const uint8_t *buf, int len);

/* Bytes currently waiting in the RX stream buffer. */
int m1_uart_app_available(void);

/* Extended button poll: like game_poll_button, but also reports whether the
 * press was a click, double-click or long-click via *out_evt. Returns a
 * game_button_t (== m1app_button_t). out_evt values below. */
#define M1_BTN_EVT_NONE   0
#define M1_BTN_EVT_CLICK  1
#define M1_BTN_EVT_DBL    2
#define M1_BTN_EVT_LONG   3
uint8_t m1_poll_button_ex(uint32_t timeout_ms, uint8_t *out_evt);

#endif /* M1_UART_APP_H_ */
