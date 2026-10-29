/*
 * settings.c — persistence on the M1 SD card (FatFS), replacing ZeroMesh's
 * Flipper Storage calls. Same key=value format; the Transport and UART-port
 * keys are gone (single header UART, no BLE in v1) and are ignored if present
 * in an older file.
 */
#include "settings.h"
#include "rtttl.h"

#define SETTINGS_VERSION 1

void settings_save(ZeroMeshApp* app) {
    if(!app) return;

    fs_directory_ensure(SETTINGS_DIR);

    FIL file;
    if(f_open(&file, SETTINGS_PATH, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK) return;

    char line[64];
    unsigned int bw;
    #define WR(...)                                            \
        do {                                                   \
            int _n = snprintf(line, sizeof(line), __VA_ARGS__);\
            if(_n > 0) f_write(&file, line, (unsigned)_n, &bw);\
        } while(0)

    WR("version=%d\n", SETTINGS_VERSION);
    WR("baud=%lu\n", (unsigned long)app->baud);
    WR("vibro=%d\n", app->notify_vibro ? 1 : 0);
    WR("led=%d\n", app->notify_led ? 1 : 0);
    int builtin = app->notify_ringtone < RINGTONE_COUNT ? (int)app->notify_ringtone : 1;
    WR("ringtone=%d\n", builtin);
    WR("scroll_speed=%d\n", app->scroll_speed);
    WR("scroll_fps=%d\n", app->scroll_framerate);
    WR("lmh_mode=%d\n", (int)app->lmh_mode);
    #undef WR

    f_close(&file);
}

void settings_load(ZeroMeshApp* app) {
    if(!app) return;
    if(!fs_file_exists(SETTINGS_PATH)) return;

    FIL file;
    if(f_open(&file, SETTINGS_PATH, FA_READ) != FR_OK) return;

    char buffer[512];
    unsigned int br = 0;
    if(f_read(&file, buffer, sizeof(buffer) - 1, &br) != FR_OK) {
        f_close(&file);
        return;
    }
    f_close(&file);
    buffer[br] = '\0';

    char* pos = buffer;
    while(pos < buffer + br) {
        char* line_end = strchr(pos, '\n');
        if(!line_end) line_end = buffer + br;

        size_t line_len = (size_t)(line_end - pos);
        if(line_len > 0 && line_len < 128) {
            char line[128];
            memcpy(line, pos, line_len);
            line[line_len] = '\0';

            char* equals = strchr(line, '=');
            if(equals) {
                *equals = '\0';
                char* key = line;
                char* value_str = equals + 1;
                int value = atoi(value_str);

                if(strcmp(key, "baud") == 0) {
                    app->baud = (uint32_t)value;
                } else if(strcmp(key, "vibro") == 0) {
                    app->notify_vibro = (value != 0);
                } else if(strcmp(key, "led") == 0) {
                    app->notify_led = (value != 0);
                } else if(strcmp(key, "ringtone") == 0) {
                    if(value >= 0 && value < RINGTONE_COUNT) app->notify_ringtone = (uint16_t)value;
                } else if(strcmp(key, "scroll_speed") == 0) {
                    if(value >= 1 && value <= 10) app->scroll_speed = (uint8_t)value;
                } else if(strcmp(key, "scroll_fps") == 0) {
                    if(value >= 1 && value <= 10) app->scroll_framerate = (uint8_t)value;
                } else if(strcmp(key, "lmh_mode") == 0) {
                    if(value >= 0 && value < LMH_COUNT) app->lmh_mode = (LongMessageHandling)value;
                }
            }
        }
        pos = line_end + 1;
    }
}
