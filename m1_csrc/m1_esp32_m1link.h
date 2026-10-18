/* See COPYING.txt for license details. */

#ifndef M1_ESP32_M1LINK_H_
#define M1_ESP32_M1LINK_H_

#include <stdint.h>

uint8_t spi_m1link_send_recv_bin(const uint8_t *tx_buf, int tx_len,
                                 uint8_t *rx_buf, int rx_buf_size,
                                 int *out_len, int timeout_sec);
uint32_t m1_esp32_m1link_last_elapsed_ms(void);

#endif /* M1_ESP32_M1LINK_H_ */
