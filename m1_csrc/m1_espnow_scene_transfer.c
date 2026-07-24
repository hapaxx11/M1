/* See COPYING.txt for license details. */

/**
 * @file   m1_espnow_scene_transfer.c
 * @brief  ESP-NOW file transfer scene — send/receive with progress display.
 *
 * Uses the espnow_file_transfer pure-logic module for protocol state and
 * m1_espnow_hal for ESP-NOW transport and FatFS file operations.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "stm32h5xx_hal.h"
#include "main.h"
#include "m1_espnow_scene.h"
#include "m1_scene.h"
#include "m1_espnow_hal.h"
#include "espnow_file_transfer.h"
#include "m1_display.h"
#include "m1_lcd.h"
#include "m1_tasks.h"

/*==========================================================================*/
/* Transfer scene state                                                     */
/*==========================================================================*/

static espnow_ft_ctx_t s_ft_ctx;
static uint32_t s_last_tick;

#define TRANSFER_POLL_INTERVAL_MS  50

/*==========================================================================*/
/* HAL ops adapter (wired to m1_espnow_hal)                                 */
/*==========================================================================*/

static bool ft_hal_send(const uint8_t mac[ESPNOW_FT_MAC_LEN],
                        const uint8_t *data, size_t len, void *ctx)
{
    (void)ctx;
    return m1_espnow_send(mac, data, len);
}

static espnow_ft_file_t ft_hal_file_open(const char *path, void *ctx)
{
    (void)ctx;
    return m1_espnow_file_open(path);
}

static bool ft_hal_file_write(espnow_ft_file_t f, const uint8_t *data,
                              size_t len, void *ctx)
{
    (void)ctx;
    return m1_espnow_file_write(f, data, len);
}

static void ft_hal_file_close(espnow_ft_file_t f, void *ctx)
{
    (void)ctx;
    m1_espnow_file_close(f);
}

static uint32_t ft_hal_millis(void *ctx)
{
    (void)ctx;
    return HAL_GetTick();
}

static const espnow_ft_hal_ops_t s_ft_hal = {
    .send       = ft_hal_send,
    .file_open  = ft_hal_file_open,
    .file_write = ft_hal_file_write,
    .file_close = ft_hal_file_close,
    .millis     = ft_hal_millis,
    .ctx        = NULL,
};

/*==========================================================================*/
/* Transfer scene — on_enter                                                */
/*==========================================================================*/

static void transfer_on_enter(M1SceneApp *app)
{
    /* Initialise as receiver — sender mode would be initiated from
     * a file browser (future Phase 5 integration). */
    espnow_ft_recv_init(&s_ft_ctx, &s_ft_hal);
    s_last_tick = HAL_GetTick();
    app->need_redraw = true;
}

/*==========================================================================*/
/* Transfer scene — on_event                                                */
/*==========================================================================*/

static bool transfer_on_event(M1SceneApp *app, M1SceneEvent event)
{
    uint32_t now = HAL_GetTick();

    /* Periodic processing */
    if ((now - s_last_tick) >= TRANSFER_POLL_INTERVAL_MS) {
        s_last_tick = now;

        /* Poll for incoming data from ESP-NOW */
        uint8_t from_mac[ESPNOW_FT_MAC_LEN];
        uint8_t msg_buf[ESPNOW_FT_CHUNK_MAX + 8];
        uint8_t msg_len = 0;
        if (m1_espnow_recv_msg(from_mac, msg_buf, sizeof(msg_buf), &msg_len)) {
            if (msg_len >= 2) {
                uint8_t type = msg_buf[0];
                uint8_t seq  = msg_buf[1];
                espnow_ft_recv_on_msg(&s_ft_ctx, from_mac, type, seq,
                                      msg_buf + 2, msg_len - 2);
                app->need_redraw = true;
            }
        }

        /* Handle offer acceptance (auto-accept for now) */
        if (s_ft_ctx.state == ESPNOW_FT_STATE_OFFER_RECEIVED) {
            char save_path[64];
            snprintf(save_path, sizeof(save_path), "/ESPNOW/%s",
                     s_ft_ctx.filename);
            espnow_ft_recv_accept(&s_ft_ctx, save_path);
            app->need_redraw = true;
        }

        /* Check for sender-side timeout */
        if (s_ft_ctx.state == ESPNOW_FT_STATE_WAIT_ACK) {
            espnow_ft_send_check_timeout(&s_ft_ctx);
            app->need_redraw = true;
        }
    }

    switch (event) {
    case M1SceneEventBack:
        if (s_ft_ctx.file_handle) {
            m1_espnow_file_close(s_ft_ctx.file_handle);
            s_ft_ctx.file_handle = NULL;
        }
        m1_scene_pop(app);
        return true;

    default:
        return false;
    }
}

/*==========================================================================*/
/* Transfer scene — draw                                                    */
/*==========================================================================*/

static void transfer_draw(M1SceneApp *app)
{
    (void)app;
    m1_u8g2_firstpage();
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
    u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
    m1_draw_text(&m1_u8g2, 2, 9, 120, "File Transfer", TEXT_ALIGN_CENTER);
    u8g2_DrawHLine(&m1_u8g2, 0, 10, M1_LCD_DISPLAY_WIDTH);

    u8g2_SetFont(&m1_u8g2, u8g2_font_NokiaSmallPlain_tf);

    switch (s_ft_ctx.state) {
    case ESPNOW_FT_STATE_IDLE:
        m1_draw_text(&m1_u8g2, 2, 36, 120, "Waiting for file...", TEXT_ALIGN_CENTER);
        break;

    case ESPNOW_FT_STATE_RECEIVING:
    case ESPNOW_FT_STATE_WAIT_ACK:
    case ESPNOW_FT_STATE_SENDING: {
        char line[32];
        uint8_t pct = 0;
        if (s_ft_ctx.file_size > 0)
            pct = (uint8_t)((s_ft_ctx.bytes_transferred * 100) / s_ft_ctx.file_size);
        snprintf(line, sizeof(line), "%s", s_ft_ctx.filename);
        m1_draw_text(&m1_u8g2, 2, 24, 120, line, TEXT_ALIGN_CENTER);
        snprintf(line, sizeof(line), "%u%% (%u/%u)",
                 pct, (unsigned)s_ft_ctx.bytes_transferred,
                 (unsigned)s_ft_ctx.file_size);
        m1_draw_text(&m1_u8g2, 2, 38, 120, line, TEXT_ALIGN_CENTER);
        /* Simple progress bar */
        u8g2_DrawFrame(&m1_u8g2, 10, 46, 108, 10);
        if (pct > 0)
            u8g2_DrawBox(&m1_u8g2, 11, 47, (uint8_t)(106 * pct / 100), 8);
        break;
    }

    case ESPNOW_FT_STATE_DONE:
        m1_draw_text(&m1_u8g2, 2, 30, 120, "Transfer Complete!", TEXT_ALIGN_CENTER);
        m1_draw_text(&m1_u8g2, 2, 44, 120, "CRC32 OK", TEXT_ALIGN_CENTER);
        break;

    case ESPNOW_FT_STATE_FAILED:
        m1_draw_text(&m1_u8g2, 2, 36, 120, "Transfer Failed", TEXT_ALIGN_CENTER);
        break;

    default:
        m1_draw_text(&m1_u8g2, 2, 36, 120, s_ft_ctx.filename, TEXT_ALIGN_CENTER);
        break;
    }
    m1_u8g2_nextpage();
}

static void transfer_on_exit(M1SceneApp *app) { (void)app; }

const M1SceneHandlers espnow_scene_transfer_handlers = {
    .on_enter = transfer_on_enter,
    .on_event = transfer_on_event,
    .on_exit  = transfer_on_exit,
    .draw     = transfer_draw,
};
