#include "history.h"
#include "mesh_app.h"

/* Single-task port: the mutexes ZeroMesh used to guard its RX thread compile
 * to no-ops via compat.h, and FURI_LOG lines are dropped. */

void history_add(ZeroMeshApp* app, const char* text, uint32_t from, uint32_t to, bool is_tx) {
    uint8_t idx = app->history.head;
    Message* msg = &app->history.msgs[idx];

    snprintf(msg->text, sizeof(msg->text), "%s", text);
    msg->from = from;
    msg->to = to;
    msg->is_tx = is_tx;
    msg->timestamp = furi_get_tick() / 1000;

    app->history.head = (app->history.head + 1) % MSG_HISTORY;
    if(app->history.count < MSG_HISTORY) app->history.count++;

    app->need_render = true;
}

void log_line(ZeroMeshApp* app, const char* fmt, ...) {
    if(!app) return;

    char buf[LOG_COLS];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    snprintf(app->lines[app->line_head], LOG_COLS, "%s", buf);
    app->line_head = (app->line_head + 1) % LOG_LINES;

    if(app->log_paused) {
        if(app->log_scroll_offset < LOG_LINES - 7) {
            app->log_scroll_offset++;
        }
    }
}

void set_status(ZeroMeshApp* app, const char* fmt, ...) {
    if(!app) return;

    va_list args;
    va_start(args, fmt);
    vsnprintf(app->status, sizeof(app->status), fmt, args);
    va_end(args);

    app->need_render = true;
}
