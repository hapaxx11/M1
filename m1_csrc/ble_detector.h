/* See COPYING.txt for license details. */

#ifndef BLE_DETECTOR_H
#define BLE_DETECTOR_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

typedef enum {
    BLE_DETECTOR_SKIMMER_MODULE = 0,
    BLE_DETECTOR_FLOCK,
    BLE_DETECTOR_RAY_BAN_META,
} ble_detector_name_kind_t;

typedef enum {
    BLE_DETECTOR_AIRTAG = 0,
    BLE_DETECTOR_RAY_BAN_META_ADV,
} ble_detector_adv_kind_t;

static inline char ble_detector_lower(char c)
{
    return (c >= 'A' && c <= 'Z') ? (char)(c + ('a' - 'A')) : c;
}

static inline bool ble_detector_text_equal(const char *left, const char *right)
{
    if (!left || !right) return false;
    while (*left && *right) {
        if (ble_detector_lower(*left++) != ble_detector_lower(*right++))
            return false;
    }
    return *left == '\0' && *right == '\0';
}

static inline bool ble_detector_text_contains(const char *text, const char *needle)
{
    size_t text_len;
    size_t needle_len;

    if (!text || !needle) return false;
    text_len = strlen(text);
    needle_len = strlen(needle);
    if (needle_len == 0u || text_len < needle_len) return false;

    for (size_t i = 0u; i <= text_len - needle_len; i++) {
        size_t j = 0u;
        while (j < needle_len &&
               ble_detector_lower(text[i + j]) == ble_detector_lower(needle[j]))
            j++;
        if (j == needle_len) return true;
    }
    return false;
}

static inline bool ble_detector_is_word_char(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9');
}

static inline bool ble_detector_text_contains_word(const char *text,
                                                   const char *word)
{
    size_t text_len;
    size_t word_len;

    if (!text || !word) return false;
    text_len = strlen(text);
    word_len = strlen(word);
    if (word_len == 0u || text_len < word_len) return false;

    for (size_t i = 0u; i <= text_len - word_len; i++) {
        size_t j = 0u;
        while (j < word_len &&
               ble_detector_lower(text[i + j]) == ble_detector_lower(word[j]))
            j++;
        if (j == word_len &&
            (i == 0u || !ble_detector_is_word_char(text[i - 1u])) &&
            (i + word_len == text_len ||
             !ble_detector_is_word_char(text[i + word_len])))
            return true;
    }
    return false;
}

static inline bool ble_detector_is_flock_name(const char *name)
{
    if (ble_detector_text_contains_word(name, "flock")) return true;
    if (!name || name[0] != 'F' || name[1] != 'S' || name[2] != '-')
        return false;

    for (size_t i = 3u; i < 9u; i++) {
        char c = name[i];
        if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') ||
              (c >= 'a' && c <= 'f')))
            return false;
    }
    return name[9] == '\0';
}

static inline bool ble_detector_name_matches(const char *name,
                                             ble_detector_name_kind_t kind)
{
    static const char *const skimmer_module_names[] = {
        "HMSoft", "BT05", "MLT-BT05", "AT-09", "CC41-A",
        "JDY-08", "JDY-10",
    };

    if (!name) return false;
    switch (kind) {
    case BLE_DETECTOR_SKIMMER_MODULE:
        for (size_t i = 0u;
             i < sizeof(skimmer_module_names) / sizeof(skimmer_module_names[0]);
             i++) {
            if (ble_detector_text_equal(name, skimmer_module_names[i]))
                return true;
        }
        return false;
    case BLE_DETECTOR_FLOCK:
        return ble_detector_is_flock_name(name);
    case BLE_DETECTOR_RAY_BAN_META:
        return ble_detector_text_contains(name, "ray-ban meta") ||
               ble_detector_text_contains(name, "rayban meta");
    default:
        return false;
    }
}

static inline bool ble_detector_advertisement_matches(const uint8_t *adv,
                                                      size_t adv_len,
                                                      ble_detector_adv_kind_t kind)
{
    size_t offset = 0u;

    if (!adv && adv_len != 0u) return false;
    while (offset < adv_len) {
        uint8_t field_len = adv[offset++];
        uint8_t type;
        const uint8_t *data;
        size_t data_len;

        if (field_len == 0u) continue;
        if ((size_t)field_len > adv_len - offset) return false;
        type = adv[offset];
        data = &adv[offset + 1u];
        data_len = (size_t)field_len - 1u;

        if (kind == BLE_DETECTOR_AIRTAG && type == 0xFFu &&
            data_len >= 3u && data[0] == 0x4Cu && data[1] == 0x00u &&
            data[2] == 0x12u)
            return true;

        if (kind == BLE_DETECTOR_RAY_BAN_META_ADV) {
            if (type == 0x16u && data_len >= 2u &&
                data[0] == 0x5Fu && data[1] == 0xFDu)
                return true;
            if ((type == 0x02u || type == 0x03u) && data_len >= 2u) {
                for (size_t i = 0u; i + 1u < data_len; i += 2u) {
                    if (data[i] == 0x5Fu && data[i + 1u] == 0xFDu)
                        return true;
                }
            }
        }
        offset += field_len;
    }
    return false;
}

#endif
