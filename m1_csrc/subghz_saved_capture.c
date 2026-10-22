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
#define SUBGHZ_SAVED_CAPTURE_SCAN_LINES 64u

typedef struct {
    DIR dir;
    char path[SUBGHZ_SAVED_CAPTURE_PATH_MAX];
} subghz_saved_capture_dir_t;

struct subghz_saved_capture_scan {
    subghz_saved_match_identity_t received;
    char protocol[SUBGHZ_SAVED_MATCH_PROTOCOL_MAX_LEN];
    subghz_saved_capture_dir_t directories[SUBGHZ_SAVED_CAPTURE_MAX_DEPTH + 1u];
    uint8_t directory_count;
    flipper_file_t file;
    char candidate_path[SUBGHZ_SAVED_CAPTURE_PATH_MAX];
    subghz_saved_match_identity_t candidate;
    uint8_t candidate_lines;
    bool candidate_open;
    bool candidate_has_seed;
};

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
        else if (strcmp(key, "Manufacture") == 0)
            out->has_manufacture = true;
        else if (strcmp(key, "CounterMode") == 0)
            out->has_counter_mode = true;
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

static void subghz_saved_capture_scan_close_candidate(
    subghz_saved_capture_scan_t *scan)
{
    if (scan->candidate_open)
        (void)ff_close(&scan->file);
    scan->candidate_open = false;
}

static subghz_saved_capture_scan_result_t
subghz_saved_capture_scan_finish_candidate(
    subghz_saved_capture_scan_t *scan, char *out_path, size_t out_path_size)
{
    subghz_saved_capture_scan_close_candidate(scan);
    scan->candidate.is_recovered_psa =
        scan->candidate_has_seed && strcmp(scan->candidate.protocol, "PSA") == 0;
    const bool matches =
        scan->candidate.protocol[0] != '\0' &&
        (scan->candidate.has_serial || scan->candidate.has_key) &&
        !scan->candidate.is_recovered_psa &&
        !scan->candidate.has_manufacture &&
        !scan->candidate.has_counter_mode &&
        subghz_saved_match_identity(&scan->received, &scan->candidate);
    if (matches)
    {
        const int out_length =
            snprintf(out_path, out_path_size, "%s", scan->candidate_path);
        if (out_length >= 0 && (size_t)out_length < out_path_size)
            return SubGhzSavedCaptureScanFound;
    }
    return SubGhzSavedCaptureScanPending;
}

subghz_saved_capture_scan_t *subghz_saved_capture_scan_start(
    const char *directory, const SubGHz_Dec_Info_t *received,
    const char *protocol_name, bool allow_serial_match)
{
    if (!directory || !received || !protocol_name)
        return NULL;

    subghz_saved_capture_scan_t *scan = calloc(1, sizeof(*scan));
    if (!scan)
        return NULL;

    subghz_saved_capture_identity_from_info(received, protocol_name,
                                            &scan->received);
    scan->received.allow_serial_match = allow_serial_match;
    snprintf(scan->protocol, sizeof(scan->protocol), "%s", protocol_name);
    if ((!scan->received.has_serial && !scan->received.has_key) ||
        scan->protocol[0] == '\0' ||
        !subghz_saved_capture_join_path(
            scan->directories[0].path, sizeof(scan->directories[0].path),
            directory, ""))
    {
        free(scan);
        return NULL;
    }

    size_t path_length = strlen(scan->directories[0].path);
    while (path_length > 1u && scan->directories[0].path[path_length - 1u] == '/')
        scan->directories[0].path[--path_length] = '\0';
    if (f_opendir(&scan->directories[0].dir,
                  scan->directories[0].path) != FR_OK)
    {
        free(scan);
        return NULL;
    }
    scan->directory_count = 1u;
    return scan;
}

static subghz_saved_capture_scan_result_t
subghz_saved_capture_scan_read_candidate(
    subghz_saved_capture_scan_t *scan, char *out_path, size_t out_path_size)
{
    if (scan->candidate_lines++ >= SUBGHZ_SAVED_CAPTURE_SCAN_LINES ||
        !ff_read_line(&scan->file))
        return subghz_saved_capture_scan_finish_candidate(
            scan, out_path, out_path_size);

    if (!ff_parse_kv(&scan->file))
        return SubGhzSavedCaptureScanPending;

    const char *key = ff_get_key(&scan->file);
    const char *value = ff_get_value(&scan->file);
    if (strcmp(key, "Protocol") == 0)
    {
        snprintf(scan->candidate.protocol, sizeof(scan->candidate.protocol),
                 "%s", value);
        if (strcmp(scan->candidate.protocol, scan->protocol) != 0)
        {
            subghz_saved_capture_scan_close_candidate(scan);
            return SubGhzSavedCaptureScanPending;
        }
    }
    else if (strcmp(key, "Key") == 0)
    {
        uint8_t bytes[8];
        if (ff_parse_hex_bytes(value, bytes, sizeof(bytes)) == sizeof(bytes))
        {
            for (size_t i = 0; i < sizeof(bytes); i++)
                scan->candidate.key =
                    (scan->candidate.key << 8) | bytes[i];
            scan->candidate.has_key = true;
        }
    }
    else if (strcmp(key, "Payload") == 0)
    {
        scan->candidate.has_key =
            parse_u64(value, &scan->candidate.key);
    }
    else if (strcmp(key, "Serial") == 0)
    {
        scan->candidate.has_serial =
            parse_u32(value, &scan->candidate.serial);
    }
    else if (strcmp(key, "Cnt") == 0)
    {
        scan->candidate.has_counter =
            parse_u32(value, &scan->candidate.counter);
    }
    else if (strcmp(key, "Seed") == 0)
    {
        scan->candidate_has_seed = true;
    }
    else if (strcmp(key, "Manufacture") == 0)
        scan->candidate.has_manufacture = true;
    else if (strcmp(key, "CounterMode") == 0)
        scan->candidate.has_counter_mode = true;
    else if (strcmp(key, "RAW_Data") == 0 || strcmp(key, "Data") == 0)
    {
        return subghz_saved_capture_scan_finish_candidate(
            scan, out_path, out_path_size);
    }
    return SubGhzSavedCaptureScanPending;
}

subghz_saved_capture_scan_result_t subghz_saved_capture_scan_step(
    subghz_saved_capture_scan_t *scan, char *out_path, size_t out_path_size)
{
    if (!scan || !out_path || out_path_size == 0)
        return SubGhzSavedCaptureScanDone;
    out_path[0] = '\0';

    if (scan->candidate_open)
        return subghz_saved_capture_scan_read_candidate(
            scan, out_path, out_path_size);

    if (scan->directory_count == 0)
        return SubGhzSavedCaptureScanDone;

    subghz_saved_capture_dir_t *current =
        &scan->directories[scan->directory_count - 1u];
    FILINFO info;
    memset(&info, 0, sizeof(info));
    if (f_readdir(&current->dir, &info) != FR_OK || info.fname[0] == '\0')
    {
        (void)f_closedir(&current->dir);
        scan->directory_count--;
        return scan->directory_count == 0
                   ? SubGhzSavedCaptureScanDone
                   : SubGhzSavedCaptureScanPending;
    }

    if (info.fattrib & AM_DIR)
    {
        if (scan->directory_count < SUBGHZ_SAVED_CAPTURE_MAX_DEPTH + 1u &&
            strcmp(info.fname, ".") != 0 &&
            strcmp(info.fname, "..") != 0)
        {
            subghz_saved_capture_dir_t *nested =
                &scan->directories[scan->directory_count];
            if (subghz_saved_capture_join_path(
                    nested->path, sizeof(nested->path), current->path,
                    info.fname) &&
                f_opendir(&nested->dir, nested->path) == FR_OK)
                scan->directory_count++;
        }
        return SubGhzSavedCaptureScanPending;
    }

    if (!has_capture_extension(info.fname) ||
        strcmp(info.fname, "_flipper_tmp.sgh") == 0 ||
        !subghz_saved_capture_join_path(
            scan->candidate_path, sizeof(scan->candidate_path),
            current->path, info.fname))
        return SubGhzSavedCaptureScanPending;

    memset(&scan->candidate, 0, sizeof(scan->candidate));
    scan->candidate_lines = 0;
    scan->candidate_has_seed = false;
    if (ff_open(&scan->file, scan->candidate_path))
        scan->candidate_open = true;
    return SubGhzSavedCaptureScanPending;
}

void subghz_saved_capture_scan_destroy(subghz_saved_capture_scan_t *scan)
{
    if (!scan)
        return;
    subghz_saved_capture_scan_close_candidate(scan);
    while (scan->directory_count > 0)
    {
        scan->directory_count--;
        (void)f_closedir(&scan->directories[scan->directory_count].dir);
    }
    free(scan);
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
    subghz_saved_capture_scan_t *scan =
        subghz_saved_capture_scan_start(directory, received, protocol_name,
                                        allow_serial_match);
    if (!scan)
        return false;

    subghz_saved_capture_scan_result_t result;
    do
    {
        result = subghz_saved_capture_scan_step(scan, out_path, out_path_size);
    } while (result == SubGhzSavedCaptureScanPending);
    subghz_saved_capture_scan_destroy(scan);
    return result == SubGhzSavedCaptureScanFound;
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
