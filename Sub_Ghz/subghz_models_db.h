/* See COPYING.txt for license details. */

/*
 * subghz_models_db.h
 *
 * ProtoPirate car-models database ("ProtoPirate Car Models Database",
 * format version 2) — parser and M1 preset mapping.  Pure logic, no
 * hardware dependencies; the scene reads the file text from SD and calls
 * subghz_models_db_parse().
 *
 * The database maps a display name to a receiver frequency and a Flipper
 * preset name.  M1 translates the preset NAME to its own SI4463 modulation
 * presets; a FuriHalSubGhzPresetCustom entry carries a CC1101 register blob
 * that M1 never consumes — the published Honda custom profile is an FM
 * configuration and maps to M1's FM476 preset.
 *
 * M1 Project -- Hapax fork
 */

#ifndef SUBGHZ_MODELS_DB_H
#define SUBGHZ_MODELS_DB_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SUBGHZ_MODELS_DB_MAX_MODELS  32u
#define SUBGHZ_MODELS_DB_NAME_MAX    40u  /* bytes including the NUL */

/* M1 modulation choices — the values match the subghz_mod_presets[] order in
 * m1_csrc/m1_sub_ghz.c (0=AM270, 1=AM650, 2=FM238, 3=FM476) so a value can be
 * passed straight to subghz_apply_config_ext()/subghz_set_mod_idx_ext(). */
typedef enum {
    SUBGHZ_MODEL_MOD_AM270 = 0,
    SUBGHZ_MODEL_MOD_AM650 = 1,
    SUBGHZ_MODEL_MOD_FM238 = 2,
    SUBGHZ_MODEL_MOD_FM476 = 3,
} subghz_model_mod_t;

typedef struct {
    char     name[SUBGHZ_MODELS_DB_NAME_MAX];
    uint32_t frequency_hz;
    uint8_t  mod;           /* subghz_model_mod_t */
    bool     custom_preset; /* entry used FuriHalSubGhzPresetCustom; the
                               CC1101 blob is acknowledged, not consumed */
} subghz_model_entry_t;

typedef struct {
    subghz_model_entry_t entries[SUBGHZ_MODELS_DB_MAX_MODELS];
    uint8_t count;
} subghz_models_db_t;

static inline void subghz_models_disable_hopping(bool *hopping)
{
    if (hopping)
        *hopping = false;
}

/**
 * Parse a v2 models-database text (LF or CRLF).  Entries with an unknown
 * preset name or an out-of-range frequency are skipped; parsing succeeds
 * when at least one valid entry remains.
 *
 * @return true when out->count > 0, false on a missing/bad header or no
 *         usable entries.
 */
bool subghz_models_db_parse(const char *text, size_t len, subghz_models_db_t *out);

/**
 * Map a Flipper/ProtoPirate preset name to an M1 modulation index.
 * FuriHalSubGhzPresetCustom maps to FM476 with *is_custom_out = true.
 *
 * @return false for an unknown preset name (outputs untouched).
 */
bool subghz_models_db_map_preset(const char *preset_name, bool has_custom_data,
                                 uint8_t *mod_out, bool *is_custom_out);

/** Display label for a modulation index ("AM270"/"AM650"/"FM238"/"FM476"). */
const char *subghz_models_db_mod_label(uint8_t mod);

#endif /* SUBGHZ_MODELS_DB_H */
