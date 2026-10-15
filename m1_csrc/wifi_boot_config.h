/* See COPYING.txt for license details. */

#ifndef WIFI_BOOT_CONFIG_H
#define WIFI_BOOT_CONFIG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define WIFI_BOOT_PRIMARY_NONE UINT8_MAX

typedef struct
{
    uint8_t primary_index;
    bool auto_connect;
} wifi_boot_config_t;

static inline void wifi_boot_config_reset(wifi_boot_config_t *config)
{
    if (config == NULL)
        return;
    config->primary_index = WIFI_BOOT_PRIMARY_NONE;
    config->auto_connect = false;
}

static inline bool wifi_boot_config_set_primary(wifi_boot_config_t *config,
                                                 uint8_t index,
                                                 uint8_t saved_count)
{
    if (config == NULL || index >= saved_count)
        return false;
    config->primary_index = index;
    config->auto_connect = true;
    return true;
}

static inline void wifi_boot_config_set_enabled(wifi_boot_config_t *config,
                                                bool enabled)
{
    if (config != NULL)
        config->auto_connect = enabled &&
                               config->primary_index != WIFI_BOOT_PRIMARY_NONE;
}

static inline bool wifi_boot_config_target(const wifi_boot_config_t *config,
                                           uint8_t saved_count,
                                           uint8_t *out_index)
{
    if (config == NULL || out_index == NULL || !config->auto_connect ||
        config->primary_index >= saved_count)
        return false;
    *out_index = config->primary_index;
    return true;
}

static inline void wifi_boot_config_on_delete(wifi_boot_config_t *config,
                                              uint8_t deleted_index)
{
    if (config == NULL || config->primary_index == WIFI_BOOT_PRIMARY_NONE)
        return;
    if (config->primary_index == deleted_index)
    {
        wifi_boot_config_reset(config);
    }
    else if (config->primary_index > deleted_index)
    {
        config->primary_index--;
    }
}

#endif /* WIFI_BOOT_CONFIG_H */
