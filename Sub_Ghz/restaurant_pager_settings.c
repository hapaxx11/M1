/* See COPYING.txt for license details. */

#include "restaurant_pager_settings.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

static bool parse_value(const char **text, uint32_t *value)
{
    if (text == NULL || *text == NULL || value == NULL)
        return false;

    const char *start = *text;
    while (isspace((unsigned char)*start))
        ++start;
    if (*start < '0' || *start > '9')
        return false;

    errno = 0;
    char *end = NULL;
    unsigned long parsed = strtoul(start, &end, 10);
    if (errno != 0 || end == start || parsed > UINT32_MAX)
        return false;

    *text = end;
    *value = (uint32_t)parsed;
    return true;
}

bool restaurant_pager_settings_parse(const char *text,
                                     restaurant_pager_settings_t *settings)
{
    if (text == NULL || settings == NULL)
        return false;

    uint32_t max_pager;
    uint32_t repeat_count;
    if (!parse_value(&text, &max_pager) ||
        !isspace((unsigned char)*text) ||
        !parse_value(&text, &repeat_count))
        return false;

    while (isspace((unsigned char)*text))
        ++text;
    if (*text != '\0' || max_pager > 1023U ||
        repeat_count < 1U || repeat_count > 50U)
        return false;

    settings->max_pager = (uint16_t)max_pager;
    settings->tx_repeat_count = (uint8_t)repeat_count;
    return true;
}

bool restaurant_pager_settings_format(
    const restaurant_pager_settings_t *settings,
    char *buffer,
    size_t buffer_size)
{
    if (settings == NULL || buffer == NULL ||
        settings->max_pager > 1023U ||
        settings->tx_repeat_count < 1U ||
        settings->tx_repeat_count > 50U)
        return false;

    int length = snprintf(buffer, buffer_size, "%u %u\n",
                          settings->max_pager, settings->tx_repeat_count);
    return length >= 0 && (size_t)length < buffer_size;
}
