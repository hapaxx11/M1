/*
 * main.c — M1 Meshtastic app shell.
 *
 * Replaces ZeroMesh's zeromesh_serial_app.c. The Flipper build ran a GUI
 * ViewPort plus a dedicated RX thread with a mutex; the M1 app model is a
 * single cooperative FreeRTOS task with a blocking button poll and a paged
 * u8g2 draw loop, so everything folds into one loop:
 *
 *   drain UART -> decode -> service pending actions / config retries / heartbeat
 *   -> poll a button -> dispatch -> (optional) keyboard -> render
 *
 * The app requires the firmware UART + extended-button primitives this port
 * adds (m1_uart_app_*, m1_poll_button_ex); without them the loader reports a
 * missing symbol. See README.md.
 */
#include "mesh_app.h"
#include "gui.h"
#include "protocol.h"
#include "transport.h"
#include "history.h"
#include "channel.h"
#include "settings.h"
#include "notify.h"
#include "rtttl.h"

M1_APP_MANIFEST("Meshtastic", 4096); /* 16 KB task stack: headroom for nanopb decode */

/* The app state is large; keep it in .bss rather than on the task stack. */
static ZeroMeshApp g_app;

/* ---- button translation: M1 poll -> Flipper-style InputEvent ---- */
static InputKey btn_to_key(m1app_button_t b, bool* ok) {
    *ok = true;
    switch(b) {
    case M1APP_BTN_UP: return InputKeyUp;
    case M1APP_BTN_DOWN: return InputKeyDown;
    case M1APP_BTN_LEFT: return InputKeyLeft;
    case M1APP_BTN_RIGHT: return InputKeyRight;
    case M1APP_BTN_OK: return InputKeyOk;
    case M1APP_BTN_BACK: return InputKeyBack;
    default: *ok = false; return InputKeyMAX;
    }
}

/* Free-text entry via the M1's multi-page keyboard (m1_vkb_get_filename is the
 * alpha keyboard; m1_vkbs_get_data is hex-only). Length is bounded by the
 * firmware keyboard. */
static void run_keyboard(ZeroMeshApp* app) {
    char buf[64];
    buf[0] = '\0';
    if(m1_vkb_get_filename("Message:", "", buf) && buf[0]) {
        uint32_t to = 0xFFFFFFFFu; /* broadcast */
        if(app->ui_mode == PAGE_ROSTER && app->roster.state == RosterStateChat) {
            to = app->roster.nodes[app->roster.selected_idx].node_id;
        }
        strlcpy(app->pending_text, buf, sizeof(app->pending_text));
        app->pending_node = to;
        app->pending_action = PendingSendText;
    }
    app->need_render = true;
}

static void service_pending(ZeroMeshApp* app) {
    if(app->pending_action == PendingNone) return;
    PendingAction act = app->pending_action;
    app->pending_action = PendingNone;
    switch(act) {
    case PendingPosReq:
        request_position(app, app->pending_node);
        break;
    case PendingInfoReq:
        request_node_info(app, app->pending_node);
        break;
    case PendingSetLora:
        set_node_lora(app, app->pending_a, app->pending_b);
        break;
    case PendingSetGps:
        set_node_gps(app, app->cfg_gps != 0);
        break;
    case PendingGetChannel:
        request_channel(app, 0);
        break;
    case PendingSetChannel:
        set_channel_config(app, app->cfg_ch_private != 0, app->cfg_ch_pos != 0);
        break;
    case PendingSetFixed:
        /* The fixed-position coordinate came from the map crosshair, which is
           deferred in v1. Clearing still works; setting needs the map. */
        set_status(app, "Fixed pos needs map (v1)");
        break;
    case PendingClearFixed:
        clear_fixed_position(app);
        break;
    case PendingSetRole:
        set_node_role(app, app->pending_a);
        break;
    case PendingReboot:
        reboot_node(app, 5);
        break;
    case PendingInfoAll:
        request_info(app);
        break;
    case PendingSendText:
        send_text_message(app, app->pending_text, app->pending_node);
        app->pending_text[0] = '\0';
        break;
    case PendingPlayTone:
        play_ringtone(app);
        break;
    default:
        break;
    }
}

int32_t app_main(void* context) {
    (void)context;

    ZeroMeshApp* app = &g_app;
    memset(app, 0, sizeof(*app));

    app->baud = 115200;
    app->ui_mode = PAGE_MESSAGES;
    app->notify_vibro = true;
    app->notify_led = true;
    app->notify_ringtone = RingtoneShort;
    app->scroll_speed = 5;
    app->scroll_framerate = 5;
    app->lmh_mode = LMH_Scroll;

    game_rand_seed();
    channel_init(app);
    ringtones_scan(app);
    settings_load(app);

    snprintf(app->status, sizeof(app->status), "Connecting...");
    for(int i = 0; i < LOG_LINES; i++) app->lines[i][0] = '\0';

    protocol_init(app);
    transport_open(app);
    m1app_delay(300);

    bool config_requested = false;
    uint32_t last_config_req = 0, last_chan_req = 0, last_pos_req = 0;
    const uint32_t CONFIG_RETRY_MS = 5000;
    uint32_t last_render = m1app_get_tick();
    uint32_t last_heartbeat = m1app_get_tick();
    const uint32_t HEARTBEAT_INTERVAL_MS = 30000;
    const uint32_t frame_delays[] = {1000, 500, 333, 250, 200, 166, 142, 125, 111, 100};

    u8g2_t* u8g2 = m1app_get_u8g2();

    while(!app->stop_thread) {
        /* 1. Pull bytes off the UART and decode any complete frames. */
        protocol_poll(app);

        /* 2. Deferred work queued by the UI or the decoder. */
        if(app->pending_notify) {
            app->pending_notify = false;
            notify_rx_message(app);
        }
        service_pending(app);

        /* 3. Ask the radio for its identity / channel / position config until
              it answers (see ZeroMesh: BLE had no reliable "subscribed" edge,
              and retrying is harmless over UART too). */
        if(transport_is_up(app)) {
            uint32_t now = m1app_get_tick();
            if(app->my_node_num == 0 &&
               (!config_requested || now - last_config_req >= CONFIG_RETRY_MS)) {
                request_info(app);
                config_requested = true;
                last_config_req = now;
            }
            if(app->my_node_num && !app->cfg_ch_known && now - last_chan_req >= CONFIG_RETRY_MS) {
                request_channel(app, 0);
                last_chan_req = now;
            }
            if(app->my_node_num && !app->cfg_pos_known && now - last_pos_req >= CONFIG_RETRY_MS) {
                request_position_config(app);
                last_pos_req = now;
            }
        } else {
            config_requested = false;
            app->cfg_ch_known = false;
            app->cfg_pos_known = false;
        }

        if(m1app_get_tick() - last_heartbeat >= HEARTBEAT_INTERVAL_MS) {
            send_heartbeat(app);
            last_heartbeat = m1app_get_tick();
        }

        /* 4. Input. A short poll timeout keeps the UART drained and paces the
              loop; the firmware primitive reports click vs long-click. */
        uint8_t evt = M1_BTN_EVT_NONE;
        m1app_button_t b = m1_poll_button_ex(12, &evt);
        if(b != M1APP_BTN_NONE) {
            bool ok = false;
            InputKey key = btn_to_key(b, &ok);
            if(ok) {
                InputEvent e;
                e.key = key;
                e.type = (evt == M1_BTN_EVT_LONG) ? InputTypeLong : InputTypeShort;
                input_dispatch(&e, app);
            }
        }

        /* 5. Modal keyboard (message compose). */
        if(app->show_keyboard) {
            app->show_keyboard = false;
            run_keyboard(app);
            last_render = m1app_get_tick();
        }

        /* 6. Render on demand, or on the scroll-animation cadence. */
        uint32_t now = m1app_get_tick();
        uint32_t frame_delay = frame_delays[app->scroll_framerate - 1];
        if(app->need_render || now - last_render >= frame_delay) {
            app->need_render = false;
            m1app_display_begin();
            do {
                render_page(u8g2, app);
            } while(m1app_display_flush());
            last_render = now;
        }
    }

    settings_save(app);
    transport_close(app);
    return 0;
}
