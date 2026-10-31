/*
 * transport.h — UART transport over the M1 header USART.
 *
 * v1 has a single transport: the M1's header UART (USART1, pin 12 TX / pin 13
 * RX), exposed to apps by the m1_uart_app_* firmware primitive this port adds.
 * The Flipper build also had a BLE transport; that needs a custom Meshtastic
 * build and a GATT server the M1 ESP32 API does not expose, so it is dropped.
 */
#pragma once

#include "mesh_app.h"

/* ---- firmware UART primitive (added to the M1 app API table) ---- */
extern void m1_uart_app_init(uint32_t baud);
extern void m1_uart_app_deinit(void);
extern int m1_uart_app_read(uint8_t *buf, int max_len);        /* -> bytes read */
extern void m1_uart_app_write(const uint8_t *buf, int len);
extern int m1_uart_app_available(void);

void transport_open(ZeroMeshApp *app);
void transport_close(ZeroMeshApp *app);
bool transport_is_up(ZeroMeshApp *app);
void transport_tx(ZeroMeshApp *app, const uint8_t *data, size_t len);
int transport_read(ZeroMeshApp *app, uint8_t *buf, int max_len);
const char *transport_name(ZeroMeshApp *app);
void transport_set_baud(ZeroMeshApp *app, uint32_t baud);
