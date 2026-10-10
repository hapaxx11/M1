/* See COPYING.txt for license details. */

#include "tpms_receiver.h"

#include <string.h>

static uint8_t tpms_schrader_gg4_crc8(uint64_t data)
{
    uint8_t crc = 0U;
    for (uint8_t byte_index = 1U; byte_index <= 6U; ++byte_index)
    {
        uint8_t byte = (uint8_t)(data >> ((7U - byte_index) * 8U));
        crc ^= byte;
        for (uint8_t bit = 0U; bit < 8U; ++bit)
            crc = (crc & 0x80U) != 0U ?
                (uint8_t)((crc << 1) ^ 0x07U) : (uint8_t)(crc << 1);
    }
    return crc;
}

bool tpms_schrader_gg4_parse(uint64_t data, uint16_t bit_length,
                             tpms_telemetry_t *telemetry)
{
    if (telemetry == NULL)
        return false;

    memset(telemetry, 0, sizeof(*telemetry));
    if (bit_length != 64U ||
        tpms_schrader_gg4_crc8(data) != (uint8_t)data)
        return false;

    uint8_t raw_pressure = (uint8_t)(data >> 16);
    uint8_t raw_temperature = (uint8_t)(data >> 8);
    telemetry->serial = (uint32_t)(data >> 24);
    telemetry->pressure_hundredths_bar =
        (uint16_t)(((uint32_t)raw_pressure * 69U + 2U) / 4U);
    telemetry->temperature_c = (int16_t)raw_temperature - 50;
    telemetry->valid = true;
    return true;
}

void tpms_history_reset(tpms_history_t *history)
{
    if (history != NULL)
        memset(history, 0, sizeof(*history));
}

static bool tpms_sensor_matches(const tpms_sensor_t *entry,
                                const tpms_sensor_t *sensor)
{
    return entry->protocol == sensor->protocol &&
        ((sensor->serial != 0U && entry->serial == sensor->serial) ||
         (sensor->serial == 0U && entry->serial == 0U &&
          entry->data == sensor->data));
}

uint8_t tpms_history_selection_index(const tpms_history_t *history,
                                     const tpms_sensor_t *sensor)
{
    if (history == NULL || sensor == NULL)
        return 0U;

    for (uint8_t i = 0; i < history->count; ++i)
    {
        if (tpms_sensor_matches(&history->entries[i], sensor))
            return i;
    }
    return 0U;
}

int tpms_history_add(tpms_history_t *history, const tpms_sensor_t *sensor)
{
    if (history == NULL || sensor == NULL)
        return -1;

    for (uint8_t i = 0; i < history->count; ++i)
    {
        if (tpms_sensor_matches(&history->entries[i], sensor))
        {
            uint16_t receptions = history->entries[i].receptions;
            tpms_sensor_t refreshed = *sensor;
            refreshed.receptions = (receptions == UINT16_MAX) ?
                UINT16_MAX : (uint16_t)(receptions + 1U);
            if (i > 0U)
                memmove(&history->entries[1], &history->entries[0],
                        i * sizeof(history->entries[0]));
            history->entries[0] = refreshed;
            return 0;
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
