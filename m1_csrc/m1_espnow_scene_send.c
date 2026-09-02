/* See COPYING.txt for license details. */

/**
 * @file   m1_espnow_scene_send.c
 * @brief  ESP-NOW saved-capture sender scenes.
 */

#include <stdint.h>
#include <stdbool.h>

#include "m1_espnow_scene.h"
#include "m1_espnow_scene_ctx.h"
#include "m1_espnow_capture_share.h"
#include "m1_scene.h"
#include "m1_submenu.h"
#include "espnow_shareable.h"
#include "m1_display.h"
#include "m1_lcd.h"

#define SEND_CATEGORY_COUNT  4u

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
    if (!m1_espnow_capture_share_choose_and_begin(
            m1_espnow_scene_ctx_share_kind()))
        m1_scene_pop(app);
    else
        app->need_redraw = true;
}

static bool send_progress_on_event(M1SceneApp *app, M1SceneEvent event)
{
    if (event == M1SceneEventBack) {
        m1_espnow_capture_share_cancel();
        m1_scene_pop(app);
        return true;
    }
    if (event == M1SceneEventNone) {
        if (!m1_espnow_capture_share_step())
            m1_scene_pop(app);
        app->need_redraw = true;
        return true;
    }
    return false;
}

static void send_progress_draw(M1SceneApp *app)
{
    (void)app;
    m1_u8g2_firstpage();
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
    u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
    m1_draw_text(&m1_u8g2, 2, 9, 120, "Send Capture", TEXT_ALIGN_CENTER);
    u8g2_SetFont(&m1_u8g2, u8g2_font_NokiaSmallPlain_tf);
    m1_draw_text(&m1_u8g2, 2, 32, 120,
                 m1_espnow_capture_share_status(), TEXT_ALIGN_CENTER);
    m1_u8g2_nextpage();
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
