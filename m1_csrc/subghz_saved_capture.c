/* See COPYING.txt for license details. */

#include "subghz_saved_capture.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "flipper_file.h"

#define SUBGHZ_DEFAULT_PRESET "FuriHalSubGhzPresetOok650Async"
#define SUBGHZ_FSK_PRESET     "FuriHalSubGhzPreset2FSKDev476Async"
#define SUBGHZ_ASK_PRESET     "FuriHalSubGhzPresetASK"
#define SUBGHZ_SAVED_CAPTURE_PATH_MAX 128u
#define SUBGHZ_SAVED_CAPTURE_MAX_DEPTH 4u

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

    bool has_seed = false;
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
        else if (strcmp(key, "Seed") == 0)
        {
            has_seed = true;
        }
    }

    ff_close(&ff);
    out->is_recovered_psa = has_seed && strcmp(out->protocol, "PSA") == 0;
    return out->protocol[0] != '\0' && (out->has_serial || out->has_key);
}

static bool subghz_saved_capture_join_path(
    char *out, size_t out_size, const char *directory, const char *name)
{
    const size_t dir_len = strlen(directory);
    const char *separator = dir_len > 0 && directory[dir_len - 1] == '/' ? "" : "/";
    const int length = snprintf(out, out_size, "%s%s%s", directory, separator, name);
    return length >= 0 && (size_t)length < out_size;
}

static bool subghz_saved_capture_find_match_in_directory(
    const char *directory, const subghz_saved_match_identity_t *received,
    char *out_path, size_t out_path_size, uint8_t depth)
{
    DIR dir;
    if (f_opendir(&dir, directory) != FR_OK)
        return false;

    bool found = false;
    while (!found)
    {
        FILINFO info;
        memset(&info, 0, sizeof(info));
        if (f_readdir(&dir, &info) != FR_OK || info.fname[0] == '\0')
            break;

        if (info.fattrib & AM_DIR)
        {
            if (depth < SUBGHZ_SAVED_CAPTURE_MAX_DEPTH &&
                strcmp(info.fname, ".") != 0 &&
                strcmp(info.fname, "..") != 0)
            {
                char nested[SUBGHZ_SAVED_CAPTURE_PATH_MAX];
                if (subghz_saved_capture_join_path(
                        nested, sizeof(nested), directory, info.fname))
                {
                    found = subghz_saved_capture_find_match_in_directory(
                        nested, received, out_path, out_path_size,
                        (uint8_t)(depth + 1u));
                }
            }
            continue;
        }

        if (!has_capture_extension(info.fname) ||
            strcmp(info.fname, "_flipper_tmp.sgh") == 0)
            continue;

        char path[SUBGHZ_SAVED_CAPTURE_PATH_MAX];
        if (!subghz_saved_capture_join_path(
                path, sizeof(path), directory, info.fname))
            continue;

        subghz_saved_match_identity_t saved;
        if (subghz_saved_capture_read_identity(path, &saved) &&
            !saved.is_recovered_psa &&
            subghz_saved_match_identity(received, &saved))
        {
            const int out_length = snprintf(out_path, out_path_size, "%s", path);
            if (out_length >= 0 && (size_t)out_length < out_path_size)
                found = true;
        }
    }

    f_closedir(&dir);
    return found;
}

bool subghz_saved_capture_find_match(
    const char *directory, const SubGHz_Dec_Info_t *received,
    const char *protocol_name, bool allow_serial_match,
    char *out_path, size_t out_path_size)
{
    if (!directory || !received || !protocol_name || !out_path ||
        out_path_size == 0)
        return false;

    out_path[0] = '\0';
    subghz_saved_match_identity_t rx;
    subghz_saved_capture_identity_from_info(received, protocol_name, &rx);
    rx.allow_serial_match = allow_serial_match;
    if ((!rx.has_serial && !rx.has_key) || rx.protocol[0] == '\0')
        return false;

    return subghz_saved_capture_find_match_in_directory(
        directory, &rx, out_path, out_path_size, 0);
}

bool subghz_saved_capture_update(
    const char *path, bool m1_native,
    const flipper_subghz_key_capture_t *capture,
    subghz_saved_capture_writer_t writer)
{
    if (!path || !capture || !writer)
        return false;

    char temporary[SUBGHZ_SAVED_CAPTURE_PATH_MAX];
    char backup[SUBGHZ_SAVED_CAPTURE_PATH_MAX];
    int length = snprintf(temporary, sizeof(temporary), "%s.update.tmp", path);
    if (length < 0 || (size_t)length >= sizeof(temporary))
        return false;
    length = snprintf(backup, sizeof(backup), "%s.update.bak", path);
    if (length < 0 || (size_t)length >= sizeof(backup))
        return false;

    (void)f_unlink(temporary);
    if (!writer(temporary, m1_native, capture))
    {
        (void)f_unlink(temporary);
        return false;
    }

    if (f_rename(path, backup) != FR_OK)
    {
        (void)f_unlink(temporary);
        return false;
    }
    if (f_rename(temporary, path) != FR_OK)
    {
        (void)f_rename(backup, path);
        (void)f_unlink(temporary);
        return false;
    }

    (void)f_unlink(backup);
    return true;
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
