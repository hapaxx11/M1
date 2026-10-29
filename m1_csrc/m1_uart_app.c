/*
 * m1_uart_app.c — app-facing header UART primitive + extended button poll.
 * See m1_uart_app.h.
 *
 * Design notes
 * ------------
 * RX: the firmware already receives USART1 (header) into h_uart_rx_streambuf
 *     via the DMA + IDLE-ISR path (usart_rxdata_process_from_isr). Normally
 *     vSer2UsbTask drains that stream to the USB CDC. While m1_uart_app_owned
 *     is true, vSer2UsbTask is gated off (see the hook in m1_usb_cdc_msc.c) so
 *     the bytes stay in the stream for the app to read here.
 *
 * TX: transmit straight to USART1 through huart_logdb (the USART1 handle) with
 *     a blocking, bounded HAL_UART_Transmit. While the app owns the UART,
 *     vUsb2SerTask's USART forwarding is gated off, so there is no DMA-TX in
 *     flight to contend with.
 *
 * This reuses existing, tested infrastructure and adds no new peripheral
 * bring-up. It assumes the CDC is in VCP mode (the default) — that is the mode
 * whose USART1 IDLE ISR feeds h_uart_rx_streambuf.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "stm32h5xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "stream_buffer.h"

#include "main.h"                 /* huart7 etc. (not used) */
#include "m1_tasks.h"             /* main_q_hdl, Q_EVENT_KEYPAD, S_M1_Main_Q_t */
#include "m1_system.h"            /* button_events_q_hdl, BUTTON_*_KP_ID, events */
#include "m1_usb_cdc_msc.h"       /* h_uart_rx_streambuf, linecoding */
#include "m1_log_debug.h"         /* huart_logdb (USART1 handle) */
#include "m1_games.h"             /* game_button_t / GAME_BTN_* */
#include "m1_uart_app.h"

volatile bool m1_uart_app_owned = false;

void m1_uart_app_init(uint32_t baud) {
    m1_uart_app_owned = true;
    linecoding.bitrate = baud;
    m1_usb_cdc_comconfig();

    /* Drop anything already buffered (RPC/log noise) so the app starts clean. */
    if(h_uart_rx_streambuf) {
        xStreamBufferReset(h_uart_rx_streambuf);
    }
}

void m1_uart_app_deinit(void) {
    m1_uart_app_owned = false;
    if(h_uart_rx_streambuf) {
        xStreamBufferReset(h_uart_rx_streambuf);
    }
}

int m1_uart_app_read(uint8_t *buf, int max_len) {
    if(!m1_uart_app_owned || !buf || max_len <= 0 || !h_uart_rx_streambuf) return 0;
    size_t got = xStreamBufferReceive(h_uart_rx_streambuf, buf, (size_t)max_len, 0);
    return (int)got;
}

int m1_uart_app_available(void) {
    if(!m1_uart_app_owned || !h_uart_rx_streambuf) return 0;
    return (int)xStreamBufferBytesAvailable(h_uart_rx_streambuf);
}

void m1_uart_app_write(const uint8_t *buf, int len) {
    if(!m1_uart_app_owned || !buf || len <= 0) return;

    /* Bounded timeout: ~10 bits/byte at the current baud, plus slack. Chunk so
     * a large frame cannot exceed the HAL's 16-bit size or a single timeout. */
    uint32_t baud = huart_logdb.Init.BaudRate;
    if(baud == 0) baud = 115200;

    int off = 0;
    while(off < len) {
        int chunk = len - off;
        if(chunk > 256) chunk = 256;
        uint32_t ms = ((uint32_t)chunk * 10u * 1000u) / baud + 50u;
        HAL_UART_Transmit(&huart_logdb, (uint8_t *)(buf + off), (uint16_t)chunk, ms);
        off += chunk;
    }
}

/* ---- extended button poll (adds click/double/long to game_poll_button) ---- */

uint8_t m1_poll_button_ex(uint32_t timeout_ms, uint8_t *out_evt) {
    S_M1_Main_Q_t q_item;
    S_M1_Buttons_Status btn_status;
    BaseType_t ret;
    game_button_t result = GAME_BTN_NONE;
    uint8_t evt = M1_BTN_EVT_NONE;

    if(out_evt) *out_evt = M1_BTN_EVT_NONE;

    ret = xQueueReceive(main_q_hdl, &q_item, pdMS_TO_TICKS(timeout_ms));
    if(ret != pdTRUE) return GAME_BTN_NONE;
    if(q_item.q_evt_type != Q_EVENT_KEYPAD) return GAME_BTN_NONE;

    ret = xQueueReceive(button_events_q_hdl, &btn_status, 0);
    if(ret != pdTRUE) return GAME_BTN_NONE;

    /* Classify one event value (click / double-click / long-click). */
    #define EVT_OF(v)                                       \
        ((v) == BUTTON_EVENT_LCLICK ? M1_BTN_EVT_LONG :     \
         (v) == BUTTON_EVENT_DBCLICK ? M1_BTN_EVT_DBL :     \
         (v) == BUTTON_EVENT_CLICK ? M1_BTN_EVT_CLICK : M1_BTN_EVT_NONE)

    uint8_t e;
    if((e = EVT_OF(btn_status.event[BUTTON_UP_KP_ID]))) {
        result = GAME_BTN_UP; evt = e;
    } else if((e = EVT_OF(btn_status.event[BUTTON_DOWN_KP_ID]))) {
        result = GAME_BTN_DOWN; evt = e;
    } else if((e = EVT_OF(btn_status.event[BUTTON_LEFT_KP_ID]))) {
        result = GAME_BTN_LEFT; evt = e;
    } else if((e = EVT_OF(btn_status.event[BUTTON_RIGHT_KP_ID]))) {
        result = GAME_BTN_RIGHT; evt = e;
    } else if((e = EVT_OF(btn_status.event[BUTTON_OK_KP_ID]))) {
        result = GAME_BTN_OK; evt = e;
    } else if((e = EVT_OF(btn_status.event[BUTTON_BACK_KP_ID]))) {
        result = GAME_BTN_BACK; evt = e;
        xQueueReset(main_q_hdl);
    }
    #undef EVT_OF

    if(out_evt) *out_evt = evt;
    return (uint8_t)result;
}
