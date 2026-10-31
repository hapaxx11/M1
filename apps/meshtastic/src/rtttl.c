#include "rtttl.h"

void ringtones_scan(ZeroMeshApp* app) {
    if(app) app->custom_count = 0; /* custom .rtttl scanning deferred from v1 */
}

uint16_t ringtone_total(const ZeroMeshApp* app) {
    return (uint16_t)(RINGTONE_COUNT + (app ? app->custom_count : 0));
}

void ringtone_label(const ZeroMeshApp* app, uint16_t index, char* out, size_t cap) {
    extern const char* ringtone_names[];
    if(!out || !cap) return;
    if(index < RINGTONE_COUNT) {
        snprintf(out, cap, "%s", ringtone_names[index]);
        return;
    }
    uint16_t i = index - RINGTONE_COUNT;
    if(app && i < app->custom_count) {
        snprintf(out, cap, "%s", app->custom_files[i]);
    } else {
        snprintf(out, cap, "?");
    }
}

int16_t ringtone_index_of(const ZeroMeshApp* app, const char* filename) {
    (void)app;
    (void)filename;
    return -1;
}

bool rtttl_play_custom(const ZeroMeshApp* app, uint16_t index) {
    (void)app;
    (void)index;
    return false;
}
