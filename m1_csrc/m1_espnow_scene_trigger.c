/* See COPYING.txt for license details. */

/**
 * @file   m1_espnow_scene_trigger.c
 * @brief  ESP-NOW remote-trigger request/consent scenes.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "stm32h5xx_hal.h"
#include "main.h"
#include "m1_espnow_scene.h"
#include "m1_espnow_scene_ctx.h"
#include "m1_espnow_trigger_exec.h"
#include "m1_sub_ghz.h"
#include "m1_ir_universal.h"
#include "m1_scene.h"
#include "m1_submenu.h"
#include "m1_espnow_hal.h"
#include "m1_espnow_secure_link.h"
#include "espnow_trigger.h"
#include "m1_espnow_scene_scratch.h"
#include "espnow_shareable.h"
#include "m1_display.h"
#include "m1_lcd.h"
#include "m1_storage.h"
#include "m1_file_browser.h"
#include "ff.h"

#define TRIG_MENU_COUNT         2u
#define TRIG_KIND_COUNT         2u
#define TRIG_POLL_INTERVAL_MS   100u
#define TRIG_REPLY_TIMEOUT_MS   30000u

static const char *const s_trig_menu_labels[TRIG_MENU_COUNT] = {
    "Request Replay",
    "Allow Incoming",
};

static const uint8_t s_trig_menu_targets[TRIG_MENU_COUNT] = {
    EspnowSceneTriggerRequest,
    EspnowSceneTriggerListen,
};

static const char *const s_trig_kind_labels[TRIG_KIND_COUNT] = {
    "Sub-GHz",
    "Infrared",
};

static const espnow_share_kind_t s_trig_kinds[TRIG_KIND_COUNT] = {
    ESPNOW_SHARE_KIND_SUBGHZ,
    ESPNOW_SHARE_KIND_IR,
};

static subghz_submenu_model_t s_trig_menu_model;
static subghz_submenu_model_t s_trig_kind_model;

/* s_init_ctx (initiator/"Request Replay" flow) and s_resp_ctx (responder/
 * "Allow Incoming" flow) are alternate scenes — never active at once — so
 * they share one physical context, further shared with the sibling Peer
 * Link scenes; see m1_espnow_scene_scratch.h. */
#define s_init_ctx (g_m1_espnow_scene_scratch.trigger_ctx)
#define s_resp_ctx (g_m1_espnow_scene_scratch.trigger_ctx)
static char s_status[24];
static char s_detail[32];
static uint32_t s_last_poll_tick;
static uint32_t s_request_start_tick;
static bool s_have_peer;
static bool s_pending_request;
static bool s_terminal;
static bool s_execution_active;
static const char *s_subghz_tmp_path;

static const char *trigger_kind_label(espnow_share_kind_t kind)
{
    switch (kind) {
    case ESPNOW_SHARE_KIND_SUBGHZ: return "Sub-GHz";
    case ESPNOW_SHARE_KIND_IR:     return "Infrared";
    default:                       return "Unsupported";
    }
}

static const char *trigger_reject_label(espnow_trig_reject_t reason)
{
    switch (reason) {
    case ESPNOW_TRIG_REJECT_DISABLED:  return "Remote disabled";
    case ESPNOW_TRIG_REJECT_DENIED:    return "Denied";
    case ESPNOW_TRIG_REJECT_NOT_FOUND: return "Not found";
    case ESPNOW_TRIG_REJECT_BAD_NAME:  return "Bad request";
    default:                           return "Rejected";
    }
}

static void trigger_draw_card(const char *title, const char *line1,
                              const char *line2, const char *line3)
{
    m1_u8g2_firstpage();
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
    u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
    m1_draw_text(&m1_u8g2, 2, 9, 120, title, TEXT_ALIGN_CENTER);
    u8g2_DrawHLine(&m1_u8g2, 0, 10, M1_LCD_DISPLAY_WIDTH);

    u8g2_SetFont(&m1_u8g2, m1_menu_font());
    if (line1)
        m1_draw_text(&m1_u8g2, 2, 24, 120, line1, TEXT_ALIGN_CENTER);
    if (line2)
        m1_draw_text(&m1_u8g2, 2, 38, 120, line2, TEXT_ALIGN_CENTER);

    u8g2_SetFont(&m1_u8g2, u8g2_font_NokiaSmallPlain_tf);
    if (line3)
        m1_draw_text(&m1_u8g2, 2, 62, 120, line3, TEXT_ALIGN_CENTER);
    m1_u8g2_nextpage();
}

static bool trigger_send_status(espnow_trig_msg_t type, uint8_t code)
{
    uint8_t frame[2];
    size_t frame_len = 0;

    if (!espnow_trig_build_status(type, code, frame, sizeof(frame), &frame_len))
        return false;
    return m1_espnow_secure_link_send(m1_espnow_scene_ctx_peer_mac(),
                                      frame, frame_len);
}

static void trigger_reset_receiver(void)
{
    espnow_trigger_init(&s_resp_ctx, ESPNOW_TRIG_ROLE_RESPONDER, true);
    s_pending_request = false;
    s_execution_active = false;
}

static bool trigger_start_execution(void)
{
    char path[96];

    if (!espnow_trig_build_replay_path(s_resp_ctx.kind, s_resp_ctx.name,
                                       path, sizeof(path)))
        return false;

    if (s_resp_ctx.kind == ESPNOW_SHARE_KIND_SUBGHZ) {
        return sub_ghz_replay_prepare_flipper(path, &s_subghz_tmp_path) == 0u &&
               sub_ghz_replay_start_async() == 0u;
    }
    if (s_resp_ctx.kind == ESPNOW_SHARE_KIND_IR)
        return m1_ir_universal_start_file_all(path);
    return false;
}

static void trigger_finish_execution(M1SceneApp *app, bool ok)
{
    if (s_subghz_tmp_path != NULL) {
        f_unlink(s_subghz_tmp_path);
        s_subghz_tmp_path = NULL;
    }
    espnow_trigger_execution_done(&s_resp_ctx, ok);
    trigger_send_status(ESPNOW_TRIG_MSG_RESULT,
                        ok ? ESPNOW_TRIG_RESULT_OK : ESPNOW_TRIG_RESULT_FAIL);
    snprintf(s_status, sizeof(s_status), "%s",
             ok ? "Triggered OK" : "Trigger failed");
    s_execution_active = false;
    s_terminal = true;
    s_pending_request = false;
    app->need_redraw = true;
}

/*==========================================================================*/
/* Remote Trigger menu                                                      */
/*==========================================================================*/

static void trigger_menu_on_enter(M1SceneApp *app)
{
    subghz_submenu_model_init(&s_trig_menu_model, TRIG_MENU_COUNT,
                              M1_MENU_VIS(TRIG_MENU_COUNT));
    app->need_redraw = true;
}

static bool trigger_menu_on_event(M1SceneApp *app, M1SceneEvent event)
{
    return m1_submenu_event(app, event, &s_trig_menu_model,
                            s_trig_menu_targets);
}

static void trigger_menu_draw(M1SceneApp *app)
{
    (void)app;
    m1_submenu_draw(&s_trig_menu_model, "Remote Trigger",
                    s_trig_menu_labels);
}

static void trigger_on_exit(M1SceneApp *app) { (void)app; }

/*==========================================================================*/
/* Request category picker                                                  */
/*==========================================================================*/

static void trigger_request_on_enter(M1SceneApp *app)
{
    subghz_submenu_model_init(&s_trig_kind_model, TRIG_KIND_COUNT,
                              M1_MENU_VIS(TRIG_KIND_COUNT));
    m1_espnow_scene_ctx_set_share_kind(s_trig_kinds[0]);
    app->need_redraw = true;
}

static bool trigger_request_on_event(M1SceneApp *app, M1SceneEvent event)
{
    switch (event) {
    case M1SceneEventOk:
        m1_espnow_scene_ctx_set_share_kind(
            s_trig_kinds[s_trig_kind_model.selected]);
        m1_scene_push(app, EspnowSceneTriggerStatus);
        return true;
    case M1SceneEventUp:
    case M1SceneEventDown:
    case M1SceneEventBack:
        return m1_submenu_event(app, event, &s_trig_kind_model, NULL);
    default:
        return false;
    }
}

static void trigger_request_draw(M1SceneApp *app)
{
    (void)app;
    m1_submenu_draw(&s_trig_kind_model, "Request Replay",
                    s_trig_kind_labels);
}

/*==========================================================================*/
/* Initiator status                                                         */
/*==========================================================================*/

static void trigger_status_fail(const char *status)
{
    snprintf(s_status, sizeof(s_status), "%s", status);
    s_terminal = true;
}

static void trigger_status_on_enter(M1SceneApp *app)
{
    espnow_share_kind_t kind = m1_espnow_scene_ctx_share_kind();
    const char *dir = espnow_share_kind_dir(kind);
    S_M1_file_info *fi;
    char path[160];
    char name[ESPNOW_TRIG_NAME_MAX + 1u];
    uint8_t frame[M1_ESPNOW_SEND_PAYLOAD_MAX];
    size_t frame_len = 0;

    s_terminal = false;
    s_detail[0] = '\0';
    espnow_trigger_init(&s_init_ctx, ESPNOW_TRIG_ROLE_INITIATOR, false);
    s_have_peer = m1_espnow_scene_ctx_peer_mac() != NULL;
    if (!s_have_peer) {
        trigger_status_fail("Scan Peers first");
        app->need_redraw = true;
        return;
    }
    if (dir == NULL || !espnow_trig_kind_can_execute(kind)) {
        trigger_status_fail("Unsupported kind");
        app->need_redraw = true;
        return;
    }
    if (!m1_espnow_start(m1_espnow_get_channel())) {
        trigger_status_fail("Peer Link offline");
        app->need_redraw = true;
        return;
    }

    fi = storage_browse(dir);
    if (fi == NULL || !fi->file_is_selected || fi->status != FB_OK) {
        trigger_status_fail("No file selected");
        app->need_redraw = true;
        return;
    }

    snprintf(path, sizeof(path), "%s/%s", fi->dir_name, fi->file_name);
    if (!espnow_share_basename(path, name, sizeof(name)) ||
        !espnow_trig_build_request(kind, name, frame, sizeof(frame),
                                   &frame_len)) {
        trigger_status_fail("Invalid file name");
        app->need_redraw = true;
        return;
    }

    if (!m1_espnow_secure_link_send(m1_espnow_scene_ctx_peer_mac(), frame,
                                    frame_len) ||
        !espnow_trigger_request_sent(&s_init_ctx, kind, name)) {
        trigger_status_fail("Send failed");
        app->need_redraw = true;
        return;
    }

    snprintf(s_detail, sizeof(s_detail), "%s", name);
    snprintf(s_status, sizeof(s_status), "Waiting accept...");
    s_last_poll_tick = HAL_GetTick();
    s_request_start_tick = s_last_poll_tick;
    app->need_redraw = true;
}

static void trigger_status_poll(M1SceneApp *app)
{
    uint8_t from_mac[ESPNOW_MAC_LEN];
    uint8_t frame[M1_ESPNOW_SEND_PAYLOAD_MAX];
    uint8_t frame_len = 0;
    espnow_trig_msg_t type;
    uint8_t code = 0;
    uint32_t now = HAL_GetTick();

    if (s_terminal || !s_have_peer)
        return;
    if ((now - s_last_poll_tick) < TRIG_POLL_INTERVAL_MS)
        return;
    s_last_poll_tick = now;

    if ((now - s_request_start_tick) >= TRIG_REPLY_TIMEOUT_MS) {
        trigger_status_fail("Timed out");
        app->need_redraw = true;
        return;
    }

    if (!m1_espnow_secure_link_recv(from_mac, frame, sizeof(frame), &frame_len))
        return;
    if (memcmp(from_mac, m1_espnow_scene_ctx_peer_mac(), ESPNOW_MAC_LEN) != 0)
        return;
    if (!espnow_trig_parse_status(frame, frame_len, &type, &code))
        return;

    if (type == ESPNOW_TRIG_MSG_ACCEPT) {
        if (espnow_trigger_on_accept(&s_init_ctx))
            snprintf(s_status, sizeof(s_status), "Executing...");
    } else if (type == ESPNOW_TRIG_MSG_REJECT) {
        espnow_trigger_on_reject(&s_init_ctx, (espnow_trig_reject_t)code);
        snprintf(s_status, sizeof(s_status), "%s",
                 trigger_reject_label((espnow_trig_reject_t)code));
        s_terminal = true;
    } else if (type == ESPNOW_TRIG_MSG_RESULT) {
        espnow_trigger_on_result(&s_init_ctx, (espnow_trig_result_t)code);
        snprintf(s_status, sizeof(s_status), "%s",
                 code == ESPNOW_TRIG_RESULT_OK ? "Triggered OK"
                                                : "Trigger failed");
        s_terminal = true;
    }
    app->need_redraw = true;
}

static bool trigger_status_on_event(M1SceneApp *app, M1SceneEvent event)
{
    trigger_status_poll(app);

    switch (event) {
    case M1SceneEventOk:
    case M1SceneEventBack:
        if (s_terminal || event == M1SceneEventBack) {
            m1_scene_pop(app);
            return true;
        }
        return true;
    default:
        return false;
    }
}

static void trigger_status_draw(M1SceneApp *app)
{
    (void)app;
    trigger_draw_card("Remote Trigger", s_detail,
                      s_have_peer ? m1_espnow_scene_ctx_peer_name() : "",
                      s_status);
}

/*==========================================================================*/
/* Responder listen/consent                                                 */
/*==========================================================================*/

static void trigger_listen_on_enter(M1SceneApp *app)
{
    s_have_peer = m1_espnow_scene_ctx_peer_mac() != NULL;
    s_terminal = false;
    s_detail[0] = '\0';
    trigger_reset_receiver();

    if (!s_have_peer) {
        snprintf(s_status, sizeof(s_status), "Scan Peers first");
        s_terminal = true;
    } else if (!m1_espnow_start(m1_espnow_get_channel())) {
        snprintf(s_status, sizeof(s_status), "Peer Link offline");
        s_terminal = true;
    } else {
        snprintf(s_status, sizeof(s_status), "Waiting request");
    }

    s_last_poll_tick = HAL_GetTick();
    app->need_redraw = true;
}

static void trigger_listen_poll(M1SceneApp *app)
{
    uint8_t from_mac[ESPNOW_MAC_LEN];
    uint8_t frame[M1_ESPNOW_SEND_PAYLOAD_MAX];
    uint8_t frame_len = 0;
    espnow_share_kind_t kind;
    char name[ESPNOW_TRIG_NAME_MAX + 1u];
    uint32_t now = HAL_GetTick();

    if (s_terminal || s_pending_request || !s_have_peer)
        return;
    if ((now - s_last_poll_tick) < TRIG_POLL_INTERVAL_MS)
        return;
    s_last_poll_tick = now;

    if (!m1_espnow_secure_link_recv(from_mac, frame, sizeof(frame), &frame_len))
        return;
    if (memcmp(from_mac, m1_espnow_scene_ctx_peer_mac(), ESPNOW_MAC_LEN) != 0)
        return;
    if (frame_len == 0u || frame[0] != (uint8_t)ESPNOW_TRIG_MSG_REQUEST)
        return;

    if (!espnow_trig_parse_request(frame, frame_len, &kind, name,
                                   sizeof(name))) {
        trigger_send_status(ESPNOW_TRIG_MSG_REJECT,
                            ESPNOW_TRIG_REJECT_BAD_NAME);
        snprintf(s_status, sizeof(s_status), "Rejected bad req");
        app->need_redraw = true;
        return;
    }

    if (!espnow_trigger_on_request(&s_resp_ctx, kind, name)) {
        trigger_send_status(ESPNOW_TRIG_MSG_REJECT,
                            s_resp_ctx.reject_reason);
        snprintf(s_status, sizeof(s_status), "%s",
                 trigger_reject_label(s_resp_ctx.reject_reason));
        trigger_reset_receiver();
        app->need_redraw = true;
        return;
    }

    if (!m1_espnow_trigger_capture_exists(kind, name)) {
        trigger_send_status(ESPNOW_TRIG_MSG_REJECT,
                            ESPNOW_TRIG_REJECT_NOT_FOUND);
        snprintf(s_status, sizeof(s_status), "Not found");
        trigger_reset_receiver();
        app->need_redraw = true;
        return;
    }

    snprintf(s_detail, sizeof(s_detail), "%s: %s",
             trigger_kind_label(kind), name);
    snprintf(s_status, sizeof(s_status), "OK allow, Back deny");
    s_pending_request = true;
    app->need_redraw = true;
}

static bool trigger_listen_on_event(M1SceneApp *app, M1SceneEvent event)
{
    trigger_listen_poll(app);

    switch (event) {
    case M1SceneEventOk:
        if (s_pending_request && espnow_trigger_grant(&s_resp_ctx)) {
            if (!trigger_send_status(ESPNOW_TRIG_MSG_ACCEPT, 0u)) {
                snprintf(s_status, sizeof(s_status), "Send failed");
                s_terminal = true;
                s_pending_request = false;
                app->need_redraw = true;
                return true;
            }
            snprintf(s_status, sizeof(s_status), "Executing...");
            s_execution_active = trigger_start_execution();
            if (!s_execution_active)
                trigger_finish_execution(app, false);
            else
                app->need_redraw = true;
        }
        return true;

    case M1SceneEventSubghzTx:
        if (s_execution_active &&
            s_resp_ctx.kind == ESPNOW_SHARE_KIND_SUBGHZ) {
            sub_ghz_replay_async_status_t status =
                sub_ghz_replay_continue_async(false);
            if (status != SUBGHZ_REPLAY_ASYNC_RUNNING)
                trigger_finish_execution(app, status == SUBGHZ_REPLAY_ASYNC_DONE);
            return true;
        }
        return false;

    case M1SceneEventInfraredTx:
        if (s_execution_active && s_resp_ctx.kind == ESPNOW_SHARE_KIND_IR) {
            m1_ir_file_tx_status_t status = m1_ir_universal_continue_file_all();
            if (status != M1_IR_FILE_TX_RUNNING)
                trigger_finish_execution(app, status == M1_IR_FILE_TX_DONE);
            return true;
        }
        return false;

    case M1SceneEventBack:
        if (s_execution_active) {
            if (s_resp_ctx.kind == ESPNOW_SHARE_KIND_SUBGHZ)
                sub_ghz_replay_abort();
            else if (s_resp_ctx.kind == ESPNOW_SHARE_KIND_IR)
                m1_ir_universal_abort_file_all();
            trigger_finish_execution(app, false);
            return true;
        }
        if (s_pending_request) {
            espnow_trigger_deny(&s_resp_ctx);
            trigger_send_status(ESPNOW_TRIG_MSG_REJECT,
                                ESPNOW_TRIG_REJECT_DENIED);
            snprintf(s_status, sizeof(s_status), "Denied");
            trigger_reset_receiver();
            app->need_redraw = true;
            return true;
        }
        m1_scene_pop(app);
        return true;

    default:
        return false;
    }
}

static void trigger_listen_draw(M1SceneApp *app)
{
    (void)app;
    if (s_pending_request) {
        trigger_draw_card("Allow Trigger", s_detail,
                          m1_espnow_scene_ctx_peer_name(), s_status);
    } else {
        trigger_draw_card("Allow Trigger", m1_espnow_scene_ctx_peer_name(),
                          "Remote replay is ON", s_status);
    }
}

const M1SceneHandlers espnow_scene_trigger_menu_handlers = {
    .on_enter = trigger_menu_on_enter,
    .on_event = trigger_menu_on_event,
    .on_exit  = trigger_on_exit,
    .draw     = trigger_menu_draw,
};

const M1SceneHandlers espnow_scene_trigger_request_handlers = {
    .on_enter = trigger_request_on_enter,
    .on_event = trigger_request_on_event,
    .on_exit  = trigger_on_exit,
    .draw     = trigger_request_draw,
};

const M1SceneHandlers espnow_scene_trigger_status_handlers = {
    .on_enter = trigger_status_on_enter,
    .on_event = trigger_status_on_event,
    .on_exit  = trigger_on_exit,
    .draw     = trigger_status_draw,
};

const M1SceneHandlers espnow_scene_trigger_listen_handlers = {
    .on_enter = trigger_listen_on_enter,
    .on_event = trigger_listen_on_event,
    .on_exit  = trigger_on_exit,
    .draw     = trigger_listen_draw,
};
