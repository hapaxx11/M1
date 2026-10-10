/* See COPYING.txt for license details. */

#include "tpms_receiver.h"

#include <string.h>

void tpms_history_reset(tpms_history_t *history)
{
    if (history != NULL)
        memset(history, 0, sizeof(*history));
}

int tpms_history_add(tpms_history_t *history, const tpms_sensor_t *sensor)
{
    if (history == NULL || sensor == NULL)
        return -1;

    for (uint8_t i = 0; i < history->count; ++i)
    {
        tpms_sensor_t *entry = &history->entries[i];
        bool same_sensor = entry->protocol == sensor->protocol &&
            ((sensor->serial != 0U && entry->serial == sensor->serial) ||
             (sensor->serial == 0U && entry->serial == 0U &&
              entry->data == sensor->data));
        if (same_sensor)
        {
            uint16_t receptions = entry->receptions;
            *entry = *sensor;
            entry->receptions = (receptions == UINT16_MAX) ?
                UINT16_MAX : (uint16_t)(receptions + 1U);
            return i;
        }
    }

    if (history->count == TPMS_HISTORY_CAPACITY)
    {
        memmove(&history->entries[1], &history->entries[0],
                (TPMS_HISTORY_CAPACITY - 1U) * sizeof(history->entries[0]));
    }
    else
    {
        memmove(&history->entries[1], &history->entries[0],
                history->count * sizeof(history->entries[0]));
        history->count++;
    }
    history->entries[0] = *sensor;
    if (history->entries[0].receptions == 0U)
        history->entries[0].receptions = 1U;
    return 0;
}

const tpms_sensor_t *tpms_history_get(const tpms_history_t *history,
                                      uint8_t newest_index)
{
    if (history == NULL || newest_index >= history->count)
        return NULL;
    return &history->entries[newest_index];
}

uint8_t tpms_sensor_age_min(const tpms_sensor_t *sensor, uint32_t now_ms)
{
    if (sensor == NULL)
        return 0U;
    uint32_t age = (uint32_t)(now_ms - sensor->last_seen_ms) / 60000U;
    return (age > 99U) ? 99U : (uint8_t)age;
}
