/* See COPYING.txt for license details. */

/**
 * @file   m1_esl_ir.h
 * @brief  ESL infrared transmitter API.
 *
 * Generates the PP4 pulse-position carrier signal required by Pricer ESL
 * tags using TIM1_CH4N (PC5) and DWT cycle-counter-based symbol timing.
 *
 * Hardware:
 *   Carrier:    ~1.25 MHz  (TIM1 APB2 clock 75 MHz / ARR+1=60)
 *   Modulation: PP4 — 4 symbols per byte, 2 bits per symbol
 *   Output pin: PC5 via TIM1_CH4N (AF1)
 *
 * This driver is mutually exclusive with the standard M1 consumer-IR encoder.
 * Always call m1_esl_ir_init() before transmitting and m1_esl_ir_deinit()
 * when done to release the shared TIM1 resource.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/**
 * @brief  Initialise the ESL IR transmitter.
 *
 * Configures TIM1 for the 1.25 MHz ESL carrier on PC5 and enables the
 * DWT cycle counter for symbol timing.  Idempotent — safe to call multiple
 * times, though m1_esl_ir_deinit() must precede each re-init if TIM1 was
 * reconfigured by other code in between.
 */
void m1_esl_ir_init(void);

/**
 * @brief  Deinitialise the ESL IR transmitter.
 *
 * Turns off the carrier, stops TIM1, disables its clock, and returns PC5
 * to a floating analog input so the pin is available for other use.
 */
void m1_esl_ir_deinit(void);

/**
 * @brief  Transmit a frame using PP4 pulse-position modulation.
 *
 * Encodes each byte as four 2-bit PP4 symbols and transmits them with the
 * 1.25 MHz carrier.  Blocks the calling FreeRTOS task for the entire
 * transmission duration.  Inter-repetition gaps use vTaskDelay so the
 * scheduler can run other tasks during the pause.
 *
 * @param  data      Frame bytes to transmit.
 * @param  len       Number of bytes (1–255).
 * @param  repeats   Number of extra repetitions (0 = send exactly once).
 * @param  delay_ms  Milliseconds between repetitions (0 for no gap).
 * @retval true   Transmission complete.
 * @retval false  Not initialised, or bad arguments.
 */
bool m1_esl_ir_transmit_pp4(const uint8_t *data, size_t len,
                             uint8_t repeats, uint16_t delay_ms);
