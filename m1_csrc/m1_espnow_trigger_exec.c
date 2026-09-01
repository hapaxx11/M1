/* See COPYING.txt for license details. */

/**
 * @file   m1_espnow_trigger_exec.c
 * @brief  Hardware-facing execution adapter for ESP-NOW remote trigger.
 */

#include "m1_espnow_trigger_exec.h"

#include <stdint.h>

#include "ff.h"
#include "m1_sub_ghz.h"
#include "m1_ir_universal.h"

bool m1_espnow_trigger_capture_exists(espnow_share_kind_t kind,
                                      const char *name)
{
    char path[96];
    FILINFO info;

    if (!espnow_trig_build_replay_path(kind, name, path, sizeof(path)))
        return false;
    return f_stat(path, &info) == FR_OK;
}

bool m1_espnow_trigger_execute(espnow_share_kind_t kind, const char *name)
{
    char path[96];

    if (!espnow_trig_build_replay_path(kind, name, path, sizeof(path)))
        return false;

    switch (kind) {
    case ESPNOW_SHARE_KIND_SUBGHZ:
        return sub_ghz_replay_flipper_file(path) == 0u;

    case ESPNOW_SHARE_KIND_IR:
        return m1_ir_universal_send_file_all(path);

    default:
        return false;
    }
}
