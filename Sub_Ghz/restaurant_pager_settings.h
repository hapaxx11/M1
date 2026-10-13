/* See COPYING.txt for license details. */

#ifndef RESTAURANT_PAGER_SETTINGS_H
#define RESTAURANT_PAGER_SETTINGS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint16_t max_pager;
    uint8_t tx_repeat_count;
} restaurant_pager_settings_t;

bool restaurant_pager_settings_parse(const char *text,
                                     restaurant_pager_settings_t *settings);
bool restaurant_pager_settings_format(
    const restaurant_pager_settings_t *settings,
    char *buffer,
    size_t buffer_size);

#endif
