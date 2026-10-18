/* See COPYING.txt for license details. */

#include "subghz_raw_pairs_writer.h"

#include <stdio.h>
#include <string.h>

static bool flush_line(char *line, size_t capacity, size_t *position,
                       subghz_raw_pairs_write_line_fn write_line, void *context)
{
    if (*position <= 5u || *position + 2u >= capacity)
        return false;
    line[(*position)++] = '\r';
    line[(*position)++] = '\n';
    line[*position] = '\0';
    if (!write_line(context, line))
        return false;
    memcpy(line, "Data:", 6u);
    *position = 5u;
    return true;
}

bool subghz_raw_pairs_write_data(const SubGhzRawPair *pairs, uint32_t pair_count,
                                uint32_t pairs_per_rep, char *line,
                                size_t line_capacity,
                                subghz_raw_pairs_write_line_fn write_line,
                                void *context)
{
    if (pairs == NULL || pair_count == 0 || line == NULL ||
        line_capacity < 16u || write_line == NULL)
        return false;

    if (pairs_per_rep == 0)
        pairs_per_rep = pair_count;

    memcpy(line, "Data:", 6u);
    size_t position = 5u;
    for (uint32_t i = 0; i < pair_count; i++)
    {
        char pair_text[32];
        const int pair_length = snprintf(pair_text, sizeof(pair_text), " %lu %lu",
                                         (unsigned long)pairs[i].high_us,
                                         (unsigned long)pairs[i].low_us);
        if (pair_length <= 0 || (size_t)pair_length >= sizeof(pair_text))
            return false;

        if (position + (size_t)pair_length + 3u > line_capacity &&
            !flush_line(line, line_capacity, &position, write_line, context))
            return false;
        if (position + (size_t)pair_length + 3u > line_capacity)
            return false;

        memcpy(&line[position], pair_text, (size_t)pair_length);
        position += (size_t)pair_length;
        line[position] = '\0';

        if (((i + 1u) % pairs_per_rep) == 0u || i + 1u == pair_count)
        {
            if (!flush_line(line, line_capacity, &position, write_line, context))
                return false;
        }
    }
    return true;
}
