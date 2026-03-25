/**
 * @file furi_hal_power.h
 * Power HAL API
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void furi_hal_power_reset(void);
uint16_t furi_hal_power_insomnia_level(void);
void furi_hal_power_insomnia_enter(void);
void furi_hal_power_insomnia_exit(void);
bool furi_hal_power_sleep_available(void);
void furi_hal_power_init(void);
uint32_t furi_hal_power_deep_sleep(uint32_t expected_idle_ticks);

#ifdef __cplusplus
}
#endif
