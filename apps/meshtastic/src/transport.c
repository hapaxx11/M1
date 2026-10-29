#include "transport.h"

void transport_open(ZeroMeshApp *app) {
    if(!app) return;
    m1_uart_app_init(app->baud);
    app->uart_up = true;
}

void transport_close(ZeroMeshApp *app) {
    if(!app) return;
    m1_uart_app_deinit();
    app->uart_up = false;
}

bool transport_is_up(ZeroMeshApp *app) {
    return app && app->uart_up;
}

void transport_tx(ZeroMeshApp *app, const uint8_t *data, size_t len) {
    if(!app || !data || !len || !app->uart_up) return;
    m1_uart_app_write(data, (int)len);
}

int transport_read(ZeroMeshApp *app, uint8_t *buf, int max_len) {
    if(!app || !app->uart_up || !buf || max_len <= 0) return 0;
    return m1_uart_app_read(buf, max_len);
}

const char *transport_name(ZeroMeshApp *app) {
    (void)app;
    return "USART1";
}

void transport_set_baud(ZeroMeshApp *app, uint32_t baud) {
    if(!app) return;
    app->baud = baud;
    if(app->uart_up) {
        m1_uart_app_deinit();
        m1_uart_app_init(baud);
    }
}
