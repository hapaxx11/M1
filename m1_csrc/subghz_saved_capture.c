/* See COPYING.txt for license details. */

#include "subghz_saved_capture.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SUBGHZ_DEFAULT_PRESET "FuriHalSubGhzPresetOok650Async"
#define SUBGHZ_FSK_PRESET     "FuriHalSubGhzPreset2FSKDev476Async"
#define SUBGHZ_ASK_PRESET     "FuriHalSubGhzPresetASK"

static bool parse_u64(const char *text, uint64_t *out)
{
    if (!text || !out)
        return false;

    char *end = NULL;
    const unsigned long long value = strtoull(text, &end, 0);
    if (end == text || !end || *end != '\0')
        return false;

    *out = (uint64_t)value;
    return true;
}

static bool parse_u32(const char *text, uint32_t *out)
{
    uint64_t value = 0;
    if (!parse_u64(text, &value) || value > UINT32_MAX)
        return false;

    *out = (uint32_t)value;
    return true;
}

static bool has_capture_extension(const char *name)
{
    const char *ext = name ? strrchr(name, '.') : NULL;
    if (!ext || strlen(ext) != 4)
        return false;

    char folded[5];
    for (size_t i = 0; i < 5; i++)
        folded[i] = (char)tolower((unsigned char)ext[i]);
    return strcmp(folded, ".sub") == 0 || strcmp(folded, ".sgh") == 0;
}

void subghz_saved_capture_identity_from_info(
    const SubGHz_Dec_Info_t *info, const char *protocol_name,
    subghz_saved_match_identity_t *out)
{
    if (!out)
        return;

    memset(out, 0, sizeof(*out));
    if (!info || !protocol_name)
        return;

    snprintf(out->protocol, sizeof(out->protocol), "%s", protocol_name);
    out->key = info->key;
    out->serial = info->serial_number;
    out->counter = info->rolling_code;
    out->has_key = info->key != 0;
    out->has_serial = info->serial_number != 0;
    out->has_counter = out->has_serial;
}

void subghz_saved_capture_from_history(
    const SubGHz_History_Entry_t *entry, const char *protocol_name,
    flipper_subghz_key_capture_t *out)
{
    if (!out)
        return;

    memset(out, 0, sizeof(*out));
    if (!entry || !protocol_name)
        return;

    out->frequency = entry->frequency;
    out->preset = SUBGHZ_DEFAULT_PRESET;
    out->protocol = protocol_name;
    out->bit_count = entry->info.bit_len;
    out->key = entry->info.key;
    out->key_2 = subghz_capture_key2_value(&entry->info);
    out->key_2_len = subghz_capture_key2_len(entry->info.protocol,
                                             entry->info.serial_number);
    out->te = entry->info.te;
    out->serial = entry->info.serial_number;
    out->button = entry->info.button_id;
    out->counter = entry->info.rolling_code;
    out->type = (uint8_t)((entry->info.key2 >> 16) & 0xFFu);
    out->fx = (uint8_t)(entry->info.key2 & 0xFFu);
}

bool subghz_saved_capture_read_identity(
    const char *path, subghz_saved_match_identity_t *out)
{
    if (!path || !out)
        return false;

    memset(out, 0, sizeof(*out));
    flipper_file_t ff;
    if (!ff_open(&ff, path))
        return false;

    while (ff_read_line(&ff))
    {
        if (!ff_parse_kv(&ff))
            continue;

        const char *key = ff_get_key(&ff);
        const char *value = ff_get_value(&ff);
        if (strcmp(key, "Protocol") == 0)
        {
            snprintf(out->protocol, sizeof(out->protocol), "%s", value);
        }
        else if (strcmp(key, "Key") == 0)
        {
            uint8_t bytes[8];
            if (ff_parse_hex_bytes(value, bytes, sizeof(bytes)) == sizeof(bytes))
            {
                uint64_t parsed = 0;
                for (size_t i = 0; i < sizeof(bytes); i++)
                    parsed = (parsed << 8) | bytes[i];
                out->key = parsed;
                out->has_key = true;
            }
        }
        else if (strcmp(key, "Payload") == 0)
        {
            out->has_key = parse_u64(value, &out->key);
        }
        else if (strcmp(key, "Serial") == 0)
        {
            out->has_serial = parse_u32(value, &out->serial);
        }
        else if (strcmp(key, "Cnt") == 0)
        {
            out->has_counter = parse_u32(value, &out->counter);
        }
    }

    ff_close(&ff);
    return out->protocol[0] != '\0' && (out->has_serial || out->has_key);
}

bool subghz_saved_capture_find_match(
    const char *directory, const SubGHz_Dec_Info_t *received,
    const char *protocol_name, char *out_path, size_t out_path_size)
{
    if (!directory || !received || !protocol_name || !out_path ||
        out_path_size == 0)
        return false;

    out_path[0] = '\0';
    subghz_saved_match_identity_t rx;
    subghz_saved_capture_identity_from_info(received, protocol_name, &rx);
    if ((!rx.has_serial && !rx.has_key) || rx.protocol[0] == '\0')
        return false;

    DIR dir;
    if (f_opendir(&dir, directory) != FR_OK)
        return false;

    bool found = false;
    const size_t dir_len = strlen(directory);
    const char *separator = dir_len > 0 && directory[dir_len - 1] == '/' ? "" : "/";
    while (!found)
    {
        FILINFO info;
        memset(&info, 0, sizeof(info));
        if (f_readdir(&dir, &info) != FR_OK || info.fname[0] == '\0')
            break;
        if ((info.fattrib & AM_DIR) || !has_capture_extension(info.fname) ||
            strcmp(info.fname, "_flipper_tmp.sgh") == 0)
            continue;

        char path[128];
        const int length = snprintf(path, sizeof(path), "%s%s%s", directory,
                                    separator, info.fname);
        if (length < 0 || (size_t)length >= sizeof(path))
            continue;

        subghz_saved_match_identity_t saved;
        if (subghz_saved_capture_read_identity(path, &saved) &&
            subghz_saved_match_identity(&rx, &saved))
        {
            const int out_length = snprintf(out_path, out_path_size, "%s", path);
            if (out_length >= 0 && (size_t)out_length < out_path_size)
                found = true;
        }
    }

    f_closedir(&dir);
    return found;
}

bool subghz_saved_capture_get_format(
    const char *path, bool *is_m1_native, char *preset, size_t preset_size)
{
    if (!path || !is_m1_native || !preset || preset_size == 0)
        return false;

    flipper_file_t ff;
    if (!ff_open(&ff, path))
        return false;

    bool native = false;
    bool flipper = false;
    bool fsk = false;
    bool ask = false;
    snprintf(preset, preset_size, "%s", SUBGHZ_DEFAULT_PRESET);

    while (ff_read_line(&ff))
    {
        if (!ff_parse_kv(&ff))
            continue;

        const char *key = ff_get_key(&ff);
        const char *value = ff_get_value(&ff);
        if (strcmp(key, "Filetype") == 0)
        {
            native = strncmp(value, "M1 SubGHz", 9) == 0;
            flipper = strncmp(value, "Flipper SubGhz", 14) == 0;
        }
        else if (strcmp(key, "Preset") == 0)
        {
            snprintf(preset, preset_size, "%s", value);
        }
        else if (strcmp(key, "Modulation") == 0)
        {
            fsk = strcmp(value, "FSK") == 0;
            ask = strcmp(value, "ASK") == 0;
        }
    }

    ff_close(&ff);
    if (!native && !flipper)
        return false;

    if (native)
    {
        snprintf(preset, preset_size, "%s",
                 fsk ? SUBGHZ_FSK_PRESET
                     : (ask ? SUBGHZ_ASK_PRESET : SUBGHZ_DEFAULT_PRESET));
    }
    *is_m1_native = native;
    return true;
}
