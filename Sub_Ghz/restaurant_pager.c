/* See COPYING.txt for license details. */

#include "restaurant_pager.h"

#include <stddef.h>
#include <string.h>

typedef struct {
    const char *name;
    uint8_t station_bits;
    uint8_t pager_bits;
    uint8_t action_bits;
    uint8_t station_offset;
    uint8_t pager_offset;
    uint8_t action_offset;
    bool reverse_station;
    bool reverse_pager;
    bool reverse_action;
} restaurant_pager_format_spec_t;

static const restaurant_pager_format_spec_t format_specs[] = {
    [RESTAURANT_PAGER_TD157] = {
        "TD157", 10U, 10U, 4U, 14U, 4U, 0U, false, false, false,
    },
    [RESTAURANT_PAGER_TD165] = {
        "TD165", 13U, 10U, 1U, 11U, 1U, 0U, true, true, false,
    },
    [RESTAURANT_PAGER_TD174] = {
        "TD174", 13U, 9U, 2U, 11U, 0U, 9U, true, true, true,
    },
    [RESTAURANT_PAGER_L8R] = {
        "L8R", 13U, 8U, 3U, 11U, 0U, 8U, true, true, true,
    },
    [RESTAURANT_PAGER_L8S] = {
        "L8S", 13U, 8U, 3U, 11U, 0U, 8U, false, false, false,
    },
};

static uint32_t field_mask(uint8_t bits)
{
    return (1UL << bits) - 1UL;
}

static uint32_t reverse_bits(uint32_t value, uint8_t bits)
{
    uint32_t reversed = 0U;
    for (uint8_t bit = 0U; bit < bits; ++bit)
    {
        reversed = (reversed << 1U) | (value & 1U);
        value >>= 1U;
    }
    return reversed;
}

static const restaurant_pager_format_spec_t *get_spec(
    restaurant_pager_format_t format)
{
    if ((unsigned)format >= RESTAURANT_PAGER_FORMAT_COUNT)
        return NULL;
    return &format_specs[format];
}

static uint32_t get_field(uint32_t data, uint8_t bits, uint8_t offset,
                          bool reversed)
{
    uint32_t value = (data >> offset) & field_mask(bits);
    return reversed ? reverse_bits(value, bits) : value;
}

static uint32_t set_field(uint32_t data, uint32_t value, uint8_t bits,
                          uint8_t offset, bool reversed)
{
    uint32_t mask = field_mask(bits) << offset;
    uint32_t encoded = reversed ? reverse_bits(value, bits) : value;
    return (data & ~mask) | ((encoded << offset) & mask);
}

bool restaurant_pager_capture_updates_history(restaurant_pager_view_t view)
{
    return view == RESTAURANT_PAGER_VIEW_SCAN;
}

bool restaurant_pager_decode(restaurant_pager_format_t format,
                             uint32_t data,
                             restaurant_pager_fields_t *fields)
{
    const restaurant_pager_format_spec_t *spec = get_spec(format);
    if (spec == NULL || fields == NULL)
        return false;

    fields->station = (uint16_t)get_field(data, spec->station_bits,
                                           spec->station_offset,
                                           spec->reverse_station);
    fields->pager = (uint16_t)get_field(data, spec->pager_bits,
                                         spec->pager_offset,
                                         spec->reverse_pager);
    fields->action_value = (uint8_t)get_field(data, spec->action_bits,
                                               spec->action_offset,
                                               spec->reverse_action);
    fields->action = RESTAURANT_PAGER_ACTION_UNKNOWN;

    switch (format)
    {
        case RESTAURANT_PAGER_TD157:
            if (fields->action_value == 2U)
                fields->action = RESTAURANT_PAGER_ACTION_RING;
            else if (fields->action_value == 15U && fields->pager == 999U)
                fields->action = RESTAURANT_PAGER_ACTION_ALL_OFF;
            break;

        case RESTAURANT_PAGER_TD165:
            if (fields->action_value == 0U)
                fields->action = (fields->pager == 1005U) ?
                    RESTAURANT_PAGER_ACTION_ALL_OFF :
                    RESTAURANT_PAGER_ACTION_RING;
            else if (fields->action_value == 1U)
                fields->action = RESTAURANT_PAGER_ACTION_MUTE;
            break;

        case RESTAURANT_PAGER_TD174:
            if (fields->action_value == 0U)
                fields->action = RESTAURANT_PAGER_ACTION_RING;
            else if (fields->action_value == 3U && fields->pager == 237U)
                fields->action = RESTAURANT_PAGER_ACTION_DESYNC;
            break;

        case RESTAURANT_PAGER_L8R:
            if (fields->action_value == 0U)
                fields->action = RESTAURANT_PAGER_ACTION_RING;
            break;

        case RESTAURANT_PAGER_L8S:
        case RESTAURANT_PAGER_FORMAT_COUNT:
            break;
    }

    return true;
}

bool restaurant_pager_set_pager(restaurant_pager_format_t format,
                                uint32_t *data,
                                uint16_t pager)
{
    const restaurant_pager_format_spec_t *spec = get_spec(format);
    if (spec == NULL || data == NULL || pager > field_mask(spec->pager_bits))
        return false;

    *data = set_field(*data, pager, spec->pager_bits, spec->pager_offset,
                      spec->reverse_pager);
    return true;
}

uint16_t restaurant_pager_pager_limit(restaurant_pager_format_t format)
{
    const restaurant_pager_format_spec_t *spec = get_spec(format);
    return (spec != NULL) ? (uint16_t)field_mask(spec->pager_bits) : 0U;
}

bool restaurant_pager_set_action(restaurant_pager_format_t format,
                                 uint32_t *data,
                                 restaurant_pager_action_t action)
{
    const restaurant_pager_format_spec_t *spec = get_spec(format);
    if (spec == NULL || data == NULL)
        return false;

    uint8_t action_value;
    uint16_t special_pager = 0U;
    bool set_special_pager = false;
    switch (format)
    {
        case RESTAURANT_PAGER_TD157:
            if (action == RESTAURANT_PAGER_ACTION_RING)
                action_value = 2U;
            else if (action == RESTAURANT_PAGER_ACTION_ALL_OFF)
            {
                action_value = 15U;
                special_pager = 999U;
                set_special_pager = true;
            }
            else
                return false;
            break;

        case RESTAURANT_PAGER_TD165:
            if (action == RESTAURANT_PAGER_ACTION_RING)
            {
                if (get_field(*data, spec->pager_bits, spec->pager_offset,
                              spec->reverse_pager) == 1005U)
                    return false;
                action_value = 0U;
            }
            else if (action == RESTAURANT_PAGER_ACTION_MUTE)
                action_value = 1U;
            else if (action == RESTAURANT_PAGER_ACTION_ALL_OFF)
            {
                action_value = 0U;
                special_pager = 1005U;
                set_special_pager = true;
            }
            else
                return false;
            break;

        case RESTAURANT_PAGER_TD174:
            if (action == RESTAURANT_PAGER_ACTION_RING)
                action_value = 0U;
            else if (action == RESTAURANT_PAGER_ACTION_DESYNC)
            {
                action_value = 3U;
                special_pager = 237U;
                set_special_pager = true;
            }
            else
                return false;
            break;

        case RESTAURANT_PAGER_L8R:
            if (action != RESTAURANT_PAGER_ACTION_RING)
                return false;
            action_value = 0U;
            break;

        case RESTAURANT_PAGER_L8S:
        case RESTAURANT_PAGER_FORMAT_COUNT:
            return false;
    }

    if (set_special_pager)
    {
        if (special_pager > field_mask(spec->pager_bits))
            return false;
        *data = set_field(*data, special_pager, spec->pager_bits,
                          spec->pager_offset, spec->reverse_pager);
    }
    *data = set_field(*data, action_value, spec->action_bits,
                      spec->action_offset, spec->reverse_action);
    return true;
}

restaurant_pager_format_t restaurant_pager_auto_detect(uint32_t data,
                                                       uint16_t max_pager)
{
    restaurant_pager_fields_t fields;
    restaurant_pager_format_t fallback = RESTAURANT_PAGER_TD157;
    bool found_fallback = false;
    for (restaurant_pager_format_t format = RESTAURANT_PAGER_TD157;
         format < RESTAURANT_PAGER_FORMAT_COUNT;
         format = (restaurant_pager_format_t)(format + 1))
    {
        if (restaurant_pager_decode(format, data, &fields) &&
            fields.pager <= max_pager)
        {
            if (!found_fallback)
            {
                fallback = format;
                found_fallback = true;
            }
            if (fields.action != RESTAURANT_PAGER_ACTION_UNKNOWN)
                return format;
        }
    }
    return found_fallback ? fallback : RESTAURANT_PAGER_TD157;
}

const char *restaurant_pager_format_name(restaurant_pager_format_t format)
{
    const restaurant_pager_format_spec_t *spec = get_spec(format);
    return (spec != NULL) ? spec->name : "Unknown";
}

const char *restaurant_pager_action_name(restaurant_pager_action_t action)
{
    switch (action)
    {
        case RESTAURANT_PAGER_ACTION_RING: return "RING";
        case RESTAURANT_PAGER_ACTION_MUTE: return "MUTE";
        case RESTAURANT_PAGER_ACTION_DESYNC: return "DESYNC";
        case RESTAURANT_PAGER_ACTION_ALL_OFF: return "ALL OFF";
        case RESTAURANT_PAGER_ACTION_UNKNOWN:
        default: return "UNKNOWN";
    }
}
