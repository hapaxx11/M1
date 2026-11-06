/* See COPYING.txt for license details. */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "stm32h5xx_hal.h"
#include "main.h"
#include "m1_wifi_scene.h"
#include "m1_submenu.h"
#include "m1_wifi.h"
#include "m1_esp32_hal.h"
#include "m1_esp32_hal_lifecycle.h"
#include "m1_esp32_rpc.h"
#include "esp32_feature_map.h"
#include "esp_app_main.h"
#include "m1_display.h"
#include "m1_tasks.h"

#define WPS_MAX_ROWS 8
#define CSI_HISTORY 32

typedef struct {
    char label[40];
    uint8_t bssid[6];
} wps_row_t;

typedef struct {
    wps_row_t rows[WPS_MAX_ROWS];
    const char *labels[WPS_MAX_ROWS];
    uint8_t count;
    uint8_t channel;
    subghz_submenu_model_t model;
    uint32_t last_poll;
} wps_ctx_t;

typedef struct {
    uint8_t data[1 + 13 * 10];
    uint8_t mode;
    uint32_t last_poll;
} channel_ctx_t;

typedef struct {
    uint8_t data[24];
    uint16_t history[CSI_HISTORY];
    uint8_t history_count;
    uint8_t history_pos;
    uint32_t last_poll;
} csi_ctx_t;

typedef union {
    wps_ctx_t wps;
    channel_ctx_t channel;
    csi_ctx_t csi;
} radio_ctx_t;

static void radio_error(const char *title, const char *message)
{
    m1_message_box(&m1_u8g2, title, message, NULL, " OK ");
}

static bool radio_init_feature(esp32_feature_id_t fid)
{
    m1_esp32_rpc_foreground_acquire();
    m1_esp32_ensure_init();
    esp32_transport_t transport = m1_esp32_active_transport();
    if (m1_esp32_lifecycle_at_task_required(transport) &&
        !get_esp32_main_init_status())
        esp32_main_init();
    return m1_esp32_require_cap(esp32_feature_required_caps(fid),
                                esp32_feature_label(fid));
}

static void radio_release_feature(void)
{
    m1_esp32_deinit();
    m1_esp32_rpc_foreground_release();
}

static m1_esp32_rpc_status_t radio_call(uint16_t op, const uint8_t *req,
                                        uint16_t req_len, uint8_t *resp,
                                        uint16_t resp_cap, uint16_t *resp_len)
{
    return m1_esp32_rpc_call(op, req, req_len, resp, resp_cap, resp_len, 2u);
}

static void radio_stop_call(uint16_t op)
{
    uint8_t resp;
    uint16_t rlen = 0;
    (void)radio_call(op, NULL, 0, &resp, sizeof(resp), &rlen);
}

static void wps_draw_empty(M1SceneApp *app)
{
    wps_ctx_t *ctx = app->user_data;
    m1_u8g2_firstpage();
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
    u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
    u8g2_DrawStr(&m1_u8g2, 2, 9, "WPS Auditor");
    u8g2_DrawHLine(&m1_u8g2, 0, 11, 128);
    u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
    u8g2_DrawStr(&m1_u8g2, 2, 26, "Scanning WPS-PIN...");
    char line[24];
    snprintf(line, sizeof(line), "Channel %u", ctx->channel ? ctx->channel : 1);
    u8g2_DrawStr(&m1_u8g2, 2, 40, line);
    u8g2_DrawStr(&m1_u8g2, 2, 54, "Candidates, not proof");
    m1_u8g2_nextpage();
}

static void wps_on_enter(M1SceneApp *app)
{
    if (!radio_init_feature(ESP32_FEATURE_WPS)) {
        m1_scene_pop(app);
        return;
    }
    wps_ctx_t *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) {
        radio_error("WPS Auditor", "Out of memory");
        m1_scene_pop(app);
        return;
    }
    app->user_data = ctx;
    subghz_submenu_model_init(&ctx->model, 0, M1_MENU_VIS(1));
    uint8_t req[2] = { 0x40, 0x01 };
    uint8_t resp;
    uint16_t rlen = 0;
    if (radio_call(M1_ESP32_RPC_WPS_START, req, sizeof(req), &resp, sizeof(resp), &rlen) !=
        M1_ESP32_RPC_OK) {
        free(ctx);
        app->user_data = NULL;
        radio_error("WPS Auditor", "Start failed");
        m1_scene_pop(app);
        return;
    }
    app->need_redraw = true;
}

static void wps_poll(M1SceneApp *app)
{
    wps_ctx_t *ctx = app->user_data;
    uint32_t now = HAL_GetTick();
    if (now - ctx->last_poll < 300) return;
    ctx->last_poll = now;

    for (uint8_t n = 0; n < 4; n++) {
        uint8_t resp[44];
        uint16_t rlen = 0;
        if (radio_call(M1_ESP32_RPC_WPS_NEXT, NULL, 0,
                       resp, sizeof(resp), &rlen) != M1_ESP32_RPC_OK || rlen < 2)
            return;
        ctx->channel = resp[1];
        if (resp[0] != 0) break;
        if (rlen < 12 || resp[8] > 32 || rlen < 12 + resp[8]) return;

        bool found = false;
        for (uint8_t i = 0; i < ctx->count; i++) {
            if (!memcmp(ctx->rows[i].bssid, &resp[2], 6)) {
                found = true;
                break;
            }
        }
        if (found) continue;
        if (ctx->count >= WPS_MAX_ROWS) continue;

        uint8_t idx = ctx->count++;
        wps_row_t *row = &ctx->rows[idx];
        memcpy(row->bssid, &resp[2], 6);
        uint8_t ssid_len = resp[8];
        char ssid[33] = {0};
        memcpy(ssid, &resp[9], ssid_len);
        uint16_t pos = 9 + ssid_len;
        uint8_t flags = resp[pos + 2];
        snprintf(row->label, sizeof(row->label), "%.15s %s c%u",
                 ssid[0] ? ssid : "*hidden*", flags ? "WPS1" : "PIN", resp[pos + 1]);
        ctx->labels[idx] = row->label;
    }
    subghz_submenu_model_set_item_count(&ctx->model, ctx->count);
    app->need_redraw = true;
}

static bool wps_on_event(M1SceneApp *app, M1SceneEvent event)
{
    wps_ctx_t *ctx = app->user_data;
    if (event == M1SceneEventBack) {
        m1_scene_pop(app);
        return true;
    }
    if (event == M1SceneEventUp) subghz_submenu_model_up(&ctx->model);
    if (event == M1SceneEventDown) subghz_submenu_model_down(&ctx->model);
    if (event == M1SceneEventUp || event == M1SceneEventDown) app->need_redraw = true;
    wps_poll(app);
    return true;
}

static void wps_on_exit(M1SceneApp *app)
{
    radio_stop_call(M1_ESP32_RPC_WPS_STOP);
    free(app->user_data);
    app->user_data = NULL;
    radio_release_feature();
}

static void wps_draw(M1SceneApp *app)
{
    wps_ctx_t *ctx = app->user_data;
    if (!ctx->count) wps_draw_empty(app);
    else m1_submenu_draw(&ctx->model, "WPS Auditor", ctx->labels);
}

const M1SceneHandlers wifi_scene_wps_handlers = {
    .on_enter = wps_on_enter,
    .on_event = wps_on_event,
    .on_exit = wps_on_exit,
    .draw = wps_draw,
};

static void channel_start(M1SceneApp *app, uint8_t mode)
{
    esp32_feature_id_t fid = mode ? ESP32_FEATURE_DEAUTH_MON : ESP32_FEATURE_CHANNEL_MON;
    if (!radio_init_feature(fid)) {
        m1_scene_pop(app);
        return;
    }
    channel_ctx_t *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) {
        radio_error("Channel Monitor", "Out of memory");
        m1_scene_pop(app);
        return;
    }
    ctx->mode = mode;
    app->user_data = ctx;
    uint8_t req[3] = { mode, 0xFA, 0x00 };
    uint8_t resp;
    uint16_t rlen = 0;
    if (radio_call(M1_ESP32_RPC_CHANNEL_MON_START, req, sizeof(req),
                   &resp, sizeof(resp), &rlen) != M1_ESP32_RPC_OK) {
        free(ctx);
        app->user_data = NULL;
        radio_error("Channel Monitor", "Start failed");
        m1_scene_pop(app);
        return;
    }
    app->need_redraw = true;
}

static void airtime_on_enter(M1SceneApp *app) { channel_start(app, 0); }
static void deauth_mon_on_enter(M1SceneApp *app) { channel_start(app, 1); }

static bool channel_on_event(M1SceneApp *app, M1SceneEvent event)
{
    channel_ctx_t *ctx = app->user_data;
    if (event == M1SceneEventBack) {
        m1_scene_pop(app);
        return true;
    }
    uint32_t now = HAL_GetTick();
    if (now - ctx->last_poll >= 400) {
        ctx->last_poll = now;
        uint16_t rlen = 0;
        if (radio_call(M1_ESP32_RPC_CHANNEL_MON_STATS, NULL, 0,
                       ctx->data, sizeof(ctx->data), &rlen) == M1_ESP32_RPC_OK &&
            rlen == sizeof(ctx->data))
            app->need_redraw = true;
    }
    return true;
}

static void channel_on_exit(M1SceneApp *app)
{
    radio_stop_call(M1_ESP32_RPC_CHANNEL_MON_STOP);
    free(app->user_data);
    app->user_data = NULL;
    radio_release_feature();
}

static void channel_draw(M1SceneApp *app)
{
    channel_ctx_t *ctx = app->user_data;
    m1_u8g2_firstpage();
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
    u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
    u8g2_DrawStr(&m1_u8g2, 1, 8, ctx->mode ? "Deauth Monitor" : "Channel Airtime");
    u8g2_DrawHLine(&m1_u8g2, 0, 10, 128);
    char line[24];
    snprintf(line, sizeof(line), "ch%u", ctx->data[0] ? ctx->data[0] : 1);
    u8g2_DrawStr(&m1_u8g2, 108, 8, line);
    u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);

    for (uint8_t ch = 1; ch <= 13; ch++) {
        const uint8_t *st = &ctx->data[1 + (ch - 1) * 10];
        uint8_t load = st[9];
        uint8_t row = (ch - 1) / 2;
        uint8_t x = (ch & 1) ? 0 : 65;
        uint8_t y = 18 + row * 7;
        snprintf(line, sizeof(line), "%2u %3u", ch, load);
        u8g2_DrawStr(&m1_u8g2, x, y, line);
        uint8_t scale = ctx->mode ? 20 : 100;
        uint8_t capped = load > scale ? scale : load;
        u8g2_DrawFrame(&m1_u8g2, x + 29, y - 6, 32, 6);
        if (capped) u8g2_DrawBox(&m1_u8g2, x + 30, y - 5,
                                 (uint16_t)(30 * capped) / scale, 4);
    }
    m1_u8g2_nextpage();
}

const M1SceneHandlers wifi_scene_airtime_handlers = {
    .on_enter = airtime_on_enter,
    .on_event = channel_on_event,
    .on_exit = channel_on_exit,
    .draw = channel_draw,
};

const M1SceneHandlers wifi_scene_deauth_mon_handlers = {
    .on_enter = deauth_mon_on_enter,
    .on_event = channel_on_event,
    .on_exit = channel_on_exit,
    .draw = channel_draw,
};

static void csi_on_enter(M1SceneApp *app)
{
    if (!radio_init_feature(ESP32_FEATURE_CSI)) {
        m1_scene_pop(app);
        return;
    }
    csi_ctx_t *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) {
        radio_error("CSI Radar", "Out of memory");
        m1_scene_pop(app);
        return;
    }
    app->user_data = ctx;
    bool connected = wifi_is_connected();
    uint8_t req[2] = { connected ? 0 : 1, connected ? 0 : 1 };
    uint8_t resp;
    uint16_t rlen = 0;
    m1_esp32_rpc_status_t st = radio_call(M1_ESP32_RPC_CSI_START, req, sizeof(req),
                                          &resp, sizeof(resp), &rlen);
    if (st != M1_ESP32_RPC_OK) {
        free(ctx);
        app->user_data = NULL;
        char message[24];
        snprintf(message, sizeof(message), "Start failed (%u)", (unsigned)st);
        radio_error("CSI Radar", message);
        m1_scene_pop(app);
        return;
    }
    app->need_redraw = true;
}

static uint32_t csi_u32(const uint8_t *p)
{
    uint32_t v;
    memcpy(&v, p, sizeof(v));
    return v;
}

static float csi_float(const uint8_t *p)
{
    float v;
    memcpy(&v, p, sizeof(v));
    return v;
}

static bool csi_on_event(M1SceneApp *app, M1SceneEvent event)
{
    csi_ctx_t *ctx = app->user_data;
    if (event == M1SceneEventBack) {
        m1_scene_pop(app);
        return true;
    }
    uint32_t now = HAL_GetTick();
    if (now - ctx->last_poll >= 200) {
        ctx->last_poll = now;
        uint16_t rlen = 0;
        if (radio_call(M1_ESP32_RPC_CSI_STATUS, NULL, 0,
                       ctx->data, sizeof(ctx->data), &rlen) == M1_ESP32_RPC_OK &&
            rlen == sizeof(ctx->data)) {
            float variance = csi_float(&ctx->data[10]);
            uint32_t scaled = variance > 0 ? (uint32_t)(variance * 1000000.0f) : 0;
            if (scaled > 1000) scaled = 1000;
            ctx->history[ctx->history_pos] = (uint16_t)scaled;
            ctx->history_pos = (ctx->history_pos + 1) % CSI_HISTORY;
            if (ctx->history_count < CSI_HISTORY) ctx->history_count++;
            app->need_redraw = true;
        }
    }
    return true;
}

static void csi_on_exit(M1SceneApp *app)
{
    radio_stop_call(M1_ESP32_RPC_CSI_STOP);
    free(app->user_data);
    app->user_data = NULL;
    radio_release_feature();
}

static void csi_draw(M1SceneApp *app)
{
    csi_ctx_t *ctx = app->user_data;
    static const char *const states[] = {
        "No signal", "Calibrating", "Idle", "Motion", "Presence",
    };
    uint8_t state = ctx->data[0] < 5 ? ctx->data[0] : 0;
    m1_u8g2_firstpage();
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
    u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
    u8g2_DrawStr(&m1_u8g2, 1, 8, "CSI Radar (exp)");
    u8g2_DrawHLine(&m1_u8g2, 0, 10, 128);
    u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
    u8g2_DrawStr(&m1_u8g2, 2, 21, states[state]);
    char line[28];
    if (state == 1) {
        snprintf(line, sizeof(line), "Baseline %u%%", ctx->data[1]);
        u8g2_DrawStr(&m1_u8g2, 70, 21, line);
    }
    uint32_t frames = csi_u32(&ctx->data[2]);
    float turb = csi_float(&ctx->data[6]);
    float var = csi_float(&ctx->data[10]);
    snprintf(line, sizeof(line), "F:%lu T:%d V:%d", (unsigned long)frames,
             (int)(turb * 100), (int)(var * 1000000));
    u8g2_DrawStr(&m1_u8g2, 2, 32, line);

    for (uint8_t i = 0; i < ctx->history_count; i++) {
        uint8_t idx = (ctx->history_pos + CSI_HISTORY - ctx->history_count + i) % CSI_HISTORY;
        uint8_t h = (uint16_t)ctx->history[idx] * 24 / 1000;
        u8g2_DrawBox(&m1_u8g2, 2 + i * 4, 60 - h, 2, h);
    }
    u8g2_DrawHLine(&m1_u8g2, 0, 61, 128);
    m1_u8g2_nextpage();
}

const M1SceneHandlers wifi_scene_csi_handlers = {
    .on_enter = csi_on_enter,
    .on_event = csi_on_event,
    .on_exit = csi_on_exit,
    .draw = csi_draw,
};
