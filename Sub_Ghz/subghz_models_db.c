/* See COPYING.txt for license details. */

/*
 * subghz_models_db.c
 *
 * ProtoPirate car-models database ("ProtoPirate Car Models Database",
 * format version 2) parser and M1 preset mapping.  Pure logic: no
 * hardware dependencies, no dynamic allocation, no globals.  LF and
 * CRLF line endings are accepted; entries with an unknown preset name
 * or an out-of-range frequency are skipped.
 *
 * M1 Project -- Hapax fork
 */

#include "subghz_models_db.h"

#include <string.h>

#define MODELS_DB_FILETYPE_VALUE "ProtoPirate Car Models Database"
#define MODELS_DB_VERSION_VALUE  "2"
#define MODELS_DB_FREQ_MIN_HZ    300000000u
#define MODELS_DB_FREQ_MAX_HZ    928000000u

/* Long enough that a value truncated to fit can never collide with a
 * known preset name (the longest known name is 34 chars). */
#define MODELS_DB_PRESET_BUF 48u

typedef struct {
    bool     started;
    bool     has_name;
    bool     has_frequency;
    bool     has_preset;
    bool     has_custom_data;
    unsigned index;
    uint32_t frequency_hz;
    char     name[SUBGHZ_MODELS_DB_NAME_MAX];
    char     preset[MODELS_DB_PRESET_BUF];
} models_db_pending_t;

static void trim_value(const char **val, size_t *val_len)
{
    while(*val_len > 0 && (**val == ' ' || **val == '\t')) {
        (*val)++;
        (*val_len)--;
    }
    while(*val_len > 0 &&
          ((*val)[*val_len - 1] == ' ' || (*val)[*val_len - 1] == '\t' ||
           (*val)[*val_len - 1] == '\r')) {
        (*val_len)--;
    }
}

/* Match "<key>: <value>" (no index). */
static bool match_plain_field(const char *line, size_t line_len, const char *key,
                              const char **val_out, size_t *val_len_out)
{
    size_t key_len = strlen(key);
    if(line_len <= key_len || memcmp(line, key, key_len) != 0 ||
       line[key_len] != ':') {
        return false;
    }
    const char *val = line + key_len + 1;
    size_t val_len = line_len - key_len - 1;
    trim_value(&val, &val_len);
    *val_out = val;
    *val_len_out = val_len;
    return true;
}

/* Match "<key><digits>: <value>" (numbered field). */
static bool match_indexed_field(const char *line, size_t line_len, const char *key,
                                unsigned *index_out, const char **val_out,
                                size_t *val_len_out)
{
    size_t key_len = strlen(key);
    if(line_len <= key_len || memcmp(line, key, key_len) != 0) {
        return false;
    }
    size_t p = key_len;
    unsigned index = 0;
    bool have_digit = false;
    while(p < line_len && line[p] >= '0' && line[p] <= '9') {
        index = index * 10u + (unsigned)(line[p] - '0');
        have_digit = true;
        p++;
    }
    if(!have_digit || p >= line_len || line[p] != ':') {
        return false;
    }
    const char *val = line + p + 1;
    size_t val_len = line_len - p - 1;
    trim_value(&val, &val_len);
    *index_out = index;
    *val_out = val;
    *val_len_out = val_len;
    return true;
}

static bool parse_frequency(const char *val, size_t val_len, uint32_t *freq_out)
{
    if(val_len == 0) {
        return false;
    }
    uint32_t freq = 0;
    for(size_t i = 0; i < val_len; i++) {
        if(val[i] < '0' || val[i] > '9') {
            return false;
        }
        uint32_t digit = (uint32_t)(val[i] - '0');
        if(freq > (UINT32_MAX - digit) / 10u) {
            return false;
        }
        freq = freq * 10u + digit;
    }
    *freq_out = freq;
    return true;
}

static void copy_capped(char *dst, size_t dst_size, const char *val, size_t val_len)
{
    size_t n = val_len < dst_size - 1u ? val_len : dst_size - 1u;
    memcpy(dst, val, n);
    dst[n] = '\0';
}

static void flush_entry(const models_db_pending_t *pend, subghz_models_db_t *out)
{
    if(!pend->started || !pend->has_name || !pend->has_frequency ||
       !pend->has_preset) {
        return;
    }
    if(pend->frequency_hz < MODELS_DB_FREQ_MIN_HZ ||
       pend->frequency_hz > MODELS_DB_FREQ_MAX_HZ) {
        return;
    }
    uint8_t mod;
    bool is_custom;
    if(!subghz_models_db_map_preset(pend->preset, pend->has_custom_data, &mod,
                                    &is_custom)) {
        return;
    }
    if(out->count >= SUBGHZ_MODELS_DB_MAX_MODELS) {
        return;
    }
    subghz_model_entry_t *entry = &out->entries[out->count++];
    memcpy(entry->name, pend->name, sizeof(entry->name));
    entry->frequency_hz = pend->frequency_hz;
    entry->mod = mod;
    entry->custom_preset = is_custom;
}

bool subghz_models_db_parse(const char *text, size_t len, subghz_models_db_t *out)
{
    if(text == NULL || out == NULL) {
        return false;
    }
    memset(out, 0, sizeof(*out));

    bool have_filetype = false;
    bool have_version = false;
    models_db_pending_t pend;
    memset(&pend, 0, sizeof(pend));

    size_t pos = 0;
    while(pos < len) {
        size_t eol = pos;
        while(eol < len && text[eol] != '\n') {
            eol++;
        }
        const char *line = text + pos;
        size_t line_len = eol - pos;
        pos = (eol < len) ? eol + 1u : len;

        /* Skip blank / whitespace-only lines (block separators). */
        bool blank = true;
        for(size_t i = 0; i < line_len; i++) {
            if(line[i] != ' ' && line[i] != '\t' && line[i] != '\r') {
                blank = false;
                break;
            }
        }
        if(blank) {
            continue;
        }

        const char *val;
        size_t val_len;
        unsigned index;

        if(match_plain_field(line, line_len, "Filetype", &val, &val_len)) {
            have_filetype = val_len == strlen(MODELS_DB_FILETYPE_VALUE) &&
                            memcmp(val, MODELS_DB_FILETYPE_VALUE, val_len) == 0;
        } else if(match_plain_field(line, line_len, "Version", &val, &val_len)) {
            have_version = val_len == strlen(MODELS_DB_VERSION_VALUE) &&
                           memcmp(val, MODELS_DB_VERSION_VALUE, val_len) == 0;
        } else if(match_indexed_field(line, line_len, "ModelName", &index, &val,
                                      &val_len)) {
            if(pend.started) {
                flush_entry(&pend, out);
                memset(&pend, 0, sizeof(pend));
            }
            pend.started = true;
            pend.index = index;
            pend.has_name = true;
            copy_capped(pend.name, sizeof(pend.name), val, val_len);
        } else if(match_indexed_field(line, line_len, "Frequency", &index, &val,
                                      &val_len)) {
            if(pend.started && index == pend.index &&
               parse_frequency(val, val_len, &pend.frequency_hz)) {
                pend.has_frequency = true;
            }
        } else if(match_indexed_field(line, line_len, "PresetName", &index, &val,
                                      &val_len)) {
            if(pend.started && index == pend.index) {
                pend.has_preset = true;
                copy_capped(pend.preset, sizeof(pend.preset), val, val_len);
            }
        } else if(match_indexed_field(line, line_len, "CustomPresetData", &index,
                                      &val, &val_len)) {
            if(pend.started && index == pend.index && val_len > 0) {
                pend.has_custom_data = true;
            }
        }
        /* Anything else (ModelCount, unknown keys) is ignored. */
    }

    flush_entry(&pend, out);
    return have_filetype && have_version && out->count > 0;
}

bool subghz_models_db_map_preset(const char *preset_name, bool has_custom_data,
                                 uint8_t *mod_out, bool *is_custom_out)
{
    (void)has_custom_data; /* M1 never consumes the CC1101 blob */
    if(preset_name == NULL || mod_out == NULL || is_custom_out == NULL) {
        return false;
    }
    uint8_t mod;
    bool is_custom = false;
    if(strcmp(preset_name, "FuriHalSubGhzPresetOok270Async") == 0) {
        mod = SUBGHZ_MODEL_MOD_AM270;
    } else if(strcmp(preset_name, "FuriHalSubGhzPresetOok650Async") == 0) {
        mod = SUBGHZ_MODEL_MOD_AM650;
    } else if(strcmp(preset_name, "FuriHalSubGhzPreset2FSKDev238Async") == 0) {
        mod = SUBGHZ_MODEL_MOD_FM238;
    } else if(strcmp(preset_name, "FuriHalSubGhzPreset2FSKDev476Async") == 0) {
        mod = SUBGHZ_MODEL_MOD_FM476;
    } else if(strcmp(preset_name, "FuriHalSubGhzPresetCustom") == 0) {
        mod = SUBGHZ_MODEL_MOD_FM476;
        is_custom = true;
    } else {
        return false; /* outputs untouched */
    }
    *mod_out = mod;
    *is_custom_out = is_custom;
    return true;
}

const char *subghz_models_db_mod_label(uint8_t mod)
{
    switch(mod) {
    case SUBGHZ_MODEL_MOD_AM270:
        return "AM270";
    case SUBGHZ_MODEL_MOD_AM650:
        return "AM650";
    case SUBGHZ_MODEL_MOD_FM238:
        return "FM238";
    case SUBGHZ_MODEL_MOD_FM476:
        return "FM476";
    default:
        return "?";
    }
}
