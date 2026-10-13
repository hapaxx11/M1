/* See COPYING.txt for license details. */

#ifndef RESTAURANT_PAGER_H
#define RESTAURANT_PAGER_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    RESTAURANT_PAGER_TD157 = 0,
    RESTAURANT_PAGER_TD165,
    RESTAURANT_PAGER_TD174,
    RESTAURANT_PAGER_L8R,
    RESTAURANT_PAGER_L8S,
    RESTAURANT_PAGER_FORMAT_COUNT
} restaurant_pager_format_t;

typedef enum {
    RESTAURANT_PAGER_ACTION_UNKNOWN = 0,
    RESTAURANT_PAGER_ACTION_RING,
    RESTAURANT_PAGER_ACTION_MUTE,
    RESTAURANT_PAGER_ACTION_DESYNC,
    RESTAURANT_PAGER_ACTION_ALL_OFF
} restaurant_pager_action_t;

typedef enum {
    RESTAURANT_PAGER_VIEW_SCAN = 0,
    RESTAURANT_PAGER_VIEW_EDIT,
    RESTAURANT_PAGER_VIEW_ACTIONS,
    RESTAURANT_PAGER_VIEW_SETTINGS,
    RESTAURANT_PAGER_VIEW_BATCH_CONFIRM
} restaurant_pager_view_t;

typedef struct {
    uint16_t station;
    uint16_t pager;
    uint8_t action_value;
    restaurant_pager_action_t action;
} restaurant_pager_fields_t;

bool restaurant_pager_capture_updates_history(restaurant_pager_view_t view);
bool restaurant_pager_decode(restaurant_pager_format_t format,
                             uint32_t data,
                             restaurant_pager_fields_t *fields);
bool restaurant_pager_set_pager(restaurant_pager_format_t format,
                                uint32_t *data,
                                uint16_t pager);
uint16_t restaurant_pager_pager_limit(restaurant_pager_format_t format);
bool restaurant_pager_set_action(restaurant_pager_format_t format,
                                 uint32_t *data,
                                 restaurant_pager_action_t action);
restaurant_pager_format_t restaurant_pager_auto_detect(uint32_t data,
                                                       uint16_t max_pager);
const char *restaurant_pager_format_name(restaurant_pager_format_t format);
const char *restaurant_pager_action_name(restaurant_pager_action_t action);

#endif
