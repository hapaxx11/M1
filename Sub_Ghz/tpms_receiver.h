/* See COPYING.txt for license details. */

#ifndef TPMS_RECEIVER_H
#define TPMS_RECEIVER_H

#include <stdbool.h>
#include <stdint.h>

#define TPMS_HISTORY_CAPACITY 8U

typedef struct {
    bool valid;
    uint32_t serial;
    uint16_t pressure_hundredths_bar;
    int16_t temperature_c;
} tpms_telemetry_t;

typedef struct {
    uint16_t protocol;
    uint16_t bit_length;
    uint32_t serial;
    uint64_t data;
    tpms_telemetry_t telemetry;
    uint32_t frequency_hz;
    uint32_t last_seen_ms;
    uint16_t receptions;
    int16_t rssi;
} tpms_sensor_t;

typedef struct {
    tpms_sensor_t entries[TPMS_HISTORY_CAPACITY];
    uint8_t count;
} tpms_history_t;

typedef struct {
    tpms_history_t history;
    uint32_t saved_custom_frequency_hz;
    uint8_t saved_frequency_index;
    uint8_t saved_modulation_index;
    uint8_t selected_sensor;
    bool detail_view;
    bool radio_active;
} tpms_receiver_app_state_t;

void tpms_history_reset(tpms_history_t *history);
int tpms_history_add(tpms_history_t *history, const tpms_sensor_t *sensor);
uint8_t tpms_history_selection_index(const tpms_history_t *history,
                                    const tpms_sensor_t *sensor);
const tpms_sensor_t *tpms_history_get(const tpms_history_t *history,
                                      uint8_t newest_index);
uint8_t tpms_sensor_age_min(const tpms_sensor_t *sensor, uint32_t now_ms);
bool tpms_schrader_gg4_parse(uint64_t data, uint16_t bit_length,
                             tpms_telemetry_t *telemetry);

#endif
