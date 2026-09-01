/* See COPYING.txt for license details. */

/**
 * @file   m1_espnow_scene_send.c
 * @brief  ESP-NOW saved-capture sender scenes.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "stm32h5xx_hal.h"
#include "main.h"
#include "m1_espnow_scene.h"
#include "m1_espnow_scene_ctx.h"
#include "m1_scene.h"
#include "m1_submenu.h"
#include "m1_espnow_hal.h"
#include "espnow_file_transfer.h"
#include "espnow_shareable.h"
#include "m1_display.h"
#include "m1_lcd.h"
#include "m1_tasks.h"
#include "m1_file_browser.h"
#include "m1_storage.h"
#include "ff.h"

#define SEND_CATEGORY_COUNT  4u
#define SEND_CHUNK_BYTES \
    (M1_ESPNOW_SEND_PAYLOAD_MAX - 6u)  /* FILE_DATA type+seq+offset */
#define SEND_OFFER_TIMEOUT_MS  10000u

static const char *const s_send_labels[SEND_CATEGORY_COUNT] = {
    "Sub-GHz",
    "NFC",
    "RFID",
    "Infrared",
};

static const espnow_share_kind_t s_send_kinds[SEND_CATEGORY_COUNT] = {
    ESPNOW_SHARE_KIND_SUBGHZ,
    ESPNOW_SHARE_KIND_NFC,
    ESPNOW_SHARE_KIND_RFID,
    ESPNOW_SHARE_KIND_IR,
};

static subghz_submenu_model_t s_send_model;
static espnow_ft_ctx_t s_send_ctx;
static char s_send_status[32];
static char s_send_path[160];
static char s_send_filename[ESPNOW_FT_FILENAME_MAX + 1u];

static const char *send_dir_for_kind(espnow_share_kind_t kind)
{
    switch (kind) {
    case ESPNOW_SHARE_KIND_SUBGHZ: return "0:/SUBGHZ";
    case ESPNOW_SHARE_KIND_NFC:    return "0:/NFC";
    case ESPNOW_SHARE_KIND_RFID:   return "0:/RFID";
    case ESPNOW_SHARE_KIND_IR:     return "0:/IR";
    default:                       return NULL;
    }
}

static bool send_hal_send(const uint8_t mac[ESPNOW_FT_MAC_LEN],
                          const uint8_t *data, size_t len, void *ctx)
{
    (void)ctx;
    return m1_espnow_send(mac, data, len);
}

static uint32_t send_hal_millis(void *ctx)
{
    (void)ctx;
    return HAL_GetTick();
}

static const espnow_ft_hal_ops_t s_send_hal = {
    .send       = send_hal_send,
    .file_open  = NULL,
    .file_write = NULL,
    .file_close = NULL,
    .millis     = send_hal_millis,
    .ctx        = NULL,
};

static bool send_compute_crc(const char *path, uint32_t *size_out,
                             uint32_t *crc_out)
{
    FIL file;
    uint8_t buf[SEND_CHUNK_BYTES];
    UINT br = 0;
    uint32_t crc = 0;
    uint32_t total = 0;

    if (f_open(&file, path, FA_READ | FA_OPEN_EXISTING) != FR_OK)
        return false;
    for (;;) {
        if (f_read(&file, buf, sizeof(buf), &br) != FR_OK) {
            f_close(&file);
            return false;
        }
        if (br == 0u)
            break;
        crc = espnow_ft_crc32(crc, buf, br);
        total += br;
    }
    f_close(&file);
    if (size_out)
        *size_out = total;
    if (crc_out)
        *crc_out = crc;
    return total > 0u;
}

static void send_progress_draw_card(void)
{
    m1_u8g2_firstpage();
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
    u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
    m1_draw_text(&m1_u8g2, 2, 9, 120, "Send Capture", TEXT_ALIGN_CENTER);
    u8g2_DrawHLine(&m1_u8g2, 0, 10, M1_LCD_DISPLAY_WIDTH);

    u8g2_SetFont(&m1_u8g2, u8g2_font_NokiaSmallPlain_tf);
    m1_draw_text(&m1_u8g2, 2, 24, 120, s_send_filename, TEXT_ALIGN_CENTER);
    m1_draw_text(&m1_u8g2, 2, 38, 120, s_send_status, TEXT_ALIGN_CENTER);

    if (s_send_ctx.file_size > 0u &&
        (s_send_ctx.state == ESPNOW_FT_STATE_SENDING ||
         s_send_ctx.state == ESPNOW_FT_STATE_WAIT_ACK ||
         s_send_ctx.state == ESPNOW_FT_STATE_DONE)) {
        uint8_t pct = (uint8_t)((s_send_ctx.bytes_transferred * 100u) /
                                s_send_ctx.file_size);
        if (pct > 100u)
            pct = 100u;
        u8g2_DrawFrame(&m1_u8g2, 10, 46, 108, 10);
        if (pct > 0u)
            u8g2_DrawBox(&m1_u8g2, 11, 47, (uint8_t)(106u * pct / 100u), 8);
    }

    m1_u8g2_nextpage();
}

static bool send_handle_button_cancel(void)
{
    S_M1_Main_Q_t q_item;
    S_M1_Buttons_Status buttons;

    if (xQueueReceive(main_q_hdl, &q_item, 20 / portTICK_PERIOD_MS) != pdTRUE)
        return false;
    if (q_item.q_evt_type != Q_EVENT_KEYPAD)
        return false;
    if (xQueueReceive(button_events_q_hdl, &buttons, 0) != pdTRUE)
        return false;
    return buttons.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK;
}

static void send_run_transfer(const uint8_t peer_mac[ESPNOW_MAC_LEN])
{
    FIL file;
    uint8_t buf[SEND_CHUNK_BYTES];
    uint32_t offer_start = HAL_GetTick();

    if (f_open(&file, s_send_path, FA_READ | FA_OPEN_EXISTING) != FR_OK) {
        snprintf(s_send_status, sizeof(s_send_status), "Open failed");
        send_progress_draw_card();
        HAL_Delay(1200);
        return;
    }

    bool offer_sent = espnow_ft_send_offer(&s_send_ctx);
    snprintf(s_send_status, sizeof(s_send_status),
             offer_sent ? "Waiting accept..." : "Offer failed");
    send_progress_draw_card();
    if (!offer_sent) {
        f_close(&file);
        HAL_Delay(1200);
        return;
    }

    while (s_send_ctx.state != ESPNOW_FT_STATE_DONE &&
           s_send_ctx.state != ESPNOW_FT_STATE_FAILED) {
        uint8_t from_mac[ESPNOW_MAC_LEN];
        uint8_t msg[64];
        uint8_t msg_len = 0;

        if (send_handle_button_cancel()) {
            uint8_t abort_msg[2] = { ESPNOW_FT_MSG_ABORT, s_send_ctx.current_seq };
            m1_espnow_send(peer_mac, abort_msg, sizeof(abort_msg));
            snprintf(s_send_status, sizeof(s_send_status), "Cancelled");
            break;
        }

        if (m1_espnow_recv_msg(from_mac, msg, sizeof(msg), &msg_len) &&
            memcmp(from_mac, peer_mac, ESPNOW_MAC_LEN) == 0 &&
            msg_len >= 2u) {
            espnow_ft_send_on_recv(&s_send_ctx, msg[0], msg + 2, msg_len - 2u);
        }

        if (s_send_ctx.state == ESPNOW_FT_STATE_OFFER_SENT &&
            (HAL_GetTick() - offer_start) >= SEND_OFFER_TIMEOUT_MS) {
            s_send_ctx.state = ESPNOW_FT_STATE_FAILED;
        }

        if (s_send_ctx.state == ESPNOW_FT_STATE_WAIT_ACK) {
            if (espnow_ft_send_check_timeout(&s_send_ctx) &&
                s_send_ctx.state == ESPNOW_FT_STATE_SENDING) {
                f_lseek(&file, s_send_ctx.bytes_transferred);
            }
        }

        if (s_send_ctx.state == ESPNOW_FT_STATE_SENDING) {
            UINT br = 0;
            if (f_lseek(&file, s_send_ctx.bytes_transferred) != FR_OK ||
                f_read(&file, buf, s_send_ctx.chunk_size, &br) != FR_OK ||
                br == 0u ||
                !espnow_ft_send_chunk(&s_send_ctx, buf, br)) {
                s_send_ctx.state = ESPNOW_FT_STATE_FAILED;
            }
        }

        switch (s_send_ctx.state) {
        case ESPNOW_FT_STATE_OFFER_SENT:
            snprintf(s_send_status, sizeof(s_send_status), "Waiting accept...");
            break;
        case ESPNOW_FT_STATE_SENDING:
        case ESPNOW_FT_STATE_WAIT_ACK:
            snprintf(s_send_status, sizeof(s_send_status), "Sending...");
            break;
        case ESPNOW_FT_STATE_DONE:
            snprintf(s_send_status, sizeof(s_send_status), "Sent OK");
            break;
        case ESPNOW_FT_STATE_FAILED:
            snprintf(s_send_status, sizeof(s_send_status), "Send failed");
            break;
        default:
            break;
        }
        send_progress_draw_card();
    }

    f_close(&file);
    send_progress_draw_card();
    HAL_Delay(1500);
}

static void send_capture_on_enter(M1SceneApp *app)
{
    (void)app;
    subghz_submenu_model_init(&s_send_model, SEND_CATEGORY_COUNT,
                              M1_MENU_VIS(SEND_CATEGORY_COUNT));
    m1_espnow_scene_ctx_set_share_kind(s_send_kinds[0]);
    app->need_redraw = true;
}

static bool send_capture_on_event(M1SceneApp *app, M1SceneEvent event)
{
    switch (event) {
    case M1SceneEventOk:
        m1_espnow_scene_ctx_set_share_kind(s_send_kinds[s_send_model.selected]);
        m1_scene_push(app, EspnowSceneSendProgress);
        return true;
    case M1SceneEventUp:
    case M1SceneEventDown:
    case M1SceneEventBack:
        return m1_submenu_event(app, event, &s_send_model, NULL);
    default:
        return false;
    }
}

static void send_capture_draw(M1SceneApp *app)
{
    (void)app;
    m1_submenu_draw(&s_send_model, "Send Capture", s_send_labels);
}

static void send_progress_on_enter(M1SceneApp *app)
{
    uint8_t peer_mac[ESPNOW_MAC_LEN];
    const char *dir = send_dir_for_kind(m1_espnow_scene_ctx_share_kind());
    uint32_t file_size = 0;
    uint32_t crc = 0;

    memset(&s_send_ctx, 0, sizeof(s_send_ctx));
    s_send_filename[0] = '\0';
    snprintf(s_send_status, sizeof(s_send_status), "Selecting...");

    if (!m1_espnow_scene_ctx_get_peer(peer_mac, NULL, 0)) {
        snprintf(s_send_status, sizeof(s_send_status), "Scan Peers first");
        send_progress_draw_card();
        HAL_Delay(1200);
        m1_scene_pop(app);
        return;
    }
    if (dir == NULL) {
        snprintf(s_send_status, sizeof(s_send_status), "Bad category");
        send_progress_draw_card();
        HAL_Delay(1200);
        m1_scene_pop(app);
        return;
    }

    S_M1_file_info *fi = storage_browse(dir);
    if (fi == NULL || !fi->file_is_selected || fi->status != FB_OK) {
        snprintf(s_send_status, sizeof(s_send_status), "No file selected");
        send_progress_draw_card();
        HAL_Delay(800);
        m1_scene_pop(app);
        return;
    }

    snprintf(s_send_path, sizeof(s_send_path), "%s/%s",
             fi->dir_name, fi->file_name);
    espnow_share_basename(s_send_path, s_send_filename, sizeof(s_send_filename));
    if (!espnow_share_is_shareable(s_send_filename) ||
        !espnow_share_name_is_safe(s_send_filename, ESPNOW_FT_FILENAME_MAX) ||
        !send_compute_crc(s_send_path, &file_size, &crc)) {
        snprintf(s_send_status, sizeof(s_send_status), "Invalid file");
        send_progress_draw_card();
        HAL_Delay(1200);
        m1_scene_pop(app);
        return;
    }

    espnow_ft_send_init(&s_send_ctx, &s_send_hal, peer_mac, s_send_filename,
                        file_size, crc, SEND_CHUNK_BYTES);
    snprintf(s_send_status, sizeof(s_send_status), "Preparing...");
    send_progress_draw_card();
    send_run_transfer(peer_mac);
    m1_scene_pop(app);
}

static bool send_progress_on_event(M1SceneApp *app, M1SceneEvent event)
{
    if (event == M1SceneEventBack) {
        m1_scene_pop(app);
        return true;
    }
    return false;
}

static void send_progress_draw(M1SceneApp *app)
{
    (void)app;
    send_progress_draw_card();
}

static void send_on_exit(M1SceneApp *app) { (void)app; }

const M1SceneHandlers espnow_scene_send_capture_handlers = {
    .on_enter = send_capture_on_enter,
    .on_event = send_capture_on_event,
    .on_exit  = send_on_exit,
    .draw     = send_capture_draw,
};

const M1SceneHandlers espnow_scene_send_progress_handlers = {
    .on_enter = send_progress_on_enter,
    .on_event = send_progress_on_event,
    .on_exit  = send_on_exit,
    .draw     = send_progress_draw,
};
