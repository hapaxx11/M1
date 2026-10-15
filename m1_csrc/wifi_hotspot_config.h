/* See COPYING.txt for license details. */

#ifndef WIFI_HOTSPOT_CONFIG_H
#define WIFI_HOTSPOT_CONFIG_H

#include <stdbool.h>
#include <stddef.h>

#include "esp32_feature_map.h"

#define WIFI_HOTSPOT_SSID_MAX_LEN      32u
#define WIFI_HOTSPOT_PASSWORD_MAX_LEN  63u
#define WIFI_HOTSPOT_CONFIG_TEXT_MAX  111u

typedef enum {
    WIFI_HOTSPOT_PATH_UNSUPPORTED = 0,
    WIFI_HOTSPOT_PATH_AT,
    WIFI_HOTSPOT_PATH_RPC
} wifi_hotspot_path_t;

wifi_hotspot_path_t wifi_hotspot_select_path(esp32_transport_t transport);

bool wifi_hotspot_config_validate(const char *ssid, const char *password);

bool wifi_hotspot_config_parse(const char *text, size_t text_len,
                               char *ssid, size_t ssid_capacity,
                               char *password, size_t password_capacity);

size_t wifi_hotspot_config_format(char *out, size_t capacity,
                                  const char *ssid, const char *password);

#endif /* WIFI_HOTSPOT_CONFIG_H */
