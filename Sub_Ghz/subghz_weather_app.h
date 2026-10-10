/* See COPYING.txt for license details. */

#ifndef SUBGHZ_WEATHER_APP_H
#define SUBGHZ_WEATHER_APP_H

#include <stdbool.h>
#include <stdint.h>

#include "subghz_weather_history.h"
#include "subghz_weather_scan.h"

typedef struct {
    SubGhzWeatherHistory history;
    SubGhzWeatherScan scan;
    uint32_t saved_custom_frequency_hz;
    uint32_t last_age_tick_ms;
    uint8_t saved_frequency_index;
    uint8_t saved_modulation_index;
    uint8_t selected_sensor;
    uint8_t first_visible_sensor;
    bool detail_view;
    bool radio_active;
} subghz_weather_app_state_t;

#endif
